#pragma once

/*
 * Replacement for the subset of the (patched) LMDB API used by SingleFileStorage that
 * stores the data in SQLite B-trees, using the SQLite B-tree, pager and WAL layers
 * directly (no SQL). Enabled via HS5_LMDB_SQLITE_BTREE (CMake option of the same name).
 * The SQLite internals are accessed via LmdbSqliteBtreeShim.h.
 *
 * The file is an SQLite database in WAL mode (application_id 'LMBT'), but without SQL
 * schema: each LMDB named database ("dbi") is an index B-tree whose entries are records
 * (key TEXT, value), compared with the comparators set via mdb_set_compare() and
 * mdb_set_dupsort(). For MDB_DUPSORT dbis the value is part of the B-tree key, so the
 * B-tree order is the LMDB order. A catalog B-tree (root page in the user_version header
 * field) maps dbi names to root pages and stores the LMDB transaction id.
 *
 * Write transactions are serialized on one writer connection, read transactions use
 * pooled connections and see a WAL snapshot (like LMDB MVCC readers). WAL checkpoints run
 * in a separate thread. All connections run with synchronous=NORMAL. Without MDB_NOSYNC
 * mdb_txn_commit() fsyncs the WAL after each commit that wrote data, otherwise only
 * mdb_env_sync() does.
 *
 * Differences to LMDB:
 *  - MDB_PREVSNAPSHOT is ignored (the current snapshot is opened).
 *  - Nested transactions are not supported (EINVAL).
 *  - The "leaf pages" of mdb_cursor_first_leaf_page() and friends are SQLite B-tree pages.
 *    Index B-trees store entries on interior pages too, so these are visited as well
 *    (each page once, when the iteration reaches its first entry).
 *    mdb_page_is_dirty() always reports dirty, so page rewriting is a no-op.
 *  - Returned MDB_val data is a copy. Results from mdb_get() (and the existing data
 *    returned by an MDB_NOOVERWRITE put) stay valid until the next mdb_get() or
 *    MDB_NOOVERWRITE put in the same transaction, or until it ends. mdb_put() and
 *    mdb_del() do not invalidate them. Results from cursor operations stay valid for the
 *    next three operations on that cursor, and until the end of the transaction if the
 *    cursor is closed before then. Results of mdb_page_get_val() stay valid until the next
 *    mdb_page_get_nkeys()/mdb_page_get_val() on another page with that cursor.
 *  - A dbi created in a write transaction cannot be used by read transactions that started
 *    before its commit (MDB_BAD_DBI), and is closed if the transaction aborts.
 *  - mdb_get_map() reports no mapping, mdb_madvise() and the map size are ignored.
 */

#include "LmdbApi.h"
