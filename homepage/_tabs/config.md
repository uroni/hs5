---
# the default layout is 'page'
icon: fas fa-cog
order: 4
title: Configuration
---

HS5 can be configured either via config file, environment variables or command line arguments. The configuration file
can be specified via `-c` command line argument pointing at the configuration file.

Options available in the configuration file:

Use `KEY=VALUE` format (one option per line). Lines starting with `#` are treated as comments.

Example:

```ini
HTTP_PORT=8085
SERVER_IP=0.0.0.0
DATA_STORAGE_PATH=/var/lib/hs5/data
METADATA_STORAGE_PATH=/var/lib/hs5/meta
LOGLEVEL=info
```

<style>
  .config-table td:first-child,
  .config-table th:first-child,
  .config-table td:nth-child(3),
  .config-table th:nth-child(3) {
    white-space: normal;
  }
  .config-table td:nth-child(3) {
    min-width: 9rem;
  }
  .config-table td:nth-child(3) code {
    white-space: nowrap;
  }
  .config-table td:last-child,
  .config-table th:last-child {
    white-space: normal;
    min-width: 16rem;
  }
</style>

| Option | Type | Default | Description |
|---|---|---|---|
| `HTTP_PORT` | integer | `80` | HTTP port HS5 listens on. |
| `SERVER_URL` | string | empty | Public/base URL of this server. |
| `DATA_STORAGE_PATH` | path | `.` | Path for HS5 data files. |
| <code class="language-plaintext highlighter-rouge">METADATA_<wbr>STORAGE_<wbr>PATH</code> | path | `.` | Path for HS5 metadata/index files. |
| `SERVER_IP` | string | `0.0.0.0` | Address/hostname HS5 binds to. |
| <code class="language-plaintext highlighter-rouge">INIT_<wbr>ROOT_<wbr>ACCESS_<wbr>KEY</code> | string | `root` | Initial root access key name on first startup. |
| `INIT_ROOT_PASSWORD` | string | auto-generated if unset | Initial root password on first startup. |
| `INIT_CREATE_BUCKET` | string | empty | Comma-separated list of buckets to create at startup. |
| `MANUAL_COMMIT` | boolean | `false` | Enable manual commit mode. Accepted true values: `true`, `yes`, `on`, `1`. |
| <code class="language-plaintext highlighter-rouge">MANUAL_<wbr>COMMIT_<wbr>LIST_<wbr>CONSISTENT</code> | boolean | `true` | Keep list operations fully consistent in manual commit mode (higher latency). |
| `COMMIT_AFTER_MS` | integer (ms) | `30000` | In manual commit mode, automatically commit after this delay. Use `0` to disable auto-commit. |
| <code class="language-plaintext highlighter-rouge">COMMIT_<wbr>MAX_<wbr>DEFER_<wbr>MS</code> | integer (ms) | `10` | Requested commits are deferred while modifications are queued, so they are committed together. This is the maximum deferral under continuous load. Low values (below about 10 ms) cause more commits and lower throughput with concurrent writers. |
| `WORKER_THREADS` | integer | `2 x CPU cores` | Number of global worker threads. |
| `HTTP_WORKER_THREADS` | integer | CPU core count | Number of HTTP worker threads. |
| <code class="language-plaintext highlighter-rouge">GET_<wbr>IN_<wbr>WORKER_<wbr>THRESHOLD</code> | integer (bytes) | `0` | Serve GET/HEAD requests in the HTTP worker thread and read objects smaller than this via mmap there. Lowers latency for small objects; `0` disables. |
| `LOGLEVEL` | enum | `info` | Log level. Allowed values: `debug`, `info`, `warn`, `error`. |
| `STOP_ON_ERROR` | boolean | `false` | Stop HS5 on write/read errors instead of continuing. |
| `DISABLE_MD5_ETAG` | boolean | `false` | Use a unique random value as ETag instead of the MD5 of the object data. Saves CPU on uploads, but ETags are no longer content MD5s (not S3 compatible for clients that verify them). |
| `PREALLOCATE_DATA` | boolean | `true` | Preallocate space in the data file to improve write performance and reduce fragmentation. Automatically disabled on Btrfs. |
| `TRIM_FREESPACE_SIZE` | integer (bytes) | `1048576` | If contiguous free space in the data file exceeds this size, HS5 can punch holes to free disk space. Set to `-1` to disable. |
| `DB_MAX_DIRTY_MB` | integer (MiB) | `50` | Maximum LMDB dirty page budget before flush. |
| `WAL_MODE` | enum | `disabled` | Performance WAL mode. Allowed values: `disabled`, `metadata-only`, `data-only`, `all-data-only`, `full`, `full-all-data`. |
| `WAL_PATH` | path | metadata path | Path where WAL files are written. |
| <code class="language-plaintext highlighter-rouge">WAL_<wbr>SMALL_<wbr>OBJECT_<wbr>LIMIT</code> | integer (bytes) | `6000` | In `full` or `data-only` WAL mode, object size threshold for writing object data to WAL. |
| `WAL_SIZE_MB` | integer (MiB) | `1000` | WAL file size threshold before reset/rollover. |
| `WAL_ITEMS` | integer | `10000` | WAL item count threshold before reset/rollover. |
| <code class="language-plaintext highlighter-rouge">WAL_<wbr>COMMIT_<wbr>SYNC</code> | boolean | `true` | Sync the WAL file on each commit. With `false`, commits only wait until the WAL is written to the OS: they survive a crash of HS5, but recent commits can be lost on an OS crash or power loss. The index stays consistent, as full index commits are still synced. Only has an effect with `WAL_MODE` enabled. |
| `REDIS_PORT` | integer | `-1` | Port for the Redis protocol (RESP2/RESP3) server, bound to `SERVER_IP`. `-1` disables it. Clients authenticate with `AUTH <access key> <secret key>`; access is checked against the S3 permissions of `REDIS_BUCKET`. |
| `REDIS_BUCKET` | string | `redis` | Bucket storing the Redis keys as objects. Created with versioning enabled if it does not exist. Versioning has to be enabled, as list elements are object versions. |
| <code class="language-plaintext highlighter-rouge">REDIS_<wbr>MAX_<wbr>VALUE_<wbr>SIZE</code> | integer (bytes) | `536870912` | Maximum size of values written or read via the Redis protocol. Larger objects can only be accessed via S3. |
| <code class="language-plaintext highlighter-rouge">REDIS_<wbr>SCRIPT_<wbr>TIMEOUT_<wbr>MS</code> | integer (ms) | `5000` | Lua scripts (`EVAL`/`EVALSHA`) running longer than this are aborted. |
| <code class="language-plaintext highlighter-rouge">REDIS_<wbr>SCRIPT_<wbr>ALWAYS_<wbr>GLOBAL</code> | boolean | `false` | Run all Lua scripts exclusively (no other Redis command runs concurrently, like a single-threaded Redis server). By default scripts only lock the keys they declare; scripts accessing undeclared keys are detected and run exclusively. |
| <code class="language-plaintext highlighter-rouge">REDIS_<wbr>SCRIPT_<wbr>ALLOW_<wbr>UNDECLARED_<wbr>KEYS</code> | boolean | `false` | Allow Lua scripts to access undeclared keys after they modified data (without atomicity). By default such scripts fail. |
| <code class="language-plaintext highlighter-rouge">REDIS_<wbr>COLLECTION_<wbr>INLINE_<wbr>MAX_<wbr>ENTRIES</code> | integer | `128` | Redis hashes, sets and sorted sets with at most this many elements are stored inline in the metadata of their key. Larger ones are stored with one entry per element, so modifying an element does not rewrite the whole collection. |
| <code class="language-plaintext highlighter-rouge">REDIS_<wbr>COLLECTION_<wbr>INLINE_<wbr>MAX_<wbr>BYTES</code> | integer (bytes) | `8192` | Maximum serialized size of Redis hashes, sets and sorted sets stored inline. |
| `AMQP_PORT` | integer | `-1` | Port for the AMQP 0-9-1 server (RabbitMQ protocol, e.g. for Celery), bound to `SERVER_IP`. `-1` disables it. Clients log in with an access key as user name and the secret key as password; access is checked against the S3 permissions of `AMQP_BUCKET`. |
| `AMQP_BUCKET` | string | `amqp` | Bucket storing the AMQP queues (messages are object versions) and durable definitions. Created with versioning enabled if it does not exist. |
| <code class="language-plaintext highlighter-rouge">AMQP_<wbr>MAX_<wbr>MESSAGE_<wbr>SIZE</code> | integer (bytes) | `134217728` (128 MiB) | Maximum size of AMQP message bodies. Larger messages are sent in several frames of at most `AMQP_FRAME_MAX`. |
| <code class="language-plaintext highlighter-rouge">AMQP_<wbr>FRAME_<wbr>MAX</code> | integer (bytes) | `131072` (128 KiB) | Maximum AMQP frame size offered to clients. |
| `AMQP_HEARTBEAT` | integer (seconds) | `60` | AMQP heartbeat interval offered to clients. `0` disables heartbeats. |
| <code class="language-plaintext highlighter-rouge">AMQP_<wbr>MEMORY_<wbr>MESSAGE_<wbr>MAX</code> | integer (bytes) | `65536` (64 KiB) | AMQP messages with bodies up to this size are also kept in memory and delivered from there instead of being read from storage. `0` disables it. |
| <code class="language-plaintext highlighter-rouge">AMQP_<wbr>MEMORY_<wbr>MAX_<wbr>MB</code> | integer (MiB) | `256` | Maximum memory used by AMQP messages kept in memory. Further messages are delivered from storage. |
{: .config-table }

HS5 can also read these options from environment variables using the `HS5_` prefix (for example,
`HS5_HTTP_PORT`). Option precedence is: config file values first, then environment variables, then command line arguments.

## Tuning for best performance

- Enable `MANUAL_COMMIT` for write-heavy workloads when you can tolerate delayed visibility/durability. This reduces commit overhead, but recent writes are not fully durable until commit (or `COMMIT_AFTER_MS`) runs.
- Enable `WAL_MODE` only when `MANUAL_COMMIT` is disabled, and prefer placing `WAL_PATH` on storage that is at least as fast as `METADATA_STORAGE_PATH`.
- Set `TRIM_FREESPACE_SIZE=-1` to disable hole-punch trimming when optimizing for throughput over disk-space reclamation.
- Increase `DB_MAX_DIRTY_MB` if you have available RAM and want fewer flushes under sustained writes in `MANUAL_COMMIT` mode.
- Increase `WORKER_THREADS` above the default when storage is fast and CPU is not saturated; raise gradually while monitoring throughput and p95/p99 latency, since too many threads can increase contention and hurt performance.
- Set `DISABLE_MD5_ETAG=true` to skip computing the MD5 of uploaded data. This only saves CPU (roughly 1–1.5 ms of CPU per MiB uploaded) and does not improve throughput unless HS5 is CPU-bound on uploads. Trade-off: ETags are no longer the MD5 of the object data, which breaks clients that verify ETags against content.
