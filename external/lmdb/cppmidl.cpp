/**
 * Copyright Martin Raiber. All Rights Reserved.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
 
#include "cppmidl.h"
#include <set>

struct _I_CPPMIDL
{
	std::set<MDB_ID> ids;
	std::set<MDB_ID>::iterator it;
};

extern "C"
{
	struct _CPPMIDL
	{
		struct _I_CPPMIDL i;
	};
}

CPPMIDL cppmidl_alloc()
{
	return new _CPPMIDL;
}

const MDB_ID* cppmidl_search(CPPMIDL ids, MDB_ID id)
{
	std::set<MDB_ID>::iterator it = ids->i.ids.lower_bound(id);
	if (it == ids->i.ids.end())
	{
		return NULL;
	}
	return &(*it);
}

void cppmidl_erase(CPPMIDL ids, MDB_ID id)
{
	ids->i.ids.erase(id);
}

int cppmidl_empty(CPPMIDL ids)
{
	return ids->i.ids.empty() ? 1 : 0;
}

void cppmidl_begin(CPPMIDL ids)
{
	ids->i.it = ids->i.ids.begin();
}

const MDB_ID* cppmidl_next(CPPMIDL ids)
{
	if (ids->i.it == ids->i.ids.end())
		return NULL;

	const MDB_ID* ret = &(*ids->i.it);
	++ids->i.it;
	return ret;
}

void cppmidl_free(CPPMIDL ids)
{
	delete ids;
}

void cppmidl_insert(CPPMIDL ids, MDB_ID id)
{
	ids->i.ids.insert(id);
}

void cppmidl_insert_list(CPPMIDL ids, CPPMIDL other)
{
	ids->i.ids.insert(other->i.ids.begin(), other->i.ids.end());
}
