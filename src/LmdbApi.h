#pragma once

/*
 * Subset of the (patched) LMDB API used by SingleFileStorage, shared by the LMDB
 * replacement backends (LmdbSqlite.h, LmdbSqliteBtree.h).
 */

#include <stddef.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef mode_t mdb_mode_t;
    typedef unsigned int MDB_dbi;

    typedef struct MDB_env MDB_env;
    typedef struct MDB_txn MDB_txn;
    typedef struct MDB_cursor MDB_cursor;

    typedef struct MDB_val
    {
        size_t mv_size;
        void* mv_data;
    } MDB_val;

    typedef int(MDB_cmp_func)(const MDB_val* a, const MDB_val* b);

/* Environment flags */
#define MDB_FIXEDMAP 0x01
#define MDB_NOSUBDIR 0x4000
#define MDB_NOSYNC 0x10000
#define MDB_RDONLY 0x20000
#define MDB_NOMETASYNC 0x40000
#define MDB_WRITEMAP 0x80000
#define MDB_MAPASYNC 0x100000
#define MDB_NOTLS 0x200000
#define MDB_NOLOCK 0x400000
#define MDB_NORDAHEAD 0x800000
#define MDB_NOMEMINIT 0x1000000
#define MDB_PREVSNAPSHOT 0x2000000

/* Database flags */
#define MDB_REVERSEKEY 0x02
#define MDB_DUPSORT 0x04
#define MDB_INTEGERKEY 0x08
#define MDB_DUPFIXED 0x10
#define MDB_INTEGERDUP 0x20
#define MDB_REVERSEDUP 0x40
#define MDB_CREATE 0x40000

/* Write flags */
#define MDB_NOOVERWRITE 0x10
#define MDB_NODUPDATA 0x20
#define MDB_CURRENT 0x40
#define MDB_RESERVE 0x10000
#define MDB_APPEND 0x20000
#define MDB_APPENDDUP 0x40000
#define MDB_MULTIPLE 0x80000

    typedef enum MDB_cursor_op
    {
        MDB_FIRST,
        MDB_FIRST_DUP,
        MDB_GET_BOTH,
        MDB_GET_BOTH_RANGE,
        MDB_GET_CURRENT,
        MDB_GET_MULTIPLE,
        MDB_LAST,
        MDB_LAST_DUP,
        MDB_NEXT,
        MDB_NEXT_DUP,
        MDB_NEXT_MULTIPLE,
        MDB_NEXT_NODUP,
        MDB_PREV,
        MDB_PREV_DUP,
        MDB_PREV_NODUP,
        MDB_SET,
        MDB_SET_KEY,
        MDB_SET_RANGE,
        MDB_PREV_MULTIPLE
    } MDB_cursor_op;

/* Return codes */
#define MDB_SUCCESS 0
#define MDB_KEYEXIST (-30799)
#define MDB_NOTFOUND (-30798)
#define MDB_PAGE_NOTFOUND (-30797)
#define MDB_CORRUPTED (-30796)
#define MDB_PANIC (-30795)
#define MDB_VERSION_MISMATCH (-30794)
#define MDB_INVALID (-30793)
#define MDB_MAP_FULL (-30792)
#define MDB_DBS_FULL (-30791)
#define MDB_READERS_FULL (-30790)
#define MDB_TLS_FULL (-30789)
#define MDB_TXN_FULL (-30788)
#define MDB_CURSOR_FULL (-30787)
#define MDB_PAGE_FULL (-30786)
#define MDB_MAP_RESIZED (-30785)
#define MDB_INCOMPATIBLE (-30784)
#define MDB_BAD_RSLOT (-30783)
#define MDB_BAD_TXN (-30782)
#define MDB_BAD_VALSIZE (-30781)
#define MDB_BAD_DBI (-30780)
#define MDB_LAST_ERRCODE MDB_BAD_DBI
    char* mdb_strerror(int err);

    int mdb_env_create(MDB_env** env);
    int mdb_env_open(MDB_env* env, const char* path, unsigned int flags, mdb_mode_t mode);
    void mdb_env_close(MDB_env* env);
    int mdb_env_sync(MDB_env* env, int force);
    int mdb_env_get_path(MDB_env* env, const char** path);
    int mdb_env_set_mapsize(MDB_env* env, size_t size);
    int mdb_env_set_maxreaders(MDB_env* env, unsigned int readers);
    int mdb_env_set_maxdbs(MDB_env* env, MDB_dbi dbs);
    int mdb_env_set_idl_dirty_max(MDB_env* env, unsigned int idl_dirty_max);

    int mdb_txn_begin(MDB_env* env, MDB_txn* parent, unsigned int flags, MDB_txn** txn);
    int mdb_txn_commit(MDB_txn* txn);
    void mdb_txn_abort(MDB_txn* txn);

    int mdb_dbi_open(MDB_txn* txn, const char* name, unsigned int flags, MDB_dbi* dbi);
    int mdb_set_compare(MDB_txn* txn, MDB_dbi dbi, MDB_cmp_func* cmp);
    int mdb_set_dupsort(MDB_txn* txn, MDB_dbi dbi, MDB_cmp_func* cmp);

    int mdb_get(MDB_txn* txn, MDB_dbi dbi, MDB_val* key, MDB_val* data);
    int mdb_put(MDB_txn* txn, MDB_dbi dbi, MDB_val* key, MDB_val* data, unsigned int flags);
    int mdb_del(MDB_txn* txn, MDB_dbi dbi, MDB_val* key, MDB_val* data);

    int mdb_cursor_open(MDB_txn* txn, MDB_dbi dbi, MDB_cursor** cursor);
    void mdb_cursor_close(MDB_cursor* cursor);
    int mdb_cursor_get(MDB_cursor* cursor, MDB_val* key, MDB_val* data, MDB_cursor_op op);
    int mdb_cursor_put(MDB_cursor* cursor, MDB_val* key, MDB_val* data, unsigned int flags);
    int mdb_cursor_del(MDB_cursor* cursor, unsigned int flags);

    /* Extensions of the patched LMDB in external/lmdb */
    int mdb_madvise(MDB_env* env, int random);
    void mdb_env_unmap(MDB_env* env);
    size_t mdb_get_txnid(MDB_txn* txn);
    void mdb_env_get_txnids(MDB_env* env, size_t* txnid1, size_t* txnid2);
    void mdb_get_map(MDB_env* env, char** p_mmap, size_t* p_size);
    int mdb_cursor_first_leaf_page(MDB_cursor* cursor, size_t* pgno);
    int mdb_cursor_next_leaf_page(MDB_cursor* cursor, size_t* pgno);
    int mdb_cursor_get_pageno(MDB_cursor* cursor, size_t* pgno);
    int mdb_page_get_nkeys(MDB_cursor* mc, size_t pgno, unsigned int* nkeys);
    int mdb_page_get_val(MDB_cursor* mc, size_t pgno, unsigned int idx, MDB_val* key, MDB_val* data);
    int mdb_page_is_dirty(MDB_cursor* mc, size_t pgno, int* dirty);

#ifdef __cplusplus
}
#endif
