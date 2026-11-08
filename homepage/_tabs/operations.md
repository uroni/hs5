---
# the default layout is 'page'
icon: fas fa-tasks
order: 5
title: S3 operations
---

HS5 implements the following AWS S3 API operations. Requests are authenticated with AWS Signature Version 4
(header-based, presigned URLs and `aws-chunked` streaming payloads). Presigned URLs using AWS Signature Version 2
are also accepted. Operations not listed here are not supported.

<style>
  .ops-table-wrap td:nth-child(2),
  .ops-table-wrap th:nth-child(2) {
    white-space: normal;
    min-width: 12rem;
  }
  .ops-table td:last-child,
  .ops-table th:last-child {
    white-space: normal;
    min-width: 16rem;
  }
</style>

## Buckets

| Operation | Request | Notes |
|---|---|---|
| `ListBuckets` | `GET /` | Only buckets the caller is allowed to list are returned. |
| `CreateBucket` | `PUT /{bucket}` | Supports `x-amz-bucket-object-lock-enabled`. |
| `DeleteBucket` | `DELETE /{bucket}` | |
| `HeadBucket` | `HEAD /{bucket}` | |
| `GetBucketLocation` | `GET /{bucket}?location` | |
| `GetBucketVersioning` | `GET /{bucket}?versioning` | |
| `PutBucketVersioning` | `PUT /{bucket}?versioning` | |
| `GetBucketCors` | `GET /{bucket}?cors` | |
| `PutBucketCors` | `PUT /{bucket}?cors` | At most 100 rules. CORS preflight (`OPTIONS`) requests are answered based on this configuration. |
| `GetObjectLockConfiguration` | `GET /{bucket}?object-lock` | |
| `PutObjectLockConfiguration` | `PUT /{bucket}?object-lock` | |
{: .ops-table }

## Listing

| Operation | Request | Notes |
|---|---|---|
| `ListObjects` | `GET /{bucket}` | Supports `prefix`, `delimiter`, `marker`, `max-keys` and `encoding-type`. Delimiter must be a single character. |
| `ListObjectsV2` | `GET /{bucket}?list-type=2` | Supports `prefix`, `delimiter`, `continuation-token`, `start-after`, `max-keys` and `encoding-type`. Delimiter must be a single character. |
| `ListObjectVersions` | `GET /{bucket}?versions` | |
| `ListMultipartUploads` | `GET /{bucket}?uploads` | |
{: .ops-table }

## Objects

| Operation | Request | Notes |
|---|---|---|
| `PutObject` | `PUT /{bucket}/{key}` | Supports user metadata, `Content-MD5`, conditional writes (`If-Match`, `If-None-Match`), object lock headers and additional checksums (CRC32, CRC32C, CRC64NVME, SHA1, SHA256). |
| `GetObject` | `GET /{bucket}/{key}` | Supports `Range`, `partNumber`, `versionId`, conditional headers (`If-Match`, `If-None-Match`, `If-Modified-Since`, `If-Unmodified-Since`), `response-*` header overrides and `x-amz-checksum-mode`. |
| `HeadObject` | `HEAD /{bucket}/{key}` | Same options as `GetObject`. |
| `GetObjectAttributes` | `GET /{bucket}/{key}?attributes` | |
| `CopyObject` | `PUT /{bucket}/{key}` with `x-amz-copy-source` | Supports `x-amz-metadata-directive` and `x-amz-copy-source-if-*` conditions. Maximum source size 5 GiB. |
| `DeleteObject` | `DELETE /{bucket}/{key}` | Supports `versionId` and `x-amz-bypass-governance-retention`. |
| `DeleteObjects` | `POST /{bucket}?delete` | At most 1000 keys per request. |
| `PostObject` | `POST /{bucket}` (HTML form) | Browser-based uploads with POST policy. |
| `PutObjectLegalHold` | `PUT /{bucket}/{key}?legal-hold` | |
| `PutObjectRetention` | `PUT /{bucket}/{key}?retention` | Supports `x-amz-bypass-governance-retention`. |
{: .ops-table }

## Multipart uploads

| Operation | Request | Notes |
|---|---|---|
| `CreateMultipartUpload` | `POST /{bucket}/{key}?uploads` | Supports user metadata, object lock headers and `x-amz-checksum-algorithm` / `x-amz-checksum-type`. |
| `UploadPart` | <code class="language-plaintext highlighter-rouge">PUT /{bucket}/{key}<wbr>?partNumber=N<wbr>&amp;uploadId=ID</code> | Supports `Content-MD5` and additional checksums. |
| `UploadPartCopy` | <code class="language-plaintext highlighter-rouge">PUT /{bucket}/{key}<wbr>?partNumber=N<wbr>&amp;uploadId=ID</code> with `x-amz-copy-source` | Supports `x-amz-copy-source-range` and `x-amz-copy-source-if-*` conditions. |
| `CompleteMultipartUpload` | `POST /{bucket}/{key}?uploadId=ID` | |
| `AbortMultipartUpload` | `DELETE /{bucket}/{key}?uploadId=ID` | |
{: .ops-table .ops-table-wrap }
