#pragma once

/*
 * SQLite backed replacement for the subset of the (patched) LMDB API used by
 * SingleFileStorage. Enabled via HS5_LMDB_SQLITE (CMake option of the same name).
 *
 * Each LMDB named database ("dbi") is a WITHOUT ROWID table. Keys (and values of
 * MDB_DUPSORT databases) are stored as TEXT with a per-dbi collation that calls the
 * comparator set via mdb_set_compare(), so the SQLite B-tree order is the LMDB order.
 * Write transactions are serialized on one writer connection, read transactions use
 * pooled connections and see a WAL snapshot (like LMDB MVCC readers).
 *
 * All connections run with synchronous=NORMAL. Without MDB_NOSYNC each commit fsyncs the
 * WAL through a separately opened descriptor, which is closed only after all connections
 * (closing it earlier would drop SQLite's POSIX locks).
 *
 * Differences to LMDB:
 *  - MDB_PREVSNAPSHOT is ignored (the current snapshot is opened).
 *  - Nested transactions are not supported (EINVAL).
 *  - Leaf "pages" are emulated as runs of LMDB_SQLITE_PAGE_ENTRIES consecutive items.
 *    mdb_page_is_dirty() always reports dirty, so page rewriting is a no-op.
 *  - Returned MDB_val data is a copy. Results from mdb_get() (and the existing data
 *    returned by an MDB_NOOVERWRITE put) stay valid until the next mdb_get() or
 *    MDB_NOOVERWRITE put in the same transaction, or until it ends. mdb_put() and
 *    mdb_del() do not invalidate them. Results from cursor operations stay valid for the next three
 *    operations on that cursor, and until the end of the transaction if the cursor is
 *    closed before then.
 *  - mdb_get_map() reports no mapping, mdb_madvise() and the map size are ignored.
 */

#include "LmdbApi.h"

#define LMDB_SQLITE_PAGE_ENTRIES 256
