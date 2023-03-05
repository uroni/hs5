/**
 * Copyright Martin Raiber. All Rights Reserved.
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
 
#include "s3handler.h"
#include "SingleFileStorage.h"
#include <algorithm>
#include <asm-generic/errno-base.h>
#include <asm-generic/errno.h>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <folly/Format.h>
#include <folly/Range.h>
#include <folly/String.h>
#include <folly/executors/GlobalExecutor.h>
#include <folly/io/IOBuf.h>
#include <folly/io/IOBufQueue.h>
#include <folly/logging/LogLevel.h>
#include <folly/logging/xlog.h>
#include <limits>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <proxygen/lib/http/HTTPCommonHeaders.h>
#include <proxygen/lib/http/HTTPMethod.h>

using namespace proxygen;

const char* c_commit_uuid = "a711e93e-93b4-4a9e-8a0b-688797470002";

std::string hashSha256Hex(const std::string &payload)
{
    unsigned char md[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char *>(payload.data()),
           payload.size(), md);
    return folly::hexlify<std::string>(
        folly::ByteRange(md, SHA256_DIGEST_LENGTH));
}

std::string hmacSha256Binary(const std::string &key,
                             const std::string &payload)
{
    std::string ret;
    ret.resize(SHA256_DIGEST_LENGTH);
    unsigned int len = SHA256_DIGEST_LENGTH;
    HMAC(EVP_sha256(), key.data(), key.size(),
         reinterpret_cast<const unsigned char *>(payload.data()), payload.size(),
         reinterpret_cast<unsigned char *>(&ret[0]), &len);
    assert(len == SHA256_DIGEST_LENGTH);
    return ret;
}

std::string currDate()
{
    time_t t = toTimeT(getCurrentTime<SteadyClock>());
    struct tm final_tm;
    localtime_r(&t, &final_tm);
    std::string ret;
    ret.resize(8);
    strftime(&ret[0], ret.size(), "%Y%m%d", &final_tm);
    return ret;
}

bool checkSig(HTTPMessage &headers, const std::string &secretKey,
              const folly::StringPiece &authorization,
              const std::string &payload)
{
    const char alg_name[] = "AWS4-HMAC-SHA256";
    const char alg[] = "AWS4-HMAC-SHA256 ";
    if (authorization.find(alg) != 0)
        return false;

    std::vector<folly::StringPiece> authorizationVec;
    folly::split(',', authorization.subpiece(sizeof(alg)), authorizationVec);

    std::map<folly::StringPiece, folly::StringPiece> authorizationMap;

    for (auto ave : authorizationVec)
    {
        size_t eq = ave.find_first_of('=');
        if (eq != std::string::npos)
        {
            authorizationMap.insert(
                std::make_pair(ave.subpiece(0, eq), ave.subpiece(eq + 1)));
        }
    }

    const char signedHeadersKey[] = "SignedHeaders";
    auto itSignedHeaders = authorizationMap.find(signedHeadersKey);
    if (itSignedHeaders == authorizationMap.end())
        return false;

    const char credentialHeaderKey[] = "Credential";
    auto itCredential = authorizationMap.find(credentialHeaderKey);
    if (itCredential == authorizationMap.end())
        return false;

    const char signatureHeaderKey[] = "Signature";
    auto itSignature = authorizationMap.find(signatureHeaderKey);
    if (itSignature == authorizationMap.end())
        return false;

    std::vector<folly::StringPiece> credentialScopeToks;
    folly::split('/', itCredential->second, credentialScopeToks);

    if (credentialScopeToks.size() != 5)
        return false;

    std::vector<folly::StringPiece> signedHeadersVec;
    folly::split(';', itSignedHeaders->second, signedHeadersVec);

    std::string canonicalHeaders;
    std::optional<folly::Range<const char *> > prevSignedHeader;
    bool hasHost = false;
    for (auto signedHeader : signedHeadersVec)
    {
        if (prevSignedHeader && prevSignedHeader >= signedHeader)
        {
            return false;
        }
        auto fullVal = headers.getHeaders().getSingleOrEmpty(signedHeader);
        auto val = folly::trimWhitespace(fullVal);
        canonicalHeaders += folly::sformat("{}:{}\n", signedHeader, val);
        prevSignedHeader = signedHeader;
        if (signedHeader == "host" && !val.empty())
            hasHost = true;
    }

    if (!hasHost)
        return false;

    auto params = headers.getQueryParams();
    std::string canonicalParamStr;
    for (auto param : params)
    {
        if (!canonicalParamStr.empty())
            canonicalParamStr += "&";
        canonicalParamStr += param.first + "=" +
                             folly::uriEscape<std::string>(
                                 param.second, folly::UriEscapeMode::QUERY);
    }
    std::string canonicalRequest = folly::sformat(
        "{}\n{}\n{}\n{}\n{}\n{}\n", headers.getMethodString(),
        headers.getPathAsStringPiece(), canonicalParamStr, canonicalHeaders,
        itSignedHeaders->second, hashSha256Hex(payload));

    std::string hashedCanonicalRequest = hashSha256Hex(canonicalRequest);
    std::string requestDateTime =
        headers.getHeaders().getSingleOrEmpty("X-Amz-Date");

    std::string stringToSign = folly::sformat(
        "{}\n{}\n{}{}{}{}\n{}\n", alg_name, requestDateTime,
        credentialScopeToks[1], credentialScopeToks[2], credentialScopeToks[3],
        credentialScopeToks[4], hashedCanonicalRequest);

    std::string signingKey = hmacSha256Binary(
        hmacSha256Binary(
            hmacSha256Binary(hmacSha256Binary("AWS4" + secretKey, currDate()),
                             credentialScopeToks[1].toString()),
            credentialScopeToks[2].toString()),
        "aws4_request");

    std::string sig = folly::hexlify(hmacSha256Binary(signingKey, stringToSign));

    return sig == itSignature->second;
}

/**
 * Handles requests by serving the file named in path.  Only supports GET.
 * reads happen in a CPU thread pool since read(2) is blocking.
 * If egress pauses, file reading is also paused.
 */

void S3Handler::onRequest(std::unique_ptr<HTTPMessage> headers) noexcept
{
    if (headers->getMethod() != HTTPMethod::PUT &&
        headers->getMethod() != HTTPMethod::GET &&
        headers->getMethod() != HTTPMethod::HEAD &&
        headers->getMethod() != HTTPMethod::DELETE)
    {
        ResponseBuilder(downstream_)
            .status(400, "Bad method")
            .body("Only GET/PUT is supported")
            .sendWithEOM();
        return;
    }

    if (headers->getMethod() == HTTPMethod::GET || headers->getMethod() == HTTPMethod::HEAD)
    {
        request_type = headers->getMethod() == HTTPMethod::GET ? RequestType::GetObject : RequestType::HeadObject;

        auto header_path = headers->getPathAsStringPiece();
        if(!header_path.empty())
        {
            fpath = std::string(header_path.subpiece(1));
        }

        running = true;

        if(fpath.find('/')==std::string::npos)
        {
            listObjects(*headers, fpath);
            return;
        }

        if(fpath.find(c_commit_uuid)!=std::string::npos)
        {
            getCommitObject(*headers);
            return;
        }
        
        getObject(*headers);
        return;
    }
    else if (headers->getMethod() == HTTPMethod::PUT)
    {
        request_type = RequestType::PutObject;
        fpath = std::string(headers->getPathAsStringPiece().subpiece(1));
        std::string cl = headers->getHeaders().getSingleOrEmpty(
            proxygen::HTTP_HEADER_CONTENT_LENGTH);
        if (cl.empty())
        {
            ResponseBuilder(downstream_)
                .status(500, "Internal error")
                .body("Content-Length header not set")
                .sendWithEOM();
            return;
        }
        put_remaining = std::atoll(cl.c_str());

        XLOGF(DBG0, "PutObject {} length {}", fpath, put_remaining);

        if(fpath.find(c_commit_uuid)!=std::string::npos)
        {
            commit(*headers);
            return;
        }

        putObject(*headers);
        return;
    }
    else if(headers->getMethod() == HTTPMethod::DELETE)
    {
        request_type = RequestType::DeleteObject;
        deleteObject(*headers);
    }
}

void S3Handler::listObjects(proxygen::HTTPMessage& headers, const std::string& bucket)
{
    request_type = RequestType::ListObjects;
    auto marker = headers.getQueryParam("marker");
    auto max_keys = headers.getIntQueryParam("max-keys", 1000);
    auto prefix = headers.getQueryParam("prefix");
    auto delimiter = headers.getQueryParam("delimiter");

    auto evb = folly::EventBaseManager::get()->getEventBase();

    folly::getGlobalCPUExecutor()->add(
    [self = self, evb, marker, max_keys, prefix, delimiter, bucket]()
    {
        self->listObjects(evb, self, marker, std::max(0, std::min(10000, max_keys)), prefix, delimiter, bucket);
    });
}

void S3Handler::getCommitObject(proxygen::HTTPMessage& headers)
{
    if(request_type==RequestType::HeadObject)
    {
        ResponseBuilder(self->downstream_).status(200, "OK").header(proxygen::HTTP_HEADER_CONTENT_LENGTH, std::to_string(sfs.get_runtime_id().size())).sendWithEOM();
        return;
    }

    ResponseBuilder(self->downstream_)
                        .status(200, "OK")
                        .body(fmt::format("{}", sfs.get_runtime_id()))
                        .sendWithEOM();
}

void S3Handler::commit(proxygen::HTTPMessage& headers)
{
    if(put_remaining>0)
    {
            ResponseBuilder(self->downstream_)
                    .status(500, "Internal error")
                    .body(fmt::format("Body length != 0"))
                    .sendWithEOM();
    }

    auto evb = folly::EventBaseManager::get()->getEventBase();
    folly::getGlobalCPUExecutor()->add(
    [self = this->self, evb]()
    {
        bool b = self->sfs.commit(false, -1);

        evb->runInEventBaseThread([self = self, b]()
                                        {
        if(!b)
        {
            ResponseBuilder(self->downstream_)
                    .status(500, "Internal error")
                    .body(fmt::format("Commit error"))
                    .sendWithEOM();
        }
        else {
            ResponseBuilder(self->downstream_)
                    .status(200, "OK")
                    .sendWithEOM();
        }
                                        });
    });
}

void S3Handler::getObject(proxygen::HTTPMessage& headers)
{
    auto evb = folly::EventBaseManager::get()->getEventBase();
    folly::getGlobalCPUExecutor()->add(
            [self = self, evb]()
            { 
                unsigned int flags = 0;
                if(self->request_type == RequestType::HeadObject)
                    flags |= SingleFileStorage::ReadMetaOnly;

                auto res = self->sfs.read_prepare(self->fpath, flags);

                if (res.err != 0)
                {
                    evb->runInEventBaseThread([self = self, res]()
                                              {

                        if(res.err==ENOENT)
                        {
                             ResponseBuilder(self->downstream_)
                                .status(404, "Not found")
                                .body(fmt::format("Object not found"))
                                .sendWithEOM();
                        }
                        else if(res.err==ENOTRECOVERABLE)
                        {
                            ResponseBuilder(self->downstream_)
                                .status(500, "Internal error")
                                .body(fmt::format("Storage is dead"))
                                .sendWithEOM();
                        }
                        else
                        {
                            ResponseBuilder(self->downstream_)
                                .status(500, "Internal error")
                                .body(fmt::format("Error code: {}", res.err))
                                .sendWithEOM();
                        }
                                              });
                    return;
                }

                evb->runInEventBaseThread([self = self, total_len = res.total_len]()
                                              {
                    auto resp = std::move(ResponseBuilder(self->downstream_).status(200, "OK").header(proxygen::HTTP_HEADER_CONTENT_LENGTH, std::to_string(total_len)));

                    if(self->request_type==RequestType::HeadObject)
                    {
                        XLOGF(DBG0, "Content length {} bytes for readObject HEAD of {}", total_len, self->fpath);
                        resp.sendWithEOM();
                        return;
                    }
                    else
                    {
                        XLOGF(DBG0, "Content length {} bytes for readObject GET of {}", total_len, self->fpath);
                        resp.send();
                    }
                                              });

                if (self->request_type == RequestType::HeadObject)
                    return;

                self->extents = std::move(res.extents);
                self->put_remaining.store(res.total_len, std::memory_order_relaxed);

                self->readObject(evb, std::move(self), 0);
            });
}

void S3Handler::putObject(proxygen::HTTPMessage& headers)
{
    auto evb = folly::EventBaseManager::get()->getEventBase();
    folly::getGlobalCPUExecutor()->add(
            [self = this->self, evb]()
            {
                auto res = self->sfs.write_prepare(self->fpath, self->put_remaining, std::string::npos);
                if (res.err != 0)
                {                    
                    evb->runInEventBaseThread([self = self, res]()
                                              {
                        ResponseBuilder(self->downstream_)
                            .status(500, "Internal error")
                            .body(fmt::format("Error preparing writing. Errno {}", res.err))
                            .sendWithEOM();
                        std::lock_guard lock(self->extents_mutex);
                        self->finished_ = true;
                        self->extents_cond.notify_all(); });
                    return;
                }

                std::lock_guard lock(self->extents_mutex);
                self->extents = std::move(res.extents);
                self->extents_cond.notify_all();
            });
}

void S3Handler::deleteObject(proxygen::HTTPMessage& headers)
{
    fpath = std::string(headers.getPathAsStringPiece().subpiece(1));
    auto evb = folly::EventBaseManager::get()->getEventBase();

    folly::getGlobalCPUExecutor()->add(
        [self = this->self, evb]()
        {
            auto res = self->sfs.del(self->fpath, SingleFileStorage::DelAction::Del, false);

            if(res && !self->sfs.get_manual_commit())
            {
                res = self->sfs.commit(false, -1);
            }

            evb->runInEventBaseThread([self = self, res]()
                                            {
                    if(!res && self->sfs.get_is_dead())
                    {
                        ResponseBuilder(self->downstream_)
                            .status(500, "Internal error")
                            .body(fmt::format("Storage is dead"))
                            .sendWithEOM();
                    }
                    else if(!res)
                    {
                        ResponseBuilder(self->downstream_)
                            .status(404, "Not found")
                            .body(fmt::format("Object not found"))
                            .sendWithEOM();
                    }
                    else
                    {
                        ResponseBuilder(self->downstream_)
                            .status(200, "OK")
                            .sendWithEOM();
                    }
                    self->finished_ = true;
                                            });

        });
}

void S3Handler::readObject(folly::EventBase *evb, std::shared_ptr<S3Handler> self, int64_t offset)
{
    const size_t bufsize = 32768;
    folly::IOBufQueue buf;

    bool did_pause = false;
    bool has_error = false;
    while(offset < put_remaining.load(std::memory_order_relaxed))
    {
        if (self->paused_)
        {
            XLOGF(DBG0, "Sending of {} paused at {} done bytes. Finished={} Running={}", self->fpath, self->done_bytes, self->finished_, self->running);
            did_pause = true;
            break;
        }

        auto it = std::upper_bound(extents.begin(), extents.end(), SingleFileStorage::Ext(offset, 0, 0));
        if(!extents.empty())
            --it;
        assert(it != extents.end());
        if(it==extents.end())
            break;

        assert(it->obj_offset <= offset && it->obj_offset + it->len > offset);
        
        int64_t ext_offset = offset - it->obj_offset;
        auto curr_ext = SingleFileStorage::Ext(it->obj_offset + ext_offset, it->data_file_offset + ext_offset, it->len - ext_offset);
        int64_t rlen = std::min(static_cast<int64_t>(bufsize), curr_ext.len);

        auto res = sfs.read_ext(curr_ext, 0, bufsize, buf);

        if(res.err!=0)
        {
            XLOGF(WARN, "Error reading extent code {}", res.err);
            evb->runInEventBaseThread([self = self]() mutable
                                      {
                self->downstream_->sendAbort();
                self->finished_ = true;
                self->running = false;
                                      } );
            has_error = true;
            break;
        }

        offset += res.buf->length();
    
        XLOGF(DBG0, "Sending body len {} of fpath {} total_len {}", res.buf->length(), self->fpath, put_remaining.load(std::memory_order_relaxed));

        evb->runInEventBaseThread([self = self, body = std::move(res.buf), total_len = put_remaining.load(std::memory_order_relaxed)]() mutable
                                      {
            if(self->finished_)
                return;

            self->done_bytes += body->length();
            auto resp = std::move(ResponseBuilder(self->downstream_).body(std::move(body)));
            if(self->done_bytes == total_len)
            {
                resp.sendWithEOM();
                self->finished_ = true;
                self->running = false;
            }
            else
            {
                resp.send();
            }
                                      });


    }

    if(!has_error && offset < put_remaining.load(std::memory_order_relaxed))
    {
        evb->runInEventBaseThread([self = self, did_pause]
                                {
            if(self->finished_)
            {
                auto rc = self->sfs.read_finalize(self->fpath, self->extents, 0);
                assert(rc==0);
                return;
            }

            XLOG(DBG0) << "Setting running=false";
            self->running = false;

            if (did_pause)
            {
                XLOG(DBG0) << "Resuming deferred readObject";
                if(!self->paused_ && !self->running)
                {
                    XLOG(DBG0) << "Was unpaused. Resuming.";
                    self->onEgressResumed();
                }
            }

                                });
    }
    else
    {
        auto rc = sfs.read_finalize(self->fpath, self->extents, 0);
        assert(rc==0);
    }
}

void S3Handler::listObjects(folly::EventBase *evb, std::shared_ptr<S3Handler> self, const std::string& marker, int max_keys, const std::string& prefix, const std::string& delimiter, const std::string& bucket)
{
    SingleFileStorage::IterData iter_data = {};
    if(!sfs.iter_start(bucket + "/" + marker, false, iter_data))
    {
        evb->runInEventBaseThread([self = self]()
                                              {
                        ResponseBuilder(self->downstream_)
                            .status(500, "Internal error")
                            .body(fmt::format("Error listing"))
                            .sendWithEOM(); });
        return;
    }

    std::string val_data;

    int i;
    bool truncated = true;
    for(i=0;i<max_keys;++i)
    {
        std::string key, md5sum;
        int64_t offset, size, last_modified;
        std::vector<SingleFileStorage::SPunchItem> extra_exts;
        if(!sfs.iter_curr_val(key, offset, size, extra_exts, last_modified, md5sum, iter_data))
        {
            truncated = false;
            break;
        }

        for(const auto& ext: extra_exts)
        {
            size += ext.len;
        }

        // Remove bucket name
        auto slash_idx = key.find('/');
        if(slash_idx != std::string::npos)
            key = key.substr(slash_idx+1);

        val_data += fmt::format("\t<Contents>\n"
            "\t\t<Key>{}</Key>\n"
            "\t\t<LastModified>2009-10-12T17:50:30.000Z</LastModified>\n"
            "\t\t<ETag>\"{}\"</ETag>\n"
            "\t\t<Size>{}</Size>\n"
            "\t\t<StorageClass>STANDARD</StorageClass>\n"
            "\t\t<Owner>\n"
            "\t\t\t<ID>75aa57f09aa0c8caeab4f8c24e99d10f8e7faeebf76c078efc7c6caea54ba06a</ID>\n"
            "\t\t\t<DisplayName>mtd@amazon.com</DisplayName>\n"
            "\t\t</Owner>\n"
            "\t</Contents>", key, folly::hexlify(md5sum), size);

        if(!sfs.iter_next(iter_data))
        {
            evb->runInEventBaseThread([self = self]()
                                              {
                        ResponseBuilder(self->downstream_)
                            .status(500, "Internal error")
                            .body(fmt::format("Error listing (in iteration)"))
                            .sendWithEOM(); });
            sfs.iter_stop(iter_data);
            return;
        }
    }

    std::string next_maker;
    if(truncated)
    {
        std::string data;
        sfs.iter_curr_val(next_maker, data, iter_data);

        // Remove bucket name
        auto slash_idx = next_maker.find('/');
        if(slash_idx != std::string::npos)
            next_maker = next_maker.substr(slash_idx+1);
    }

    sfs.iter_stop(iter_data);

    std::string resp = fmt::format("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<ListBucketResult>\n"
        "\t<IsTruncated>{}</IsTruncated>\n"
        "\t<Marker>{}</Marker>\n"
        "\t<MaxKeys>{}</MaxKeys>\n"
        "\t<Delimiter>{}</Delimiter>\n"
        "\t<NextMarker>{}</NextMarker>\n"
        "{}"
        "</ListBucketResult>", truncated ? "true" : "false", marker, max_keys, delimiter, next_maker, val_data);

     evb->runInEventBaseThread([self = self, resp = std::move(resp)]()
                                              {
                        ResponseBuilder(self->downstream_)
                            .status(200, "OK")
                            .body(resp)
                            .sendWithEOM(); });
}

void S3Handler::onEgressPaused() noexcept
{
    // This will terminate readFile soon
    XLOG(DBG0) << "S3Handler paused";
    paused_ = true;
}

void S3Handler::onEgressResumed() noexcept
{
    XLOG(DBG0) << "S3Handler resumed";
    paused_ = false;
    // If readFileScheduled_, it will reschedule itself
    if (!running && !fpath.empty() && !finished_)
    {
        running = true;
        XLOGF(DBG0, "Starting readObject of {} offset {}", fpath, done_bytes);
        folly::getGlobalCPUExecutor()->add(
            [self = self, evb = folly::EventBaseManager::get()->getEventBase(), offset = done_bytes]()
            {
                std::string fpath = self->fpath;
                self->readObject(evb, std::move(self), offset);
            });
    }
    else
    {
        XLOGF(DBG0, "Deferred scheduling readFile finished={}", finished_);
    }
}

void S3Handler::onBody(std::unique_ptr<folly::IOBuf> body) noexcept
{
    auto evb = folly::EventBaseManager::get()->getEventBase();

    size_t body_bytes = body->length();

    folly::getGlobalCPUExecutor()->add(
        [self = this->self, evb, offset = done_bytes, lbody = std::move(body)]() mutable
        {
            self->onBodyCPU(evb, offset, std::move(lbody));
        });

    done_bytes += body_bytes;
}

void S3Handler::onBodyCPU(folly::EventBase *evb, int64_t offset, std::unique_ptr<folly::IOBuf> body)
{
    {
        std::unique_lock lock(extents_mutex);
        while (extents.empty() && !finished_)
        {
            extents_cond.wait(lock);
        }

        if (finished_)
        {
            return;
        }
    }

    if(extents.size()>1)
    {
        assert(extents[0] < extents[1]);
    }

    const uint8_t *data = body->data();
    size_t data_size = body->length();
    while(data_size > 0)
    {
        auto it = std::upper_bound(extents.begin(), extents.end(), SingleFileStorage::Ext(offset, 0, 0));
        if(!extents.empty())
            --it;
        assert(it != extents.end());
        if(it==extents.end())
            break;

        if(!(it->obj_offset <= offset && it->obj_offset + it->len > offset))
        {
            XLOGF(DBG0, "Selected ext obj_offset={} len={} data_file_offset={} offset={} exts={}", it->obj_offset, it->len, it->data_file_offset, offset, extents.size());
            std::sort(extents.begin(), extents.end());
            auto it2= std::upper_bound(extents.begin(), extents.end(), SingleFileStorage::Ext(offset, 0, 0));
            XLOGF(DBG0, "Selected ext obj_offset={} len={} data_file_offset={} offset={} exts={}", it2->obj_offset, it2->len, it2->data_file_offset, offset, extents.size());
            break;
        }
        assert(it->obj_offset <= offset && it->obj_offset + it->len > offset);
        
        int64_t ext_offset = offset - it->obj_offset;
        auto curr_ext = SingleFileStorage::Ext(it->obj_offset + ext_offset, it->data_file_offset + ext_offset, it->len - ext_offset);
        int64_t wlen = std::min(static_cast<int64_t>(data_size), curr_ext.len);

        auto rc = sfs.write_ext(curr_ext, data, data_size);
        if (rc != 0)
        {
            evb->runInEventBaseThread([self = self]()
                                  {           
                    ResponseBuilder(self->downstream_)
                        .status(500, "Internal error")
                        .body("Write ext error")
                        .sendWithEOM();
                    self->finished_ = true; });
            return;
        }

        data += wlen;
        data_size -= wlen;
        offset += wlen;
    }

    assert(data_size == 0);

    if (put_remaining.fetch_sub(body->length(), std::memory_order_release) == body->length())
    {
        auto rc = sfs.write_finalize(fpath, extents, 0, std::string(), false, true);

        if (rc != 0)
        {
            evb->runInEventBaseThread([self = self]()
                                  {           
                    ResponseBuilder(self->downstream_)
                        .status(500, "Internal error")
                        .body("Write finalization error")
                        .sendWithEOM();
                    self->finished_ = true; });
            return;
        }

        if(!sfs.get_manual_commit())
        {
            bool b = sfs.commit(false, -1);
            
            if(!b)
            {
                evb->runInEventBaseThread([self = self]()
                                    {           
                        ResponseBuilder(self->downstream_)
                            .status(500, "Internal error")
                            .body("Commit error")
                            .sendWithEOM();
                        self->finished_ = true; });
                return;
            }
        }

        evb->runInEventBaseThread([self = self]()
                                  {           
                    ResponseBuilder(self->downstream_)
                        .status(200, "OK")
                        .sendWithEOM();
                    self->finished_ = true; });
    }
}

void S3Handler::onEOM() noexcept {}

void S3Handler::onUpgrade(UpgradeProtocol /*protocol*/) noexcept
{
    // handler doesn't support upgrades
}

void S3Handler::requestComplete() noexcept
{
    XLOG(DBG0, "Request complete");
    finished_ = true;
    paused_ = true;
    self.reset();
}

void S3Handler::onError(ProxygenError /*err*/) noexcept
{
    XLOG(DBG0, "onError");
    finished_ = true;
    paused_ = true;

    if (request_type == RequestType::PutObject)
    {
        // TODO: Free extents
    }

    self.reset();
}