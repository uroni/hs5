/*
 * C layer over the SQLite B-tree/pager/WAL internals, see LmdbSqliteBtreeShim.h.
 *
 * The SQLite amalgamation is compiled into this translation unit with all public API
 * symbols static, so this is a private copy of SQLite that does not clash with the
 * SQLite linked into the rest of the program (both copies must not open the same file:
 * the POSIX lock bookkeeping of the unix VFS is per copy).
 *
 * Connections are opened with sqlite3_open_v2() (which sets up the Btree and pager),
 * but no SQL statement is ever prepared: all data access goes through the Btree API.
 * Built against the SQLite version of the sqlite-cpp-sqlgen port (3.45.1); the
 * internals used here are not a stable interface.
 */

#define SQLITE_API static
#define SQLITE_EXTERN
#define SQLITE_OMIT_LOAD_EXTENSION 1
#define SQLITE_OMIT_SHARED_CACHE 1
#define SQLITE_DEFAULT_MEMSTATUS 0

#include "sqlite3.c"

#include "LmdbSqliteBtreeShim.h"

#if SQLITE_VERSION_NUMBER != 3045001
#warning "LmdbSqliteBtreeShim.c uses SQLite internals and was written against SQLite 3.45.1"
#endif

struct lsbt_conn
{
    sqlite3* db;
    Btree* bt;
    unsigned char* buf; /* Scratch buffer for entries with overflow pages in lsbt_read_page() */
    u32 buf_cap;
};

struct lsbt_keyinfo
{
    KeyInfo* ki;
    int dupsort;
    CollSeq coll[2];
};

struct lsbt_cursor
{
    lsbt_keyinfo* kinfo;
    unsigned char* buf; /* Record encoding and payloads with overflow pages */
    u32 buf_cap;
    BtCursor cur;
};

static int ensure_buf(unsigned char** buf, u32* cap, u64 n)
{
    if (n <= *cap)
        return SQLITE_OK;
    if (n > 0x7fffff00)
        return SQLITE_TOOBIG;
    u32 ncap = *cap > 0 ? *cap : 256;
    while (ncap < n)
        ncap = ncap > 0x3fffffff ? (u32)n : ncap * 2;
    unsigned char* nbuf = (unsigned char*)sqlite3_realloc64(*buf, ncap);
    if (nbuf == 0)
        return SQLITE_NOMEM;
    *buf = nbuf;
    *cap = ncap;
    return SQLITE_OK;
}

const char* lsbt_errstr(int rc)
{
    return sqlite3ErrStr(rc);
}

int lsbt_open(const char* path, int create, lsbt_conn** out)
{
    lsbt_conn* c = (lsbt_conn*)sqlite3_malloc(sizeof(*c));
    if (c == 0)
        return SQLITE_NOMEM;
    memset(c, 0, sizeof(*c));

    int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_NOMUTEX | SQLITE_OPEN_PRIVATECACHE | (create ? SQLITE_OPEN_CREATE : 0);
    int rc = sqlite3_open_v2(path, &c->db, flags, 0);
    if (rc != SQLITE_OK)
    {
        sqlite3_close_v2(c->db);
        sqlite3_free(c);
        return rc;
    }
    c->bt = c->db->aDb[0].pBt;
    *out = c;
    return SQLITE_OK;
}

void lsbt_close(lsbt_conn* c)
{
    if (c == 0)
        return;
    sqlite3_close_v2(c->db);
    sqlite3_free(c->buf);
    sqlite3_free(c);
}

void lsbt_configure(lsbt_conn* c, int sync_full, int cache_kib, int busy_timeout_ms)
{
    sqlite3_busy_timeout(c->db, busy_timeout_ms);
    sqlite3BtreeSetCacheSize(c->bt, -cache_kib);
    sqlite3BtreeSetPagerFlags(c->bt, (sync_full ? PAGER_SYNCHRONOUS_FULL : PAGER_SYNCHRONOUS_NORMAL) | PAGER_CACHESPILL);
}

int lsbt_enable_wal(lsbt_conn* c)
{
    /* Same as the VDBE does for PRAGMA journal_mode=WAL. Leaves a transaction open. */
    int rc = sqlite3BtreeSetVersion(c->bt, 2);
    if (rc == SQLITE_OK)
        rc = lsbt_commit(c);
    if (rc != SQLITE_OK)
        lsbt_rollback(c);
    return rc;
}

int lsbt_begin(lsbt_conn* c, int write)
{
    return sqlite3BtreeBeginTrans(c->bt, write ? 1 : 0, 0);
}

int lsbt_commit(lsbt_conn* c)
{
    int rc = sqlite3BtreeCommitPhaseOne(c->bt, 0);
    if (rc == SQLITE_OK)
        rc = sqlite3BtreeCommitPhaseTwo(c->bt, 0);
    return rc;
}

void lsbt_rollback(lsbt_conn* c)
{
    sqlite3BtreeRollback(c->bt, SQLITE_OK, 0);
}

uint32_t lsbt_get_meta(lsbt_conn* c, int idx)
{
    u32 v = 0;
    sqlite3BtreeGetMeta(c->bt, idx, &v);
    return v;
}

int lsbt_update_meta(lsbt_conn* c, int idx, uint32_t value)
{
    return sqlite3BtreeUpdateMeta(c->bt, idx, value);
}

uint32_t lsbt_page_count(lsbt_conn* c)
{
    return btreePagecount(c->bt->pBt);
}

int lsbt_create_tree(lsbt_conn* c, uint32_t* root)
{
    Pgno pgno = 0;
    int rc = sqlite3BtreeCreateTable(c->bt, &pgno, BTREE_BLOBKEY);
    *root = pgno;
    return rc;
}

int lsbt_wal_frames(lsbt_conn* c)
{
    return sqlite3PagerWalCallback(sqlite3BtreePager(c->bt));
}

int lsbt_checkpoint(lsbt_conn* c, int* n_log, int* n_ckpt)
{
    return sqlite3BtreeCheckpoint(c->bt, SQLITE_CHECKPOINT_PASSIVE, n_log, n_ckpt);
}

int lsbt_sync_wal(lsbt_conn* c)
{
    Pager* pager = sqlite3BtreePager(c->bt);
    if (pager->pWal == 0)
        return SQLITE_OK;
    sqlite3_file* f = sqlite3PagerJrnlFile(pager);
    if (f == 0 || !isOpen(f))
        return SQLITE_OK;
    return sqlite3OsSync(f, SQLITE_SYNC_NORMAL);
}

lsbt_keyinfo* lsbt_keyinfo_create(lsbt_conn* c, int dupsort, lsbt_cmp key_cmp, void* key_arg, lsbt_cmp dup_cmp, void* dup_arg)
{
    lsbt_keyinfo* r = (lsbt_keyinfo*)sqlite3_malloc(sizeof(*r));
    if (r == 0)
        return 0;
    memset(r, 0, sizeof(*r));
    r->dupsort = dupsort;
    /* Number of key fields: positions saved by a cursor (before another cursor modifies the
     * tree) are restored by comparing the first nKeyField+1 fields only, so for non-dupsort
     * trees this has to be 0, otherwise a changed value moves the restored position. */
    r->ki = sqlite3KeyInfoAlloc(c->db, dupsort ? 1 : 0, dupsort ? 1 : 2);
    if (r->ki == 0)
    {
        sqlite3_free(r);
        return 0;
    }
    /* sqlite3KeyInfoAlloc() only zeroes the memory after the struct, aColl[0] is in it */
    for (int i = 0; i < r->ki->nAllField; ++i)
    {
        r->ki->aColl[i] = 0;
        r->ki->aSortFlags[i] = 0;
    }
    if (key_cmp != 0)
    {
        r->coll[0].zName = (char*)"lmdb_key";
        r->coll[0].enc = SQLITE_UTF8;
        r->coll[0].pUser = key_arg;
        r->coll[0].xCmp = key_cmp;
        r->ki->aColl[0] = &r->coll[0];
    }
    if (dupsort && dup_cmp != 0)
    {
        r->coll[1].zName = (char*)"lmdb_dup";
        r->coll[1].enc = SQLITE_UTF8;
        r->coll[1].pUser = dup_arg;
        r->coll[1].xCmp = dup_cmp;
        r->ki->aColl[1] = &r->coll[1];
    }
    return r;
}

void lsbt_keyinfo_free(lsbt_keyinfo* ki)
{
    if (ki == 0)
        return;
    sqlite3KeyInfoUnref(ki->ki);
    sqlite3_free(ki);
}

int lsbt_cursor_open(lsbt_conn* c, uint32_t root, int write, lsbt_keyinfo* ki, lsbt_cursor** out)
{
    if (root < 2)
        return SQLITE_CORRUPT;
    lsbt_cursor* cur = (lsbt_cursor*)sqlite3_malloc(sizeof(*cur));
    if (cur == 0)
        return SQLITE_NOMEM;
    memset(cur, 0, sizeof(*cur));
    cur->kinfo = ki;
    sqlite3BtreeCursorZero(&cur->cur);
    int rc = sqlite3BtreeCursor(c->bt, root, write ? BTREE_WRCSR : 0, ki->ki, &cur->cur);
    if (rc != SQLITE_OK)
    {
        sqlite3_free(cur);
        return rc;
    }
    *out = cur;
    return SQLITE_OK;
}

void lsbt_cursor_close(lsbt_cursor* cur)
{
    if (cur == 0)
        return;
    sqlite3BtreeCloseCursor(&cur->cur);
    sqlite3_free(cur->buf);
    sqlite3_free(cur);
}

static void set_text_mem(Mem* m, sqlite3* db, const void* p, size_t n)
{
    memset(m, 0, sizeof(*m));
    m->flags = MEM_Str;
    m->enc = SQLITE_UTF8;
    m->z = (char*)(p != 0 ? p : "");
    m->n = (int)n;
    m->db = db;
}

int lsbt_seek(lsbt_cursor* cur, const void* k, size_t nk, const void* v, size_t nv, lsbt_seek_op op, int* eq)
{
    if (nk > 0x7fffffff || nv > 0x7fffffff)
        return SQLITE_TOOBIG;

    KeyInfo* ki = cur->kinfo->ki;
    Mem m[2];
    UnpackedRecord r;
    memset(&r, 0, sizeof(r));
    set_text_mem(&m[0], ki->db, k, nk);
    r.nField = 1;
    if (v != 0 && cur->kinfo->dupsort)
    {
        set_text_mem(&m[1], ki->db, v, nv);
        r.nField = 2;
    }
    r.pKeyInfo = ki;
    r.aMem = m;
    /* Equal entries compare as greater (GE, LT) or smaller (GT, LE) than the key, so the
     * search ends next to the first/last equal entry. */
    r.default_rc = (op == LSBT_SEEK_GE || op == LSBT_SEEK_LT) ? 1 : -1;

    int res = 0;
    int rc = sqlite3BtreeIndexMoveto(&cur->cur, &r, &res);
    if (rc != SQLITE_OK)
        return rc;
    if (r.errCode != 0)
        return r.errCode;
    if (eq != 0)
        *eq = r.eqSeen;
    if (cur->cur.eState != CURSOR_VALID)
        return SQLITE_DONE; /* Empty tree */

    switch (op)
    {
        case LSBT_SEEK_GE:
        case LSBT_SEEK_GT:
            if (res < 0)
                rc = sqlite3BtreeNext(&cur->cur, 0);
            break;
        case LSBT_SEEK_LE:
        case LSBT_SEEK_LT:
            if (res > 0)
                rc = sqlite3BtreePrevious(&cur->cur, 0);
            break;
    }
    return rc;
}

int lsbt_first(lsbt_cursor* cur)
{
    int empty = 0;
    int rc = sqlite3BtreeFirst(&cur->cur, &empty);
    if (rc == SQLITE_OK && empty)
        return SQLITE_DONE;
    return rc;
}

int lsbt_last(lsbt_cursor* cur)
{
    int empty = 0;
    int rc = sqlite3BtreeLast(&cur->cur, &empty);
    if (rc == SQLITE_OK && empty)
        return SQLITE_DONE;
    return rc;
}

int lsbt_next(lsbt_cursor* cur)
{
    return sqlite3BtreeNext(&cur->cur, 0);
}

int lsbt_prev(lsbt_cursor* cur)
{
    return sqlite3BtreePrevious(&cur->cur, 0);
}

int lsbt_positioned(lsbt_cursor* cur)
{
    return cur->cur.eState == CURSOR_VALID || cur->cur.eState == CURSOR_SKIPNEXT || cur->cur.eState == CURSOR_REQUIRESEEK;
}

static int decode_record(const unsigned char* p, u32 n, lsbt_kv* out)
{
    u32 hdr, t1, t2;
    if (n < 3)
        return SQLITE_CORRUPT_BKPT;
    u32 i = getVarint32(p, hdr);
    if (hdr > n || hdr < 3)
        return SQLITE_CORRUPT_BKPT;
    i += getVarint32(&p[i], t1);
    if (i >= hdr)
        return SQLITE_CORRUPT_BKPT;
    i += getVarint32(&p[i], t2);
    if (i != hdr || t1 < 12 || t2 < 12)
        return SQLITE_CORRUPT_BKPT;
    u64 n1 = (t1 - 12) / 2;
    u64 n2 = (t2 - 12) / 2;
    if ((u64)hdr + n1 + n2 != n)
        return SQLITE_CORRUPT_BKPT;
    out->k = p + hdr;
    out->nk = (size_t)n1;
    out->v = p + hdr + n1;
    out->nv = (size_t)n2;
    return SQLITE_OK;
}

int lsbt_read(lsbt_cursor* cur, lsbt_kv* out)
{
    BtCursor* bc = &cur->cur;
    if (bc->eState != CURSOR_VALID)
    {
        int different = 0;
        int rc = sqlite3BtreeCursorRestore(bc, &different);
        if (rc != SQLITE_OK)
            return rc;
        if (bc->eState == CURSOR_SKIPNEXT)
            bc->eState = CURSOR_VALID;
        if (bc->eState != CURSOR_VALID)
            return SQLITE_DONE;
    }
    u32 n = sqlite3BtreePayloadSize(bc);
    u32 amt = 0;
    const unsigned char* p = (const unsigned char*)sqlite3BtreePayloadFetch(bc, &amt);
    if (amt < n)
    {
        int rc = ensure_buf(&cur->buf, &cur->buf_cap, n);
        if (rc != SQLITE_OK)
            return rc;
        rc = sqlite3BtreePayload(bc, 0, n, cur->buf);
        if (rc != SQLITE_OK)
            return rc;
        p = cur->buf;
    }
    return decode_record(p, n, out);
}

int lsbt_put(lsbt_cursor* cur, const void* k, size_t nk, const void* v, size_t nv)
{
    if (nk > 0x3fffffff || nv > 0x3fffffff)
        return SQLITE_TOOBIG;

    const int dupsort = cur->kinfo->dupsort;
    const u64 t1 = (u64)nk * 2 + 13;                                 /* TEXT */
    const u64 t2 = (u64)nv * 2 + (dupsort ? 13 : 12);                /* TEXT or BLOB */
    const int hdr = 1 + sqlite3VarintLen(t1) + sqlite3VarintLen(t2); /* < 128, so the size is one byte */
    const u64 total = (u64)hdr + nk + nv;
    if (total > 0x7fffff00)
        return SQLITE_TOOBIG;

    int rc = ensure_buf(&cur->buf, &cur->buf_cap, total);
    if (rc != SQLITE_OK)
        return rc;
    unsigned char* p = cur->buf;
    p[0] = (unsigned char)hdr;
    int i = 1;
    i += sqlite3PutVarint(&p[i], t1);
    i += sqlite3PutVarint(&p[i], t2);
    if (nk > 0)
        memcpy(&p[i], k, nk);
    if (nv > 0)
        memcpy(&p[i + nk], v, nv);

    KeyInfo* ki = cur->kinfo->ki;
    Mem m[2];
    set_text_mem(&m[0], ki->db, k, nk);
    set_text_mem(&m[1], ki->db, v, nv);

    BtreePayload x;
    memset(&x, 0, sizeof(x));
    x.pKey = p;
    x.nKey = (sqlite3_int64)total;
    /* The insert seeks with these fields: the key (and value for dupsort), so an entry
     * with the same key (pair) is overwritten */
    x.aMem = m;
    x.nMem = dupsort ? 2 : 1;

    rc = sqlite3BtreeInsert(&cur->cur, &x, 0, 0);
    if (rc != SQLITE_OK)
        return rc;

    if (cur->cur.eState != CURSOR_VALID)
    {
        /* Rebalanced: reposition on the new entry */
        int eq = 0;
        rc = lsbt_seek(cur, k, nk, dupsort ? v : 0, nv, LSBT_SEEK_GE, &eq);
        if (rc == SQLITE_OK && !eq)
            rc = SQLITE_CORRUPT_BKPT;
        if (rc == SQLITE_DONE)
            rc = SQLITE_CORRUPT_BKPT;
    }
    return rc;
}

int lsbt_delete(lsbt_cursor* cur)
{
    return sqlite3BtreeDelete(&cur->cur, BTREE_SAVEPOSITION);
}

static int ensure_valid(BtCursor* bc)
{
    if (bc->eState >= CURSOR_REQUIRESEEK)
    {
        int different = 0;
        int rc = sqlite3BtreeCursorRestore(bc, &different);
        if (rc != SQLITE_OK)
            return rc;
    }
    if (bc->eState == CURSOR_SKIPNEXT)
    {
        /* Positioned next to a deleted entry. Take the position as it is. */
        bc->eState = CURSOR_VALID;
        bc->skipNext = 0;
    }
    return bc->eState == CURSOR_VALID ? SQLITE_OK : SQLITE_DONE;
}

int lsbt_cursor_pgno(lsbt_cursor* cur, uint32_t* pgno)
{
    int rc = ensure_valid(&cur->cur);
    if (rc != SQLITE_OK)
        return rc;
    *pgno = cur->cur.pPage->pgno;
    return SQLITE_OK;
}

int lsbt_next_page(lsbt_cursor* cur)
{
    BtCursor* bc = &cur->cur;
    int rc = ensure_valid(bc);
    if (rc != SQLITE_OK)
        return rc;

    /* In-order traversal: a leaf is entered at its first cell, an interior page first at
     * cell 0 (after its leftmost subtree) and then again at each later cell. So the first
     * entry of a page not visited yet is the next entry that is on a leaf or at cell 0. */
    for (;;)
    {
        MemPage* page = bc->pPage;
        if (page->leaf && page->nCell > 0)
        {
            /* Skip the rest of the leaf */
            bc->ix = page->nCell - 1;
            bc->info.nSize = 0;
            bc->curFlags &= ~(BTCF_ValidNKey | BTCF_ValidOvfl | BTCF_AtLast);
        }
        rc = sqlite3BtreeNext(bc, 0);
        if (rc != SQLITE_OK)
            return rc;
        if (bc->pPage->leaf || bc->ix == 0)
            return SQLITE_OK;
    }
}

int lsbt_read_page(lsbt_conn* c, uint32_t pgno, lsbt_page_cb cb, void* arg, unsigned* n_entries)
{
    BtShared* bt = c->bt->pBt;
    if (pgno < 2 || pgno > btreePagecount(bt))
        return SQLITE_NOTFOUND;

    MemPage* page = 0;
    int rc = getAndInitPage(bt, pgno, &page, PAGER_GET_READONLY);
    if (rc != SQLITE_OK)
        return rc;

    if (page->intKey || page->nOverflow != 0)
    {
        releasePage(page);
        return SQLITE_NOTFOUND;
    }

    const u32 ovfl_size = bt->usableSize - 4;
    for (unsigned i = 0; i < page->nCell && rc == SQLITE_OK; ++i)
    {
        u8* cell = findCell(page, i);
        if (cell < page->aData || cell >= page->aDataEnd)
        {
            rc = SQLITE_CORRUPT_BKPT;
            break;
        }
        CellInfo info;
        page->xParseCell(page, cell, &info);
        const unsigned char* payload = info.pPayload;
        if (info.nLocal < info.nPayload)
        {
            /* Collect the overflow chain */
            if (info.pPayload + info.nLocal + 4 > page->aDataEnd)
            {
                rc = SQLITE_CORRUPT_BKPT;
                break;
            }
            rc = ensure_buf(&c->buf, &c->buf_cap, info.nPayload);
            if (rc != SQLITE_OK)
                break;
            memcpy(c->buf, info.pPayload, info.nLocal);
            Pgno ovfl = get4byte(&info.pPayload[info.nLocal]);
            u32 off = info.nLocal;
            while (off < info.nPayload)
            {
                if (ovfl < 2 || ovfl > btreePagecount(bt))
                {
                    rc = SQLITE_CORRUPT_BKPT;
                    break;
                }
                DbPage* dbp = 0;
                rc = sqlite3PagerGet(bt->pPager, ovfl, &dbp, PAGER_GET_READONLY);
                if (rc != SQLITE_OK)
                    break;
                const unsigned char* data = (const unsigned char*)sqlite3PagerGetData(dbp);
                u32 n = info.nPayload - off;
                if (n > ovfl_size)
                    n = ovfl_size;
                memcpy(c->buf + off, data + 4, n);
                ovfl = get4byte(data);
                sqlite3PagerUnref(dbp);
                off += n;
            }
            if (rc != SQLITE_OK)
                break;
            payload = c->buf;
        }
        lsbt_kv kv;
        rc = decode_record(payload, info.nPayload, &kv);
        if (rc == SQLITE_OK)
            cb(arg, i, &kv);
    }
    if (rc == SQLITE_OK && n_entries != 0)
        *n_entries = page->nCell;
    releasePage(page);
    return rc;
}
