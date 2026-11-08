---
# the default layout is 'page'
icon: fas fa-history
order: 7
title: Changelog
---

Release notes for all HS5 releases. Binaries are available on the [GitHub releases page](https://github.com/uroni/hs5/releases).

## 1.4.0

*Released 2026-11-08* · [GitHub release](https://github.com/uroni/hs5/releases/tag/1.4.0)

{% raw %}
### New features

 * [Add DISABLE_MD5_ETAG option to use a unique random value instead of the content MD5 as ETag](https://github.com/uroni/hs5/commit/bef4af2cd890c07e31227af839f27bfa4e1fb51a)
 * [Add PREALLOCATE_DATA config and env setting](https://github.com/uroni/hs5/commit/59c020ad94e359046af2941cbe3cd861ba9a4f0e)
 * [Support CopySource VersionId and URL-encoded copy sources](https://github.com/uroni/hs5/commit/eb02b4cae735ae717447774c5cab06ce1dd38d78)
 * [Return x-amz-copy-source-version-id for copies](https://github.com/uroni/hs5/commit/f6795d148d1a31172e6db6ba640b8f8cb90bf515)
 * [Send x-amz-mp-parts-count for partNumber requests](https://github.com/uroni/hs5/commit/7aebb4d941b99a48641de8641699b56dd5ef7690)

### Changes

 * [Create delete marker when deleting missing key in versioned bucket](https://github.com/uroni/hs5/commit/ddac78137de6bacae144dd860d5f9752fd5cfb8b)
 * [Pad WAL data writes to full pages and clamp used space](https://github.com/uroni/hs5/commit/5a264869c033c8874dbec66fba1e4722cc30e60d)

### Bug fixes

 * Range and partNumber GET fixes:
   * [Send Content-Range header on partial GET responses](https://github.com/uroni/hs5/commit/4730a2af5e02f2b10a9f286cf378e698e5bca486)
   * [Return 206 Partial Content with Content-Range for partNumber GETs](https://github.com/uroni/hs5/commit/57a34362986897d566b0a390a35715ba2058db24)
 * Versioning fixes:
   * [Fix IsLatest and version marker paging in ListObjectVersions](https://github.com/uroni/hs5/commit/47c81cde71565cc81a0eb0c6a2f919e013bd1af5)
   * [Fix object size in ListObjectVersions](https://github.com/uroni/hs5/commit/be56978dc804dcccfc64360496c1c350d51da7da)
   * [Return 400 for invalid version id and check DELETE signature first](https://github.com/uroni/hs5/commit/c9f97215a5e5559235d2d0c09f73d51e318c03f8)
   * [Return InvalidRequest when copying a delete marker by version id](https://github.com/uroni/hs5/commit/1b30ba035bc694863e7939ff1c4e1ca51a440a07)
 * [Check copy source permission in CopyObject](https://github.com/uroni/hs5/commit/eb02b4cae735ae717447774c5cab06ce1dd38d78)
 * Multipart upload fixes:
   * [Fix crash in GetObjectAttributes with ObjectParts on non-multipart object](https://github.com/uroni/hs5/commit/a99c7f8b501ce8bdac2e994822da11db69d9e838)
   * [Fix parsing part keys with '.' in upload id](https://github.com/uroni/hs5/commit/cc34e10e44eb5e26ca6ad437ce470651d490be54)
   * [Reject part numbers outside 1..10000 in CompleteMultipartUpload](https://github.com/uroni/hs5/commit/badd208a14d0ef307e718ac24c966693e2ad1430)
 * Fix data races and thread-safety issues found with clang thread safety analysis and TSan:
   * [Skip HTTP thread callbacks after the request finished (fixes use of downstream after error)](https://github.com/uroni/hs5/commit/2a0766ded7996dea45e24c9b902f67fca8fa94d4)
   * [Fix data race on data_file_max_size in write_prepare](https://github.com/uroni/hs5/commit/1495fee4acbb12cac722022c38f4e0013cf1edfb)

### Build

 * Add clang thread safety annotations to [WalFile](https://github.com/uroni/hs5/commit/3c770a1ff2a366c12d736169fb02cd6e5e9e5b10), [S3Handler](https://github.com/uroni/hs5/commit/f7397a48b130e4ce1adb1e6d7d16037fca77cc5e), [ApiHandler](https://github.com/uroni/hs5/commit/ad50d9a0136ecc78f842844e35f50d72bc97939f) and [StaticHandler](https://github.com/uroni/hs5/commit/9e7253ed67ebfda65b558287abddfe752153d668)
 * [Suppress TSan reports for LMDB lock-free reader table](https://github.com/uroni/hs5/commit/f431a65e54b3a9c1b4bea399ee41ceaa539d44ab)

**Full Changelog**: https://github.com/uroni/hs5/compare/1.3.1...1.4.0
{% endraw %}

## 1.3.1

*Released 2026-11-02* · [GitHub release](https://github.com/uroni/hs5/releases/tag/1.3.1)

{% raw %}
### New features

 * [Add static musl build with zig](https://github.com/uroni/hs5/commit/7c821a5bcba08f2c04b579b01273392a30b7e751)
 * [Support ETag conditions in DeleteObjects](https://github.com/uroni/hs5/commit/676f628d162d31abf6cef4afe1109b73729d124a)
 * [Report delete markers in DeleteObject and DeleteObjects responses](https://github.com/uroni/hs5/commit/47ef1c159bd43843d2e57ac8101fa0f609746c2a)

### Changes

 * [Rename release binaries to hs5-amd64-glibc.xz and hs5-arm64-glibc.xz](https://github.com/uroni/hs5/commit/2792e07d96928ee54f71add456091f4a6e5f2d14)
 * [Use musl build in README and website](https://github.com/uroni/hs5/commit/5306060c506ac49d846c61fe0fca4a8321202935)
 * [Accept unquoted ETags in conditional headers](https://github.com/uroni/hs5/commit/35634ca9c0e23ec2f9f4dc0ecdaf80c768f52703)
 * [Avoid commit thread wait for delete markers in DeleteObjects](https://github.com/uroni/hs5/commit/442605eda0d73e4c45b761657db45e97e731f5c3)

### Bug fixes

 * Conditional request fixes:
   * [Return 304 Not Modified instead of 412 Precondition Failed for failed If-None-Match/If-Modified-Since on GET/HEAD](https://github.com/uroni/hs5/commit/33548df5fdf0868700043be5dbe5576ecba02f49)
   * [Return 404 Not Found instead of 412 Precondition Failed for If-Match writes on missing objects](https://github.com/uroni/hs5/commit/9f50d03e27b8c1ee8cac091dcb05e07deb6a7f18)
   * [Check current version in conditional write precheck](https://github.com/uroni/hs5/commit/45006200ec0def336b1c94b39602941246b74afa)
   * [Read copy source conditions from x-amz-copy-source-if-* headers](https://github.com/uroni/hs5/commit/f1e7c144a7f816bf3a45d4d121718c019d60d5b3)
   * [Return 412 Precondition Failed instead of 204 No Content for If-Match deletes on missing objects in versioned buckets](https://github.com/uroni/hs5/commit/aaf9706f70df898c134ee12def2a328230855e81)
 * [Return names for all actions in actionToStr](https://github.com/uroni/hs5/commit/0419e8edafad39ad74c82a76dc7564b15d68be16)

### Build

 * [Mark DuckDbFs::GetFileSize as override](https://github.com/uroni/hs5/commit/13d598049f24a88728eb729ada37d6a47483a80d)

**Full Changelog**: https://github.com/uroni/hs5/compare/1.3.0...1.3.1
{% endraw %}

## 1.3.0

*Released 2026-10-26* · [GitHub release](https://github.com/uroni/hs5/releases/tag/1.3.0)

{% raw %}
### New features

 * Object lock support:
   * [Implement object lock metadata](https://github.com/uroni/hs5/commit/6945c0bf700b50b94ebdb93db24af1a0bec944b5)
   * [Implement PutObjectLegalHold](https://github.com/uroni/hs5/commit/5e3c11b57e01064f9811e63655095849de5f3798)
   * [Implement PutObjectRetention](https://github.com/uroni/hs5/commit/a76367d35639896d1033fdec3e759be88c3c6c7e)
   * [Add GetObjectLockConfiguration](https://github.com/uroni/hs5/commit/8abeee3cac298497d37ffab976366083f99525c2)
   * [Support object lock headers on PutObject, CopyObject and CreateMultipartUpload](https://github.com/uroni/hs5/commit/1550a28e94562ee79d502489626786025f066129)
   * [Fail operations on objects protected by legal hold](https://github.com/uroni/hs5/commit/7b08aef803148aa9d17d730559bdb7fc12578822)
 * [Return x-amz-version-id from GetObject and HeadObject](https://github.com/uroni/hs5/commit/9b1331214135b6394ebbd1a835dadfaa0b64804c)
 * [Return x-amz-version-id from CompleteMultipartUpload](https://github.com/uroni/hs5/commit/793c5b4796427081deaeae5a7c17639f77a5f54b)
 * [Add CORS actions to policies and omit empty CORS rule fields](https://github.com/uroni/hs5/commit/3ac93b80f8069b9a5ec1c0bfc73c82402286c73a)
 * [Optionally serve small GET requests in the HTTP worker via mmap](https://github.com/uroni/hs5/commit/496a67991dad232e8876cde515823eb797cafb30)
 * [Use deterministic AES encryption for ids and shorter upload ids](https://github.com/uroni/hs5/commit/52ddcfb0aaa0f37e6af37ff5bf7b040978b9413b)

### Changes

 * [Let config file settings take precedence over HS5_ environment variables](https://github.com/uroni/hs5/commit/34b87535bd5adf24cea6577bd92ef74389ad3129)
 * [Use key form field as object name in PostObject](https://github.com/uroni/hs5/commit/07e84c3cd3a69710afa708dda95ebfdf3a8038d1)

### Bug fixes

 * [Fix PutBucketCors](https://github.com/uroni/hs5/commit/bb3005e9b04288969068012965c48debe9780a4f)
 * [Fix deleting specific versions in DeleteObjects](https://github.com/uroni/hs5/commit/b8e41e75cb081bcf0aef8b0ce522c9809573bbe8)
 * [Use separate match info per object in DeleteObjects](https://github.com/uroni/hs5/commit/5b16a9ac9ba3899959fcd0d2da353a02541f012f)
 * [Return 400 KeyTooLongError instead of 500 for too long keys](https://github.com/uroni/hs5/commit/943ce6cf541f756ff6d365242092b25e02e72c63)
 * [Return 400 instead of 500 for invalid base64 checksum header](https://github.com/uroni/hs5/commit/87056a341d3b6b0314edc4ea11ff7bfe88696aab)
 * Fix data races and thread-safety issues found with TSan/ASan/UBSan:
   * [Fix data race in readObject pause log](https://github.com/uroni/hs5/commit/72d5532d8c69413f363c6cf072220006b50f467b)
   * [Read data file offsets under lock in freespace_check](https://github.com/uroni/hs5/commit/1a5c96279934569cdfb697c1e276fc08382f3b38)
   * [Free prepared extents if request finished during write_prepare](https://github.com/uroni/hs5/commit/6ba0435181ef2f5b82dd71681c4332554b60db53)
   * [Set finished_ under lock if necessary](https://github.com/uroni/hs5/commit/6871953046bfb8472de2a7c7db6d331cf930c0c8)
   * [Don't modify keyInfo in CPU thread](https://github.com/uroni/hs5/commit/18085275aa1fb8c22829dc50286d3342fc6580be)
   * [Delete multi-part objects without holding other mutex](https://github.com/uroni/hs5/commit/08fa60dc89a93a807400b42a7b0d61403f1785fa)

### Build

 * [Bootstrap missing vcpkg checkout in CMake](https://github.com/uroni/hs5/commit/fb1f6976a9976527e5975497c094aecdb0585683)
 * [Add setting to build without duckdb](https://github.com/uroni/hs5/commit/3937836482926f8d30b9af0cab9f3bc7345153cb)
 * [Fall back to date::parse when std::chrono::parse is unavailable](https://github.com/uroni/hs5/commit/573a57ffcadd19e4f47a0f8d3fcdabed86a6d0c0)

**Full Changelog**: https://github.com/uroni/hs5/compare/1.2.0...1.3.0
{% endraw %}

## 1.2.0

*Released 2026-08-01* · [GitHub release](https://github.com/uroni/hs5/releases/tag/1.2.0)

{% raw %}
* [Implement CORS functionality](https://github.com/uroni/hs5/commit/90c8c7c6c8a3418d9ee93d7ef16d5ed84b8eab62)
{% endraw %}

## 1.1.0

*Released 2026-06-21* · [GitHub release](https://github.com/uroni/hs5/releases/tag/1.1.0)

{% raw %}
* [Fix Content-MD5 header being base64 encoded](https://github.com/uroni/hs5/commit/66789a0a9f726e76c8b65229215feb9b88061f1c)
 * [Implement basic form upload to public bucket](https://github.com/uroni/hs5/commit/4367239517cdb949cc4c88aeb0daf3ab9cc8eb7e)
{% endraw %}

## 1.0.0

*Released 2026-04-26* · [GitHub release](https://github.com/uroni/hs5/releases/tag/1.0.0)

{% raw %}
- Add GetObjectAttributes call support
- Fix potential race between multi-part object reading and deletion
- Fix potential race between adding reading item and deletion
{% endraw %}

## 0.33.0

*Released 2026-04-21* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.33.0)

{% raw %}
- Per bucket versioning config and version listing
{% endraw %}

## 0.32.0

*Released 2026-04-19* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.32.0)

- Add support for AWS SDK checksums

## 0.31.0

*Released 2026-04-17* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.31.0)

{% raw %}
* [Persist more header values like Cache-Control](https://github.com/uroni/hs5/commit/e6ec11ddf7780217420428b0dfad0007c86216c5)
{% endraw %}

## 0.30.1

*Released 2026-04-15* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.30.1)

{% raw %}
* Fix DeleteObjects quiet parameter parsing
{% endraw %}

## 0.30.0

*Released 2026-04-12* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.30.0)

- Add `encoding-type` support for object listing
- Fix several ListObjects bugs (KeyCount includes CommonPrefixes, no marker in ListObjectsV2 response)
- Limit the number of objects in a DeleteObjects request

## 0.29.1

*Released 2026-04-08* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.29.1)

{% raw %}
- [Allow setting public permissions via simple permission system](https://github.com/uroni/hs5/commit/8b0426edc774acf670b9d471c484f901db3324a3)
{% endraw %}

## 0.28.1

*Released 2026-04-03* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.28.1)

- Add `stop_on_error` config via environment variable
- Allow disabling data file trimming with `trim_freespace_size<0`
- Enable jemalloc in folly

## 0.28.0

*Released 2026-04-03* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.28.0)

- Environment variables now use the `HS5_` prefix; more settings configurable via environment
- Add LMDB `idl_dirty_max` configuration
- Use jemalloc

## 0.26.0

*Released 2026-03-31* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.26.0)

{% raw %}
- Allow wildcards in action in policy document
 - Add session expiration handling and improve logout process in web UI
{% endraw %}

## 0.25.2

*Released 2026-03-18* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.25.2)

{% raw %}
Ignore case of x-amz-meta prefix
{% endraw %}

## 0.25.1

*Released 2026-03-17* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.25.1)

{% raw %}
Fix metadata handling in CopyObject
{% endraw %}

## 0.25.0

*Released 2026-03-16* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.25.0)

{% raw %}
Add object metadata support
{% endraw %}

## 0.24.1

*Released 2026-03-08* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.24.1)

- Check signature when completing multi-part uploads
- Per-bucket write permission no longer allows CreateBucket

## 0.24.0

*Released 2026-03-08* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.24.0)

- Add simple per-bucket permissions (API and web UI)

## 0.23.0

*Released 2026-03-08* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.23.0)

- ListBuckets only returns buckets the user is authorized for
- Return ids when creating users, roles or policies via API
- Fix SigV2 pre-signed uploads

## 0.22.9

*Released 2026-03-07* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.22.9)

- Fix free space mismatch with WAL after startup
- Don't trim extents that are currently being read

## 0.22.8

*Released 2026-03-06* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.22.8)

- Strip leading slash from copy source header

## 0.22.7

*Released 2026-03-04* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.22.7)

- Fix commit item leaks and persist linked items to WAL

## 0.22.6

*Released 2026-03-04* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.22.6)

- Fix XML parsing and escaping bugs
- Correctly handle DeleteObjects `quiet` parameter

## 0.22.5

*Released 2026-03-04* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.22.5)

- Set default WAL path correctly
- Update S3 explorer in web UI

## 0.22.4

*Released 2026-02-27* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.22.4)

- Fix trimming of larger extents

## 0.22.3

*Released 2026-02-27* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.22.3)

- Fix continuation token in combination with delimiter

## 0.22.2

*Released 2026-02-26* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.22.2)

- Verify ETags in CompleteMultipartUpload

## 0.22.1

*Released 2026-02-26* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.22.1)

- Fix `x-id` check

## 0.22.0

*Released 2026-02-26* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.22.0)

- Trim large contiguous free space from data file
- Faster GET via larger buffer size

## 0.21.0

*Released 2026-02-26* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.21.0)

- Implement CopyObject and UploadPartCopy

## 0.20.1

*Released 2026-02-26* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.20.1)

- Return bucket creation date and region

## 0.20.0

*Released 2026-02-25* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.20.0)

- Implement conditional request headers and conditional multi-part upload completion

## 0.19.0

*Released 2026-02-22* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.19.0)

- Don't interrupt ongoing multi-part downloads when the object is deleted

## 0.18.2

*Released 2026-02-22* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.18.2)

- Fix CommonPrefixes missing the prefix

## 0.18.1

*Released 2026-02-21* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.18.1)

- Listing is consistent in manual commit mode by default

## 0.18.0

*Released 2026-02-21* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.18.0)

- Implement chunked (aws-chunked) upload support

## 0.17.3

*Released 2026-02-20* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.17.3)

- Fix missing lock

## 0.17.2

*Released 2026-02-20* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.17.2)

- Test changes only

## 0.17.1

*Released 2026-02-20* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.17.1)

- Test and benchmark changes only

## 0.17.0

*Released 2026-02-20* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.17.0)

- Add WAL mode that logs only small object data (`wal-small-object-limit`)
- Pre-allocate data file for better performance
- Add worker threads option with a higher default

## 0.16.4

*Released 2026-02-18* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.16.4)

- Don't create buckets with invalid names on startup
- Fix HTTP status code and XML response for bucket deletion

## 0.16.3

*Released 2026-02-18* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.16.3)

- Fix bucket name parsing on deletion
- Set next bucket id correctly on startup

## 0.16.2

*Released 2026-02-18* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.16.2)

- Return XML on access denied
- Improve bucket name parsing

## 0.16.0

*Released 2026-02-18* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.16.0)

- Store Content-Type of uploaded objects
- Reject invalid bucket names
- Fix range requests from end of object and listing continuation
- Return 403 instead of 401

## 0.15.0

*Released 2026-02-18* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.15.0)

- Add `partNumber` parameter support
- Fix HeadBucket and `--storage-path` argument
- Use RFC 1123 format for Last-Modified header
- Store SQLite database in metadata directory

## 0.14.4

*Released 2026-02-16* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.14.4)

- Implement GetLastModifiedTime

## 0.14.3

*Released 2026-02-16* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.14.3)

- Upgrade DuckDB UI extension

## 0.14.2

*Released 2026-02-15* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.14.2)

- Fix bucket deletion via API
- Add manual commit Python context manager

## 0.14.0

*Released 2026-02-15* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.14.0)

- Add ListBuckets

## 0.13.1

*Released 2026-02-15* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.13.1)

- Add role based access control (API and web UI)
- Add logout to web UI
- Fix zero size for multi-part objects in listings

## 0.12.0

*Released 2026-02-13* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.12.0)

- Create initial buckets via parameters

## 0.11.0

*Released 2026-01-22* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.11.0)

- Implement listing and aborting multi-part uploads

## 0.10.0

*Released 2025-12-04* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.10.0)

- URL decode object paths
- Fix S3 explorer in web UI

## 0.9.8

*Released 2025-12-01* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.9.8)

- Fix WAL file header writing

## 0.9.7

*Released 2025-12-01* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.9.7)

- Fix WAL pending items accounting

## 0.9.6

*Released 2025-11-30* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.9.6)

- Fix WAL reset race

## 0.9.5

*Released 2025-11-26* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.9.5)

- WAL fixes (sequence numbers on startup, reset when fully committed)

## 0.9.4

*Released 2025-11-13* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.9.4)

- Fix WAL write ordering

## 0.9.3

*Released 2025-11-11* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.9.3)

- Fix data-only WAL mode setup

## 0.9.2

*Released 2025-11-11* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.9.2)

- Fix build issue

## 0.9.1

*Released 2025-11-11* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.9.1)

- Fix use-after-free issues with range downloads

## 0.9.0

*Released 2025-11-10* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.9.0)

- Add optional WAL logging of object data for performance

## 0.8.1

*Released 2025-10-23* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.8.1)

- DeleteObject returns success if the object does not exist

## 0.8.0

*Released 2025-10-23* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.8.0)

- Improve returned error messages

## 0.7.4

*Released 2025-10-12* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.7.4)

- Improve write error handling and free extents on request failure
- Disable h2c

## 0.7.3

*Released 2025-10-05* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.7.3)

- Return Last-Modified on HEAD and GetObject
- Improve empty payload signature handling
- Add stop-on-error option

## 0.7.2

*Released 2025-09-28* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.7.2)

- Fix multi-part upload detection via `uploads` parameter

## 0.7.1

*Released 2025-09-28* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.7.1)

- Support `UNSIGNED-PAYLOAD` content SHA256

## 0.7.0

*Released 2025-09-28* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.7.0)

- Store upload parts and partial uploads in separate internal buckets

## 0.6.9

*Released 2025-07-16* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.6.9)

- Return correct ETag in listings

## 0.6.8

*Released 2025-07-14* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.6.8)

- Fix default commit interval

## 0.6.7

*Released 2025-07-13* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.6.7)

- Use IP and port for server URL by default

## 0.6.6

*Released 2025-07-12* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.6.6)

- Return correct last modified in listings

## 0.6.5

*Released 2025-07-12* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.6.5)

- Fix returned ETag and `x-id` for UploadPart

## 0.6.4

*Released 2025-07-12* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.6.4)

- Support `x-id` PUT parameter

## 0.6.3

*Released 2025-07-12* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.6.3)

- Remove trailing slash from bucket name

## 0.6.2

*Released 2025-07-12* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.6.2)

- Fix listing with trailing slash in path

## 0.6.1

*Released 2025-07-12* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.6.1)

- Fix DuckDB static linking

## 0.6

*Released 2025-07-12* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.6)

- Commit after 30s by default in manual commit mode

## 0.5

*Released 2025-07-03* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.5)

- Fix multi-part object deletion

## 0.4.3

*Released 2025-06-29* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.4.3)

- Disable minimum free space check

## 0.4.2

*Released 2025-06-29* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.4.2)

- Decrease default minimum free space to zero

## 0.4.1

*Released 2025-06-29* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.4.1)

- Test changes only

## 0.4.0

*Released 2025-06-29* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.4.0)

- DuckDB integration with built-in DuckDB UI (read-only)
- Multi-object delete (WIP)

## 0.2.0

*Released 2025-05-13* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.2.0)

- Add WAL file for faster commits
- Reuse free space in data files
- Increase max key size to 1024

## 0.1.10

*Released 2025-02-25* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.1.10)

- Browse buckets in web UI via S3 explorer

## 0.1.7

*Released 2025-02-20* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.1.7)

- Build and release pipeline changes only

## 0.1.6

*Released 2025-02-20* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.1.6)

- Build for arm64

## 0.1.5

*Released 2025-02-19* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.1.5)

- Add CreateBucket and DeleteBucket
- Bucket versioning off by default

## 0.1.4

*Released 2025-02-16* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.1.4)

- Fix database writes in release builds
- Don't serve static files if an authorization header is present

## 0.1.3

*Released 2025-02-16* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.1.3)

- Get initial root secret key from environment

## 0.1.1

*Released 2025-02-16* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.1.1)

- Read storage paths from environment as fallback

## 0.1

*Released 2025-02-16* · [GitHub release](https://github.com/uroni/hs5/releases/tag/0.1)

- Initial release: S3 API with multi-part uploads, range downloads, pre-signed URLs, ListObjectsV2, object versioning and a web interface

