/**
 * Copyright Martin Raiber. All Rights Reserved.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#include "KeyStore.h"
#include "KeyStoreDetail.h"

KeyStore::KeyStore(SingleFileStorage& sfs, const std::string& bucket)
    : sfs(sfs), bucketName(bucket)
{
}

KeyStore::~KeyStore()
{
    stop();
}

void KeyStore::start()
{
    // Large collections exist if there are collection counters
    if (const auto bucketInfo = buckets::getBucketInfo(bucketName))
        anyLargeCollections = hasEntries(buckets::getCountersBucket(bucketInfo->id), reservedKeyPrefix + "c");

    expiryThread = std::thread([this]()
                               { expiryThreadFunc(); });
}

void KeyStore::stop()
{
    if (expiryThread.joinable())
    {
        {
            std::scoped_lock lock(expiryThreadMutex);
            expiryThreadStop = true;
        }
        expiryThreadCond.notify_all();
        expiryThread.join();
    }
}

void KeyStore::notifyKey(const std::string& key)
{
    if (keyChangedCallback)
        keyChangedCallback(key);
}

size_t KeyStore::keyLockIdx(const std::string& key) const
{
    return std::hash<std::string>{}(key) % keyLocks.size();
}

std::shared_mutex& KeyStore::keyLock(const std::string& key)
{
    return keyLocks[keyLockIdx(key)];
}

void KeyStore::touchKeyLock(const size_t idx)
{
    keyEpochs[idx].fetch_add(1, std::memory_order_relaxed);
}

std::optional<std::string> KeyStore::getBucketCtx(BucketCtx& ctx)
{
    const auto bucketInfo = buckets::getBucketInfo(bucketName);
    if (!bucketInfo)
        return resp::error(fmt::format("ERR bucket '{}' does not exist", bucketName));

    if (bucketInfo->versioning != buckets::VersioningState::Enabled)
        return resp::error(fmt::format("ERR versioning has to be enabled on bucket '{}'", bucketName));

    if (bucketInfo->objectLockEnabled)
        return resp::error(fmt::format("ERR bucket '{}' has object lock enabled, which is not supported", bucketName));

    ctx.id = bucketInfo->id;
    return std::nullopt;
}

std::optional<std::string> KeyStore::checkKey(const std::string_view key)
{
    if (key.empty())
        return resp::error("ERR empty keys are not supported");
    if (key.size() > maxKeyLen)
        return resp::error(fmt::format("ERR key too long (max {} bytes)", maxKeyLen));
    if (key.starts_with(reservedKeyPrefix))
        return resp::error("ERR keys starting with byte 0xff are reserved");
    return std::nullopt;
}

bool KeyStore::listEntries(const int64_t bucketId, const std::string& key, std::vector<Entry>& entries, const size_t maxElements)
{
    syncListing();

    SingleFileStorage::ListIterData iterData;
    SCOPE_EXIT { sfs.list_iter_stop(iterData); };

    const auto start = make_key(key, bucketId, std::numeric_limits<int64_t>::max());
    if (!sfs.list_iter_start(start, start, [bucketId, key](const std::string& iterKey)
                             {
                                 const auto keyInfo = extractKeyInfoView(iterKey);
                                 return keyInfo.bucketId == bucketId && keyInfo.key == key; }, iterData))
    {
        return false;
    }

    size_t elements = 0;
    while (elements < maxElements)
    {
        std::string keyBin, md5sum;
        int64_t offset, size, lastModified;
        std::vector<SingleFileStorage::SPunchItem> extraExts;
        if (!sfs.iter_curr_val(keyBin, offset, size, extraExts, lastModified, md5sum, iterData.curr))
            break;

        const auto keyInfo = extractKeyInfoView(keyBin);
        if (keyInfo.bucketId != bucketId || keyInfo.key != key)
            break;

        const auto tombstone = isTombstone(md5sum);
        const auto [type, expiry] = keyInfoFromMetadata(md5sum);
        entries.push_back(Entry{ .version = keyInfo.version, .tombstone = tombstone, .isList = type == KeyType::List, .type = type, .expiry = expiry });
        if (!tombstone)
            ++elements;

        if (!sfs.list_iter_next(iterData))
            return false;
    }

    return true;
}

std::optional<KeyStore::Entry> KeyStore::findEnd(const int64_t bucketId, const std::string& key, const bool newest, int& err)
{
    const auto fn = make_key(key, bucketId, newest ? std::numeric_limits<int64_t>::max() : std::numeric_limits<int64_t>::min());
    const auto res = sfs.read_prepare(fn, (newest ? SingleFileStorage::ReadNewest : SingleFileStorage::ReadOldest) | SingleFileStorage::ReadSkipAddReading);
    if (res.err == ENOENT)
        return std::nullopt;
    if (res.err != 0)
    {
        err = res.err;
        return std::nullopt;
    }

    const auto [type, expiry] = keyInfoFromMetadata(res.md5sum);
    return Entry{ .version = extractKeyInfoView(res.key).version, .tombstone = isTombstone(res.md5sum), .isList = type == KeyType::List, .type = type, .expiry = expiry };
}

std::optional<KeyStore::Entry> KeyStore::findElement(const int64_t bucketId, const std::string& key, const bool newest, int& err)
{
    const auto entry = findEnd(bucketId, key, newest, err);
    if (!entry || !entry->tombstone)
        return entry;

    // Skip delete markers (rare)
    std::vector<Entry> entries;
    if (!listEntries(bucketId, key, entries, newest ? 1 : std::numeric_limits<size_t>::max()))
    {
        err = EIO;
        return std::nullopt;
    }

    if (newest)
    {
        for (const auto& curr : entries)
        {
            if (!curr.tombstone)
                return curr;
        }
    }
    else
    {
        for (auto it = entries.rbegin(); it != entries.rend(); ++it)
        {
            if (!it->tombstone)
                return *it;
        }
    }

    return std::nullopt;
}

KeyStore::ReadRes KeyStore::readFromReader(std::shared_ptr<ObjectReader> reader, const std::string& key, const ReadMode mode)
{
    ReadRes ret{ .status = ReadRes::Status::Ok, .size = reader->FileSize() };
    if (mode == ReadMode::Size)
        return ret;

    if (ret.size > FLAGS_redis_max_value_size)
    {
        return ReadRes{ .status = ReadRes::Status::Error,
                        .value = fmt::format("value of key '{}' is too large ({} bytes, max {}). Read it via S3", key, ret.size, FLAGS_redis_max_value_size) };
    }

    const auto size = static_cast<size_t>(ret.size);
    if (mode == ReadMode::Bulk)
    {
        if (ret.size >= streamThreshold)
        {
            // Read while sending the reply. The reader keeps the data readable even if the version is deleted
            ret.reader = std::move(reader);
            return ret;
        }

        const auto header = fmt::format("${}\r\n", size);
        ret.value.resize(header.size() + size + 2);
        std::memcpy(ret.value.data(), header.data(), header.size());
        if (size > 0)
            reader->Read(ret.value.data() + header.size(), ret.size, 0);
        ret.value[header.size() + size] = '\r';
        ret.value[header.size() + size + 1] = '\n';
    }
    else if (size > 0)
    {
        ret.value.resize(size);
        reader->Read(ret.value.data(), ret.size, 0);
    }
    return ret;
}

KeyStore::ReadRes KeyStore::readValue(const std::string& key, const int64_t version, const ReadMode mode)
{
    try
    {
        return readFromReader(std::make_shared<ObjectReader>(sfs, bucketName, key, version, ObjectReader::VersionSelect::Exact), key, mode);
    }
    catch (const ObjectReadError& e)
    {
        if (e.notFound())
            return ReadRes{ .status = ReadRes::Status::NotFound };

        XLOGF(WARN, "Error reading Redis key {} version {}: {}", key, version, e.what());
        return ReadRes{ .status = ReadRes::Status::Error, .value = e.what() };
    }
}

KeyStore::ReadRes KeyStore::getString(const std::string& key, const ReadMode mode)
{
    try
    {
        // Newest version (one lookup). Not found if it is a delete marker
        auto reader = std::make_shared<ObjectReader>(sfs, bucketName, key);
        const auto expiry = expiryFromObjMetadata(reader->GetObjMetadata());
        if (isExpired(expiry))
            return ReadRes{ .status = ReadRes::Status::NotFound };
        if (readerKeyType(*reader) != KeyType::String)
            return ReadRes{ .status = ReadRes::Status::WrongType };

        auto ret = readFromReader(std::move(reader), key, mode);
        ret.expiry = expiry;
        return ret;
    }
    catch (const ObjectReadError& e)
    {
        if (e.notFound())
            return ReadRes{ .status = ReadRes::Status::NotFound };

        XLOGF(WARN, "Error reading Redis key {}: {}", key, e.what());
        return ReadRes{ .status = ReadRes::Status::Error, .value = e.what() };
    }
}

int KeyStore::prepareValue(const int64_t bucketId, const std::string& key, const ValueArg& value, const ContentType& contentType,
                           const std::optional<ExpiryInfo>& expiry, PreparedValue& prepared, const std::string* redisData)
{
    ObjMetadata objMetadata;
    objMetadata.contentType = contentType;
    if (redisData != nullptr)
        objMetadata.redisData = *redisData;
    if (expiry)
    {
        objMetadata.lockInfo.emplace();
        objMetadata.lockInfo->flags = lockinfo_flag_redis_expiry;
        objMetadata.lockInfo->redisExpiryMs = expiry->ms;
        objMetadata.lockInfo->redisExpiryIndexVersion = expiry->indexVersion;
    }

    CWData metadata;
    metadata.addVarInt(metadata_object | objMetadata.flags());
    if (FLAGS_disable_md5_etag)
    {
        const auto versionEtag = sfs.get_next_version_etag();
        if (versionEtag.size() != MD5_DIGEST_LENGTH)
            return EIO;
        metadata.addBuffer(versionEtag.data(), MD5_DIGEST_LENGTH);
    }
    else if (value.streamed)
    {
        // Command runs on the serial executor after the value was written completely
        value.streamed->assertWriter();
        if (value.streamed->md5sum.size() != MD5_DIGEST_LENGTH)
            return value.streamed->err != 0 ? value.streamed->err : EIO;
        metadata.addBuffer(value.streamed->md5sum.data(), MD5_DIGEST_LENGTH);
    }
    else
    {
        unsigned char md5[EVP_MAX_MD_SIZE];
        unsigned int md5Len = 0;
        if (EVP_Digest(value.data.data(), value.data.size(), md5, &md5Len, EVP_md5(), nullptr) != 1 || md5Len != MD5_DIGEST_LENGTH)
            return EIO;
        metadata.addBuffer(reinterpret_cast<const char*>(md5), MD5_DIGEST_LENGTH);
    }
    objMetadata.serialize(metadata);
    prepared.metadata.assign(metadata.getDataPtr(), metadata.getDataSize());

    if (value.streamed)
    {
        auto& stream = *value.streamed;
        stream.assertWriter();
        if (stream.err != 0)
            return stream.err;
        // Can only be used once
        if (!stream.finished || stream.extents.empty())
            return EIO;

        prepared.extents = std::move(stream.extents);
        stream.extents.clear();
        return 0;
    }

    if (value.data.empty())
        return 0;

    // Version is allocated when the value is added as object. Use the version with the longest encoding
    auto res = sfs.write_prepare(make_key(key, bucketId, std::numeric_limits<int64_t>::min()), value.data.size());
    if (res.err != 0)
        return res.err;

    prepared.extents = std::move(res.extents);
    return writeToExtents(sfs, prepared.extents, 0, value.data.data(), value.data.size());
}

int KeyStore::deleteVersion(const int64_t bucketId, const std::string& key, const int64_t version, const bool requireExisting, bool& deleted)
{
    SingleFileStorage::MatchInfo matchInfo;
    if (requireExisting)
        matchInfo.ifMatch.push_back("*");

    const auto rc = sfs.del(make_key(key, bucketId, version), SingleFileStorage::DelAction::Del, false, nullptr, requireExisting ? &matchInfo : nullptr);
    deleted = rc == 0 && (!requireExisting || matchInfo.match);
    return rc;
}

int KeyStore::setValue(const int64_t bucketId, const std::string& key, const ValueArg& value, const std::optional<ExpiryInfo>& expiry)
{
    return setValue(bucketId, key, value, stringContentType(), expiry);
}

int KeyStore::setValue(const int64_t bucketId, const std::string& key, const ValueArg& value, const ContentType& contentType,
                       const std::optional<ExpiryInfo>& expiry, const std::string* redisData)
{
    // Strings replace large collections. Delete their entries
    if (contentType.contentType == stringContentType().contentType && anyLargeCollections.load(std::memory_order_relaxed))
    {
        int err = 0;
        const auto entry = findEnd(bucketId, key, true, err);
        if (err != 0)
            return err;
        if (entry && !entry->tombstone && (entry->type == KeyType::Hash || entry->type == KeyType::Set || entry->type == KeyType::ZSet))
        {
            const auto rc = deleteCollectionEntries(bucketId, key);
            if (rc != 0)
                return rc;
        }
    }

    PreparedValue prepared(sfs);
    auto rc = prepareValue(bucketId, key, value, contentType, expiry, prepared, redisData);
    if (rc != 0)
        return rc;

    // Strings are stored as unversioned object (version 0). It is the value of the key if there are no other
    // versions: Positive versions (list elements, S3 versions, delete markers) sort before it. Negative versions
    // (list elements) only exist after it if the newest version is a list element (pushes check the type)
    const auto fn = make_key(key, bucketId, 0);
    SingleFileStorage::MatchInfo matchInfo;
    matchInfo.readFn = make_key(key, bucketId, std::numeric_limits<int64_t>::max());
    matchInfo.readNewest = true;
    matchInfo.ifVersion = 0;
    rc = prepared.add(fn, &matchInfo);
    if (rc != 0 || matchInfo.match)
        return rc;

    // Key has other versions. Replace them
    rc = prepared.add(fn, nullptr);
    if (rc != 0)
        return rc;

    std::vector<Entry> entries;
    if (!listEntries(bucketId, key, entries))
        return EIO;

    for (const auto& entry : entries)
    {
        if (entry.version != 0)
        {
            bool deleted;
            rc = deleteVersion(bucketId, key, entry.version, false, deleted);
            if (rc != 0)
                return rc;
        }
    }

    return 0;
}

KeyStore::PopStatus KeyStore::popElement(const int64_t bucketId, const std::string& key, const bool left, ReadRes& value, std::string& err)
{
    for (int i = 0; i < maxRetries; ++i)
    {
        int rc = 0;
        const auto entry = findElement(bucketId, key, left, rc);
        if (rc != 0)
        {
            err = fmt::format("ERR storage error {}", rc);
            return PopStatus::Error;
        }

        if (!entry)
            return PopStatus::Empty;

        if (!entry->isList)
            return PopStatus::WrongType;

        // Large values are read while sending the reply (also after the version is deleted)
        auto res = readValue(key, entry->version, ReadMode::Bulk);
        if (res.status == ReadRes::Status::NotFound)
            continue;

        if (res.status == ReadRes::Status::Error)
        {
            err = fmt::format("ERR error reading value: {}", res.value);
            return PopStatus::Error;
        }

        bool deleted;
        rc = deleteVersion(bucketId, key, entry->version, true, deleted);
        if (rc != 0)
        {
            err = fmt::format("ERR storage error {}", rc);
            return PopStatus::Error;
        }

        if (!deleted)
            continue;

        value = std::move(res);
        return PopStatus::Popped;
    }

    err = "ERR too many concurrent modifications";
    return PopStatus::Error;
}

int64_t KeyStore::listLength(const int64_t bucketId, const std::string& key, int& err)
{
    const auto res = sfs.read_prepare(make_key(key, buckets::getCountersBucket(bucketId), 0), SingleFileStorage::ReadSkipAddReading);
    if (res.err == ENOENT)
        return 0;
    if (res.err != 0)
    {
        err = res.err;
        return 0;
    }

    CRData rdata(res.md5sum.data(), res.md5sum.size());
    int64_t count;
    if (!rdata.getVarInt(&count))
    {
        err = EIO;
        return 0;
    }
    return count;
}

void KeyStore::setupCounters(SingleFileStorage::SFSOptions& options)
{
    // Number of list elements per key in Redis buckets, maintained by the storage engine (also for modifications via
    // S3 and if the Redis server is disabled)
    // Also the number of elements of large collections (entries in the counters bucket)
    options.counter_key_callback = [](const std::string& fn) -> std::string
    {
        const auto keyInfo = extractKeyInfoView(fn);
        if (buckets::isCountersBucket(keyInfo.bucketId))
        {
            if (!keyInfo.key.starts_with(reservedKeyPrefix) || !buckets::isRedisBucket(keyInfo.bucketId - 3))
                return std::string();
            const auto counterKey = collectionCounterKeyOfEntry(keyInfo.key);
            if (counterKey.empty())
                return std::string();
            return make_key(counterKey, keyInfo.bucketId, 0);
        }
        if (!buckets::isRedisBucket(keyInfo.bucketId))
            return std::string();
        return make_key(keyInfo.key, buckets::getCountersBucket(keyInfo.bucketId), 0);
    };
    options.counter_counts_callback = [](const std::string& fn, const std::string& md5sum)
    {
        // Collection entries (counter_key_callback only returns a counter for them in the counters bucket)
        if (buckets::isCountersBucket(extractKeyInfoView(fn).bucketId))
            return true;
        return isListElement(md5sum);
    };
}

void KeyStore::syncListing()
{
    // Listings without WAL/manual commit only see committed modifications. Commit the modifications of the
    // transaction/script so far (atomicity is ensured by the locks). With WAL/manual commit, listings include
    // uncommitted modifications
    if (currentTx != nullptr && currentTx->needCommit && !sfs.listings_include_uncommitted())
    {
        if (S3Handler::commitStorage(sfs))
            currentTx->needCommit = false;
    }
}

bool KeyStore::commit()
{
    if (currentTx != nullptr)
    {
        // Committed at the end of the transaction/script
        currentTx->needCommit = true;
        return true;
    }
    return S3Handler::commitStorage(sfs);
}

bool KeyStore::iterateKeys(const int64_t bucketId, const std::optional<std::string>& startAfter, const std::string& prefix,
                           const std::function<bool(const std::string&, KeyType)>& f, bool& atEnd)
{
    syncListing();

    atEnd = false;

    const auto start = startAfter && *startAfter >= prefix ? make_key(*startAfter, bucketId, std::numeric_limits<int64_t>::min())
                                                           : make_key(prefix, bucketId, std::numeric_limits<int64_t>::max());

    SingleFileStorage::ListIterData iterData;
    SCOPE_EXIT { sfs.list_iter_stop(iterData); };

    if (!sfs.list_iter_start(start, start, [bucketId, prefix](const std::string& iterKey)
                             {
                                 const auto keyInfo = extractKeyInfoView(iterKey);
                                 return keyInfo.bucketId == bucketId && keyInfo.key.starts_with(prefix); }, iterData))
    {
        return false;
    }

    // Versions of a key are newest first, so the first entry of a key determines whether it is live
    std::optional<std::string> prevKey = startAfter;
    while (true)
    {
        std::string keyBin, md5sum;
        int64_t offset, size, lastModified;
        std::vector<SingleFileStorage::SPunchItem> extraExts;
        if (!sfs.iter_curr_val(keyBin, offset, size, extraExts, lastModified, md5sum, iterData.curr))
        {
            atEnd = true;
            return true;
        }

        const auto keyInfo = extractKeyInfoView(keyBin);
        if (keyInfo.bucketId != bucketId || !keyInfo.key.starts_with(prefix))
        {
            atEnd = true;
            return true;
        }

        if (!prevKey || *prevKey != keyInfo.key)
        {
            prevKey = std::string(keyInfo.key);
            auto [type, expiry] = keyInfoFromMetadata(md5sum);
            if (type == KeyType::List && anyExpiries.load(std::memory_order_relaxed))
            {
                int err = 0;
                expiry = listExpiry(bucketId, *prevKey, err);
            }
            if (isExpired(expiry))
                type = KeyType::None;
            if (!f(*prevKey, type))
                return true;
        }

        if (!sfs.list_iter_next(iterData))
            return false;
    }
}

int KeyStore::deleteKey(const int64_t bucketId, const std::string& key, bool& existed)
{
    existed = false;
    std::vector<Entry> entries;
    if (!listEntries(bucketId, key, entries))
        return EIO;

    // Expiry of lists (in the counter, which is deleted with the last element)
    const auto newest = std::find_if(entries.begin(), entries.end(), [](const Entry& entry)
                                     { return !entry.tombstone; });
    if (newest != entries.end() && newest->isList && anyExpiries.load(std::memory_order_relaxed))
    {
        int err = 0;
        const auto expiry = listExpiry(bucketId, key, err);
        if (err != 0)
            return err;
        if (expiry)
        {
            const auto rc = removeExpiryIndex(bucketId, *expiry);
            if (rc != 0)
                return rc;
        }
    }

    // Entries of large collections
    if (newest != entries.end() && (newest->type == KeyType::Hash || newest->type == KeyType::Set || newest->type == KeyType::ZSet) &&
        anyLargeCollections.load(std::memory_order_relaxed))
    {
        const auto rc = deleteCollectionEntries(bucketId, key);
        if (rc != 0)
            return rc;
    }

    for (const auto& entry : entries)
    {
        if (!entry.tombstone)
            existed = true;

        if (entry.expiry)
        {
            const auto rc = removeExpiryIndex(bucketId, *entry.expiry);
            if (rc != 0)
                return rc;
        }

        bool deleted;
        const auto rc = deleteVersion(bucketId, key, entry.version, false, deleted);
        if (rc != 0)
            return rc;
    }

    return 0;
}

std::optional<std::string> KeyStore::openCollection(const std::string& key, const KeyType type, CollState& cs)
{
    cs = CollState();
    cs.small.type = type;
    try
    {
        ObjectReader reader(sfs, bucketName, key);
        const auto objMetadata = reader.GetObjMetadata();
        const auto expiry = expiryFromObjMetadata(objMetadata);
        // Expired (but not deleted yet)
        if (isExpired(expiry))
            return std::nullopt;
        if (readerKeyType(reader) != type)
            return resp::wrongType;

        cs.exists = true;
        cs.expiry = expiry;
        cs.small.expiry = expiry;
        if (objMetadata == nullptr || !objMetadata->redisData)
        {
            cs.large = true;
            return std::nullopt;
        }

        if (!cs.small.deserialize(*objMetadata->redisData))
            return resp::error(fmt::format("ERR corrupted value of key '{}'", key));
        return std::nullopt;
    }
    catch (const ObjectReadError& e)
    {
        if (e.notFound())
            return std::nullopt;

        XLOGF(WARN, "Error reading Redis key {}: {}", key, e.what());
        return resp::error(fmt::format("ERR {}", e.what()));
    }
}

int KeyStore::deleteCollectionEntries(const int64_t bucketId, const std::string& key)
{
    const auto countersBucket = buckets::getCountersBucket(bucketId);
    for (const auto type : { KeyType::Hash, KeyType::Set, KeyType::ZSet })
    {
        if (auto err = deleteEntries(countersBucket, collectionPrefix(type, key), nullptr))
            return EIO;
    }
    return 0;
}

int KeyStore::writeSmallCollection(const int64_t bucketId, const std::string& key, const Collection& coll, bool& converted)
{
    converted = false;
    if (coll.size() == 0)
    {
        bool existed;
        return deleteKey(bucketId, key, existed);
    }

    const auto data = coll.serialize();
    if (static_cast<int64_t>(coll.size()) <= FLAGS_redis_collection_inline_max_entries &&
        static_cast<int64_t>(data.size()) <= FLAGS_redis_collection_inline_max_bytes)
    {
        return setValue(bucketId, key, ValueArg{}, collectionContentType(coll.type), coll.expiry, &data);
    }

    // Convert to large collection. Remove entries left over (e.g. if the key was deleted via S3)
    anyLargeCollections = true;
    converted = true;
    auto rc = deleteCollectionEntries(bucketId, key);
    if (rc != 0)
        return rc;
    LargeCollOps large(*this, bucketId, key, coll.type);
    rc = large.addAll(coll);
    if (rc != 0)
        return rc;
    return setValue(bucketId, key, ValueArg{}, collectionContentType(coll.type), coll.expiry, nullptr);
}

bool KeyStore::locksCollection(const std::string& key, const KeyType type, const std::vector<std::string>& fields, CollLocks& locks,
                               CollState& cs, std::optional<std::string>& err)
{
    if (!fields.empty())
    {
        // Large collections: Lock the key shared and the modified fields
        locks.shared.emplace(*this, key);
        err = openCollection(key, type, cs);
        if (err)
            return false;
        if (cs.large)
        {
            locks.group.emplace(*this, key, CollGroupLock::writers);
            locks.fields.emplace(*this, key, fields);
            // For WATCH
            touchKeyLock(keyLockIdx(key));
            return true;
        }
        locks.shared.reset();
    }

    locks.exclusive.emplace(*this, key);
    err = openCollection(key, type, cs);
    return !err;
}

std::optional<std::string> KeyStore::openCollectionForRead(const std::string& key, const KeyType type, const bool multiElement, CollLocks& locks,
                                                           CollState& cs)
{
    if (!multiElement)
        return openCollection(key, type, cs);

    // Reads of multiple elements of large collections exclude field modifications (atomic). Small collections are
    // read from one object
    locks.shared.emplace(*this, key);
    auto err = openCollection(key, type, cs);
    if (!err && cs.large)
        locks.group.emplace(*this, key, CollGroupLock::readers);
    return err;
}

void KeyStore::deleteIfEmpty(const int64_t bucketId, const std::string& key, const KeyType type)
{
    KeyLock lock(*this, key);
    CollState cs;
    if (openCollection(key, type, cs) || !cs.exists || !cs.large)
        return;
    LargeCollOps large(*this, bucketId, key, type);
    int err = 0;
    if (large.count(err) == 0 && err == 0)
    {
        bool existed;
        if (deleteKey(bucketId, key, existed) == 0 && !commit())
            XLOGF(WARN, "Error committing deletion of empty Redis key {}", key);
    }
}

std::optional<KeyStore::ExpiryInfo> KeyStore::listExpiry(const int64_t bucketId, const std::string& key, int& err)
{
    const auto res = sfs.read_prepare(make_key(key, buckets::getCountersBucket(bucketId), 0), SingleFileStorage::ReadSkipAddReading);
    if (res.err == ENOENT)
        return std::nullopt;
    if (res.err != 0)
    {
        err = res.err;
        return std::nullopt;
    }

    CRData rdata(res.md5sum.data(), res.md5sum.size());
    int64_t count;
    ExpiryInfo expiry;
    if (!rdata.getVarInt(&count) || !rdata.getVarInt(&expiry.ms) || !rdata.getVarInt(&expiry.indexVersion))
        return std::nullopt;
    return expiry;
}

std::optional<KeyStore::ExpiryInfo> KeyStore::entryExpiry(const int64_t bucketId, const std::string& key, const Entry& entry, int& err)
{
    if (entry.isList)
        return listExpiry(bucketId, key, err);
    return entry.expiry;
}

std::optional<KeyStore::ExpiryInfo> KeyStore::getExpiry(const int64_t bucketId, const std::string& key, int& err)
{
    const auto entry = findElement(bucketId, key, true, err);
    if (!entry)
        return std::nullopt;
    return entryExpiry(bucketId, key, *entry, err);
}

int KeyStore::addExpiryIndex(const int64_t bucketId, const std::string& key, const int64_t expiryMs, ExpiryInfo& expiry)
{
    // Unique version (> 0), so entries with the same expiry time differ
    const auto indexVersion = sfs.get_next_version();

    const auto rc = sfs.write_finalize(make_key(expiryIndexKey(expiryMs), buckets::getCountersBucket(bucketId), indexVersion), {}, expiryMs, key, false, true);
    if (rc != 0)
        return rc;

    expiry = ExpiryInfo{ .ms = expiryMs, .indexVersion = indexVersion };

    std::scoped_lock lock(expiryFlagMutex);
    ++expiryGeneration;
    anyExpiries = true;
    return 0;
}

int KeyStore::removeExpiryIndex(const int64_t bucketId, const ExpiryInfo& expiry)
{
    const auto rc = sfs.del(make_key(expiryIndexKey(expiry.ms), buckets::getCountersBucket(bucketId), expiry.indexVersion), SingleFileStorage::DelAction::Del, false);
    return rc == ENOENT ? 0 : rc;
}

int KeyStore::modifyExpiry(const int64_t bucketId, const std::string& key, const Entry& entry, const std::optional<ExpiryInfo>& expiry)
{
    CWData param;
    if (entry.isList)
    {
        // Stored in the counter of the list
        param.addChar(metadata_modify_counter_tail);
        if (expiry)
        {
            param.addVarInt(expiry->ms);
            param.addVarInt(expiry->indexVersion);
        }
        return sfs.modify_metadata(make_key(key, buckets::getCountersBucket(bucketId), 0), std::string_view(param.getDataPtr(), param.getDataSize()));
    }

    param.addChar(metadata_modify_redis_expiry);
    param.addChar(expiry ? 1 : 0);
    if (expiry)
    {
        param.addVarInt(expiry->ms);
        param.addVarInt(expiry->indexVersion);
    }
    return sfs.modify_metadata(make_key(key, bucketId, entry.version), std::string_view(param.getDataPtr(), param.getDataSize()));
}

int KeyStore::setExpiry(const int64_t bucketId, const std::string& key, const int64_t expiryMs)
{
    int err = 0;
    const auto entry = findElement(bucketId, key, true, err);
    if (err != 0)
        return err;
    if (!entry)
        return ENOENT;

    const auto prev = entryExpiry(bucketId, key, *entry, err);
    if (err != 0)
        return err;
    if (prev)
    {
        const auto rc = removeExpiryIndex(bucketId, *prev);
        if (rc != 0)
            return rc;
    }

    ExpiryInfo expiry;
    auto rc = addExpiryIndex(bucketId, key, expiryMs, expiry);
    if (rc != 0)
        return rc;

    return modifyExpiry(bucketId, key, *entry, expiry);
}

int KeyStore::clearExpiry(const int64_t bucketId, const std::string& key, bool* removed)
{
    if (removed)
        *removed = false;

    int err = 0;
    const auto entry = findElement(bucketId, key, true, err);
    if (err != 0)
        return err;
    if (!entry)
        return 0;
    const auto prev = entryExpiry(bucketId, key, *entry, err);
    if (err != 0)
        return err;
    if (!prev)
        return 0;

    auto rc = removeExpiryIndex(bucketId, *prev);
    if (rc != 0)
        return rc;

    rc = modifyExpiry(bucketId, key, *entry, std::nullopt);
    if (rc != 0)
        return rc;

    if (removed)
        *removed = true;
    return 0;
}

std::optional<KeyStore::ExpiryInfo> KeyStore::removeCurrentExpiry(const int64_t bucketId, const std::string& key, int& err)
{
    if (!anyExpiries.load(std::memory_order_relaxed))
        return std::nullopt;

    const auto expiry = getExpiry(bucketId, key, err);
    if (err != 0 || !expiry)
        return std::nullopt;

    err = removeExpiryIndex(bucketId, *expiry);
    return expiry;
}

bool KeyStore::expireIfNeeded(const int64_t bucketId, const std::string& key)
{
    int err = 0;
    if (!isExpired(getExpiry(bucketId, key, err)))
        return false;

    {
        KeyLock lock(*this, key);
        if (!isExpired(getExpiry(bucketId, key, err)))
            return false;

        bool existed;
        const auto rc = deleteKey(bucketId, key, existed);
        if (rc != 0)
        {
            XLOGF(WARN, "Error deleting expired Redis key {}: {}", key, rc);
            return false;
        }
    }

    if (!commit())
        XLOGF(WARN, "Error committing deletion of expired Redis key {}", key);

    notifyKey(key);
    return true;
}

void KeyStore::expireKeys()
{
    const auto bucketInfo = buckets::getBucketInfo(bucketName);
    if (!bucketInfo)
        return;

    uint64_t generation;
    {
        std::scoped_lock lock(expiryFlagMutex);
        generation = expiryGeneration;
    }

    constexpr size_t maxBatch = 1000;
    const auto countersBucket = buckets::getCountersBucket(bucketInfo->id);

    while (true)
    {
        struct IndexEntry
        {
            std::string key;
            ExpiryInfo expiry;
        };
        std::vector<IndexEntry> expired;
        bool foundAny = false;
        const auto now = nowMs();
        {
            SingleFileStorage::ListIterData iterData;
            SCOPE_EXIT { sfs.list_iter_stop(iterData); };

            const auto start = make_key(expiryIndexPrefix, countersBucket, std::numeric_limits<int64_t>::max());
            if (!sfs.list_iter_start(start, start, [countersBucket](const std::string& iterKey)
                                     {
                                         const auto keyInfo = extractKeyInfoView(iterKey);
                                         return keyInfo.bucketId == countersBucket && keyInfo.key.starts_with(expiryIndexPrefix); }, iterData))
            {
                XLOGF(WARN, "Error listing Redis key expiries");
                return;
            }

            while (expired.size() < maxBatch)
            {
                std::string keyBin, md5sum;
                int64_t offset, size, lastModified;
                std::vector<SingleFileStorage::SPunchItem> extraExts;
                if (!sfs.iter_curr_val(keyBin, offset, size, extraExts, lastModified, md5sum, iterData.curr))
                    break;

                const auto keyInfo = extractKeyInfoView(keyBin);
                if (keyInfo.bucketId != countersBucket || !keyInfo.key.starts_with(expiryIndexPrefix))
                    break;

                const auto expiryMs = parseExpiryIndexKey(keyInfo.key);
                // Skip counters of keys which happen to start with the prefix (only possible via S3)
                if (expiryMs && keyInfo.version != 0)
                {
                    foundAny = true;
                    if (*expiryMs > now)
                        break;
                    expired.push_back(IndexEntry{ .key = md5sum, .expiry = ExpiryInfo{ .ms = *expiryMs, .indexVersion = keyInfo.version } });
                }

                if (!sfs.list_iter_next(iterData))
                    break;
            }
        }

        if (!foundAny)
        {
            std::scoped_lock lock(expiryFlagMutex);
            if (generation == expiryGeneration)
                anyExpiries = false;
            return;
        }

        if (expired.empty())
            return;

        std::shared_lock globalLockShared(globalLock);
        for (const auto& entry : expired)
        {
            KeyLock lock(*this, entry.key);
            int err = 0;
            const auto curr = getExpiry(bucketInfo->id, entry.key, err);
            if (err != 0)
                continue;

            if (curr == entry.expiry)
            {
                bool existed;
                const auto rc = deleteKey(bucketInfo->id, entry.key, existed);
                if (rc != 0)
                    XLOGF(WARN, "Error deleting expired Redis key {}: {}", entry.key, rc);
            }
            else
            {
                // Stale index entry (e.g. key was deleted via S3)
                removeExpiryIndex(bucketInfo->id, entry.expiry);
            }
        }

        if (!commit())
            XLOGF(WARN, "Error committing deletion of expired Redis keys");

        for (const auto& entry : expired)
            notifyKey(entry.key);

        if (expired.size() < maxBatch)
            return;
    }
}

void KeyStore::expiryThreadFunc()
{
    while (true)
    {
        {
            CondVarLock lock(expiryThreadMutex);
            if (expiryThreadStop)
                return;
            lock.waitFor(expiryThreadCond, std::chrono::milliseconds(100));
            if (expiryThreadStop)
                return;
        }

        try
        {
            expireKeys();
        }
        catch (const std::exception& e)
        {
            XLOGF(ERR, "Exception expiring Redis keys: {}", e.what());
        }
    }
}

std::optional<std::string> KeyStore::deleteEntries(const int64_t bucketId, const std::string& prefix, const std::function<bool(int64_t)>& filter)
{
    syncListing();
    while (true)
    {
        // Collect a batch of entries (all versions) and delete them
        std::vector<std::string> batch;
        {
            SingleFileStorage::ListIterData iterData;
            SCOPE_EXIT { sfs.list_iter_stop(iterData); };

            const auto start = make_key(prefix, bucketId, std::numeric_limits<int64_t>::max());
            if (!sfs.list_iter_start(start, start, [bucketId, &prefix](const std::string& iterKey)
                                     {
                                         const auto keyInfo = extractKeyInfoView(iterKey);
                                         return keyInfo.bucketId == bucketId && keyInfo.key.starts_with(prefix); }, iterData))
            {
                return resp::error("ERR error listing keys");
            }

            while (batch.size() < flushBatchSize)
            {
                std::string keyBin, md5sum;
                int64_t offset, size, lastModified;
                std::vector<SingleFileStorage::SPunchItem> extraExts;
                if (!sfs.iter_curr_val(keyBin, offset, size, extraExts, lastModified, md5sum, iterData.curr))
                    break;

                const auto keyInfo = extractKeyInfoView(keyBin);
                if (keyInfo.bucketId != bucketId || !keyInfo.key.starts_with(prefix))
                    break;

                if (!filter || filter(keyInfo.version))
                    batch.push_back(std::move(keyBin));

                if (!sfs.list_iter_next(iterData))
                    return resp::error("ERR error listing keys");
            }
        }

        if (batch.empty())
            return std::nullopt;

        for (const auto& keyBin : batch)
        {
            const auto rc = sfs.del(keyBin, SingleFileStorage::DelAction::Del, false);
            if (rc != 0)
                return resp::error(fmt::format("ERR storage error {}", rc));
        }

        // Listings without WAL/manual commit only see committed deletions. Commit also in transactions/scripts, so
        // the next listing does not return the deleted entries again
        if (!(sfs.listings_include_uncommitted() ? commit() : S3Handler::commitStorage(sfs)))
            return resp::error("ERR storage commit failed");
    }
}

bool KeyStore::hasEntries(const int64_t bucketId, const std::string& prefix)
{
    SingleFileStorage::ListIterData iterData;
    SCOPE_EXIT { sfs.list_iter_stop(iterData); };

    const auto start = make_key(prefix, bucketId, std::numeric_limits<int64_t>::max());
    if (!sfs.list_iter_start(start, start, [bucketId, &prefix](const std::string& iterKey)
                             {
                                 const auto keyInfo = extractKeyInfoView(iterKey);
                                 return keyInfo.bucketId == bucketId && keyInfo.key.starts_with(prefix); }, iterData))
    {
        // Assume there are
        return true;
    }

    std::string keyBin, md5sum;
    int64_t offset, size, lastModified;
    std::vector<SingleFileStorage::SPunchItem> extraExts;
    if (!sfs.iter_curr_val(keyBin, offset, size, extraExts, lastModified, md5sum, iterData.curr))
        return false;
    const auto keyInfo = extractKeyInfoView(keyBin);
    return keyInfo.bucketId == bucketId && keyInfo.key.starts_with(prefix);
}

std::optional<std::string> KeyStore::flushReserved(const int64_t bucketId)
{
    // All entries starting with the reserved prefix in the counters bucket (expiry index, large collections)
    return deleteEntries(buckets::getCountersBucket(bucketId), reservedKeyPrefix, nullptr);
}

std::unique_ptr<KeyStore::CollOps> KeyStore::collOps(const int64_t bucketId, const std::string& key, const KeyType type, CollState& cs)
{
    if (cs.large)
        return std::make_unique<LargeCollOps>(*this, bucketId, key, type);
    return std::make_unique<SmallCollOps>(cs.small);
}

bool KeyStore::initBucket(const std::string& bucket)
{
    if (!buckets::isValidBucketName(bucket))
    {
        XLOGF(ERR, "Invalid Redis bucket name {}", bucket);
        return false;
    }

    if (const auto bucketId = buckets::getBucket(bucket))
    {
        const auto bucketInfo = buckets::getBucketInfo(bucket);
        if (bucketInfo && bucketInfo->versioning != buckets::VersioningState::Enabled)
            XLOGF(WARN, "Versioning is not enabled on Redis bucket {}. Redis commands will fail until it is enabled", bucket);

        if (bucketInfo && !bucketInfo->redis)
            XLOGF(WARN, "Marking existing bucket {} as Redis bucket. List elements already in it are not counted", bucket);

        if (!buckets::setRedis(*bucketId))
        {
            XLOGF(ERR, "Error marking bucket {} as Redis bucket", bucket);
            return false;
        }
        return true;
    }

    const auto bucketId = buckets::addBucket(bucket, false);
    if (bucketId < 0)
    {
        XLOGF(ERR, "Error creating Redis bucket {}", bucket);
        return false;
    }

    if (buckets::setVersioning(bucket, buckets::VersioningState::Enabled) != buckets::SetVersioningResult::Ok)
    {
        XLOGF(ERR, "Error enabling versioning on Redis bucket {}", bucket);
        return false;
    }

    if (!buckets::setRedis(bucketId))
    {
        XLOGF(ERR, "Error marking bucket {} as Redis bucket", bucket);
        return false;
    }

    XLOGF(INFO, "Created Redis bucket {} with versioning enabled", bucket);
    return true;
}
