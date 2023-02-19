#pragma once
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

	typedef size_t MDB_ID;

	struct _CPPMIDL;
	typedef struct _CPPMIDL* CPPMIDL;

	struct _CPPMIDL_IT;
	typedef struct __CPPMIDL_IT* CPPMIDL_IT;

	CPPMIDL cppmidl_alloc();

	const MDB_ID* cppmidl_search(CPPMIDL ids, MDB_ID id);

	void cppmidl_erase(CPPMIDL ids, MDB_ID id);

	int cppmidl_empty(CPPMIDL ids);

	void cppmidl_begin(CPPMIDL ids);

	const MDB_ID* cppmidl_next(CPPMIDL ids);

	void cppmidl_free(CPPMIDL ids);

	void cppmidl_insert_list(CPPMIDL ids, CPPMIDL other);

	void cppmidl_insert(CPPMIDL ids, MDB_ID id);

#ifdef __cplusplus
}
#endif