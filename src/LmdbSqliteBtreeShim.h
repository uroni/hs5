#pragma once

/*
 * Minimal C interface to the SQLite B-tree, pager and WAL layers (no SQL), used by
 * LmdbSqliteBtree.cpp. Implemented in LmdbSqliteBtreeShim.c, which compiles a private
 * copy of the SQLite amalgamation (all SQLite symbols static).
 *
 * Every B-tree is an index B-tree whose entries are SQLite records with two fields:
 * the key (TEXT, compared by the key collation) and the value (TEXT compared by the
 * value collation for DUPSORT trees, otherwise BLOB and not compared). Without a
 * collation function the comparison is memcmp() then length.
 *
 * Return codes are SQLite result codes.
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define LSBT_OK 0
#define LSBT_ERROR 1
#define LSBT_BUSY 5
#define LSBT_LOCKED 6
#define LSBT_NOMEM 7
#define LSBT_READONLY 8
#define LSBT_IOERR 10
#define LSBT_CORRUPT 11
#define LSBT_FULL 13
#define LSBT_CANTOPEN 14
#define LSBT_NOTFOUND 12
#define LSBT_TOOBIG 18
#define LSBT_PERM 3
#define LSBT_NOTADB 26
#define LSBT_DONE 101

/* Header meta value slots (see sqlite3BtreeGetMeta()) */
#define LSBT_META_USER_VERSION 6
#define LSBT_META_APPLICATION_ID 8

    typedef struct lsbt_conn lsbt_conn;
    typedef struct lsbt_cursor lsbt_cursor;
    typedef struct lsbt_keyinfo lsbt_keyinfo;

    typedef int (*lsbt_cmp)(void* arg, int na, const void* a, int nb, const void* b);

    typedef struct lsbt_kv
    {
        const void* k;
        size_t nk;
        const void* v;
        size_t nv;
    } lsbt_kv;

    typedef enum lsbt_seek_op
    {
        LSBT_SEEK_GE, /* First entry >= key */
        LSBT_SEEK_GT, /* First entry > key */
        LSBT_SEEK_LE, /* Last entry <= key */
        LSBT_SEEK_LT  /* Last entry < key */
    } lsbt_seek_op;

    const char* lsbt_errstr(int rc);

    /* Connection (one pager, one WAL read snapshot at a time). Not thread safe. */
    int lsbt_open(const char* path, int create, lsbt_conn** out);
    void lsbt_close(lsbt_conn* c);
    void lsbt_configure(lsbt_conn* c, int sync_full, int cache_kib, int busy_timeout_ms);
    /* Switches the file to WAL mode if it is not (sets file format version 2) */
    int lsbt_enable_wal(lsbt_conn* c);

    int lsbt_begin(lsbt_conn* c, int write);
    int lsbt_commit(lsbt_conn* c);
    void lsbt_rollback(lsbt_conn* c);

    /* Only within a transaction */
    uint32_t lsbt_get_meta(lsbt_conn* c, int idx);
    int lsbt_update_meta(lsbt_conn* c, int idx, uint32_t value);
    uint32_t lsbt_page_count(lsbt_conn* c);
    int lsbt_create_tree(lsbt_conn* c, uint32_t* root);

    /* WAL frames after the last commit on this connection (0 if none since the last call) */
    int lsbt_wal_frames(lsbt_conn* c);
    /* Passive checkpoint. Outside of transactions. */
    int lsbt_checkpoint(lsbt_conn* c, int* n_log, int* n_ckpt);
    /* fsync() of the connection's WAL file */
    int lsbt_sync_wal(lsbt_conn* c);

    /* Comparison info for the trees of one dbi on one connection. Freed before the connection. */
    lsbt_keyinfo* lsbt_keyinfo_create(lsbt_conn* c, int dupsort, lsbt_cmp key_cmp, void* key_arg, lsbt_cmp dup_cmp, void* dup_arg);
    void lsbt_keyinfo_free(lsbt_keyinfo* ki);

    /* Cursors are only valid within the transaction and must be closed before it ends */
    int lsbt_cursor_open(lsbt_conn* c, uint32_t root, int write, lsbt_keyinfo* ki, lsbt_cursor** out);
    void lsbt_cursor_close(lsbt_cursor* cur);

    /* Returns LSBT_DONE if there is no such entry. *eq (optional) is set if an entry equal
     * to the key (key and, if dupsort and v != NULL, value) exists. With GE/LE the cursor is
     * then positioned on it (first/last such entry for a key only search on a dupsort tree). */
    int lsbt_seek(lsbt_cursor* cur, const void* k, size_t nk, const void* v, size_t nv, lsbt_seek_op op, int* eq);
    int lsbt_first(lsbt_cursor* cur);
    int lsbt_last(lsbt_cursor* cur);
    int lsbt_next(lsbt_cursor* cur);
    int lsbt_prev(lsbt_cursor* cur);
    /* True if positioned on an entry or on a saved position that is restored by the next move */
    int lsbt_positioned(lsbt_cursor* cur);

    /* Entry at the cursor. Valid until the cursor or tree changes. */
    int lsbt_read(lsbt_cursor* cur, lsbt_kv* out);

    /* Insert, replacing an entry with the same key (non-dupsort) or same key and value
     * (dupsort). The cursor is left on the new entry. */
    int lsbt_put(lsbt_cursor* cur, const void* k, size_t nk, const void* v, size_t nv);
    /* Delete the entry the cursor is positioned on. lsbt_next()/lsbt_prev() then move to
     * the entry after/before the deleted one. */
    int lsbt_delete(lsbt_cursor* cur);

    /* B-tree pages. Every entry is on exactly one page; leaf pages and interior pages
     * (index B-trees store entries on interior pages too) are both visited. */
    int lsbt_cursor_pgno(lsbt_cursor* cur, uint32_t* pgno);
    /* Move to the first entry, in key order, on a page not visited yet by an iteration
     * started with lsbt_first(). Returns LSBT_DONE at the end. */
    int lsbt_next_page(lsbt_cursor* cur);

    typedef void (*lsbt_page_cb)(void* arg, unsigned idx, const lsbt_kv* kv);
    /* Read all entries of B-tree page pgno (in page order, which is key order). */
    int lsbt_read_page(lsbt_conn* c, uint32_t pgno, lsbt_page_cb cb, void* arg, unsigned* n_entries);

#ifdef __cplusplus
}
#endif
