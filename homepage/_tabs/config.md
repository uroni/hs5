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

| Option | Type | Default | Description |
|---|---|---|---|
| `HTTP_PORT` | integer | `80` | HTTP port HS5 listens on. |
| `SERVER_URL` | string | empty | Public/base URL of this server. |
| `DATA_STORAGE_PATH` | path | `.` | Path for HS5 data files. |
| `METADATA_STORAGE_PATH` | path | `.` | Path for HS5 metadata/index files. |
| `SERVER_IP` | string | `0.0.0.0` | Address/hostname HS5 binds to. |
| `INIT_ROOT_ACCESS_KEY` | string | `root` | Initial root access key name on first startup. |
| `INIT_ROOT_PASSWORD` | string | auto-generated if unset | Initial root password on first startup. |
| `INIT_CREATE_BUCKET` | string | empty | Comma-separated list of buckets to create at startup. |
| `MANUAL_COMMIT` | boolean | `false` | Enable manual commit mode. Accepted true values: `true`, `yes`, `on`, `1`. |
| `MANUAL_COMMIT_LIST_CONSISTENT` | boolean | `true` | Keep list operations fully consistent in manual commit mode (higher latency). |
| `COMMIT_AFTER_MS` | integer (ms) | `30000` | In manual commit mode, automatically commit after this delay. Use `0` to disable auto-commit. |
| `WORKER_THREADS` | integer | `2 x CPU cores` | Number of global worker threads. |
| `HTTP_WORKER_THREADS` | integer | CPU core count | Number of HTTP worker threads. |
| `LOGLEVEL` | enum | `info` | Log level. Allowed values: `debug`, `info`, `warn`, `error`. |
| `STOP_ON_ERROR` | boolean | `false` | Stop HS5 on write/read errors instead of continuing. |
| `TRIM_FREESPACE_SIZE` | integer (bytes) | `1048576` | If contiguous free space in the data file exceeds this size, HS5 can punch holes to free disk space. Set to `-1` to disable. |
| `DB_MAX_DIRTY_MB` | integer (MiB) | `50` | Maximum LMDB dirty page budget before flush. |
| `WAL_MODE` | enum | `disabled` | Performance WAL mode. Allowed values: `disabled`, `metadata-only`, `data-only`, `all-data-only`, `full`, `full-all-data`. |
| `WAL_PATH` | path | metadata path | Path where WAL files are written. |
| `WAL_SMALL_OBJECT_LIMIT` | integer (bytes) | `6000` | In `full` or `data-only` WAL mode, object size threshold for writing object data to WAL. |
| `WAL_SIZE_MB` | integer (MiB) | `1000` | WAL file size threshold before reset/rollover. |
| `WAL_ITEMS` | integer | `10000` | WAL item count threshold before reset/rollover. |

HS5 can also read these options from environment variables using the `HS5_` prefix (for example,
`HS5_HTTP_PORT`). Option precedence is: config file values first, then environment variables, then command line arguments.

## Tuning for best performance

- Enable `MANUAL_COMMIT` for write-heavy workloads when you can tolerate delayed visibility/durability. This reduces commit overhead, but recent writes are not fully durable until commit (or `COMMIT_AFTER_MS`) runs.
- Set `MANUAL_COMMIT_LIST_CONSISTENT=false` to reduce list latency in manual commit mode. Trade-off: list results may temporarily miss uncommitted objects (eventual consistency until commit).
- Enable `WAL_MODE` only when `MANUAL_COMMIT` is disabled, and prefer placing `WAL_PATH` on storage that is at least as fast as `METADATA_STORAGE_PATH`.
- Set `TRIM_FREESPACE_SIZE=-1` to disable hole-punch trimming when optimizing for throughput over disk-space reclamation.
- Increase `DB_MAX_DIRTY_MB` if you have available RAM and want fewer flushes under sustained writes in `MANUAL_COMMIT` mode.
- Increase `WORKER_THREADS` above the default when storage is fast and CPU is not saturated; raise gradually while monitoring throughput and p95/p99 latency, since too many threads can increase contention and hurt performance.
