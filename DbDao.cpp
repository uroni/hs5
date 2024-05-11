/**
 * Copyright Martin Raiber. All Rights Reserved.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
 #include "DbDao.h"
#include <Database.h>
#include <assert.h>
#include <gflags/gflags.h>

DEFINE_string(db_path, "./hs5.db", "Path where to put the database");

typedef void(upgrade_func_t)(sqlgen::Database&);

void upgrade0_1(sqlgen::Database& db)
{
	db.write(R"""(
		CREATE TABLE users (id INTEGER PRIMARY KEY,
		name TEXT NOT NULL, 
		password_state INTEGER NOT NULL,
		password TEXT NOT NULL,
		created INTEGER NOT NULL DEFAULT (unixepoch()),
		modified INTEGER NOT NULL DEFAULT (unixepoch()),
		system INTEGER NOT NULL
		) STRICT;
	)""");

	db.write(R"""(
		CREATE TABLE access_keys(
			id INTEGER PRIMARY KEY,
			user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
			created INTEGER NOT NULL DEFAULT (unixepoch()),
			description TEXT NOT NULL,
			key TEXT NOT NULL,
			secret_key TEXT NOT NULL,
			system INTEGER NOT NULL
		) STRICT;
	)""");

	db.write(R"""(
		CREATE TABLE roles (
			id INTEGER PRIMARY KEY,
			name TEXT NOT NULL,
			created INTEGER NOT NULL DEFAULT (unixepoch()),
			system INTEGER NOT NULL
		) STRICT;
	)""");

	db.write(R"""(
		CREATE TABLE policies (
			id INTEGER PRIMARY KEY,
			name TEXT NOT NULL,
			description TEXT NOT NULL,
			created INTEGER NOT NULL DEFAULT (unixepoch()),
			modified INTEGER NOT NULL DEFAULT (unixepoch()),
			ver INTEGER NOT NULL,
			data TEXT NOT NULL,
			system INTEGER NOT NULL		
		) STRICT;
	)""");

	db.write(R"""(
		CREATE TABLE user_roles (
			id INTEGER PRIMARY KEY,
			user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
			role_id INTEGER NOT NULL REFERENCES roles(id) ON DELETE CASCADE,
			created INTEGER NOT NULL DEFAULT (unixepoch()),
			system INTEGER NOT NULL,
			UNIQUE(user_id,role_id)
		) STRICT;
	)""");

	db.write(R"""(
		CREATE TABLE role_policies (
			id INTEGER PRIMARY KEY,
			role_id INTEGER NOT NULL REFERENCES roles(id) ON DELETE CASCADE,
			policy_id INTEGER NOT NULL REFERENCES policies(id) ON DELETE CASCADE,
			created INTEGER NOT NULL DEFAULT (unixepoch()),
			system INTEGER NOT NULL,
			UNIQUE(role_id,policy_id)
		) STRICT;
	)""");

	db.write(R"""(
		CREATE TABLE buckets (
			id INTEGER PRIMARY KEY,
			name TEXT NOT NULL,
			created INTEGER NOT NULL DEFAULT (unixepoch())
		) STRICT;
	)""");
}

const std::map<int64_t, upgrade_func_t*> upgrade_funcs {
	{0, upgrade0_1}
};

sqlgen::Database& DbDao::getStaticDb()
{
	static thread_local sqlgen::Database db(FLAGS_db_path);
	return db;
}

void DbDao::init()
{
	DbDao dao;
	sqlgen::ScopedManualCommitWriteTransaction trans;
	auto ver = dao.getUserVersion();
	const auto initVer = ver;
	for(auto it=upgrade_funcs.find(ver);it!=upgrade_funcs.end();it = upgrade_funcs.find(ver))
	{
		if(ver==initVer)
			trans.reset(&dao.getDb());

		it->second(dao.getDb());
		++ver;

		dao.setUserVersion(ver);
	}

	trans.commit();
}

int64_t DbDao::getUserVersion()
{
	auto res = db.read("PRAGMA user_version");
	if(res.empty() || res.begin()->empty())
		return 0;

	return std::atoll(res.begin()->begin()->second.c_str());
}

void DbDao::setUserVersion(int64_t newVersion)
{
	db.write("PRAGMA user_version = "+std::to_string(newVersion));
}

/**
* @-SQLGenAccess
* @func vector<User> DbDao::getUsers
* @return int64 id, string name, int password_state, string password
* @sql
*      SELECT id, name, password_state, password FROM users
*/
std::vector<DbDao::User> DbDao::getUsers()
{
	if(!_getUsers.prepared())
	{
		_getUsers=db.prepare("SELECT id, name, password_state, password FROM users");
	}
	auto& cursor=_getUsers.cursor();
	std::vector<DbDao::User> ret;
	while(cursor.next())
	{
		ret.emplace_back();
		DbDao::User& obj=ret.back();
		cursor.get(0, obj.id);
		cursor.get(1, obj.name);
		cursor.get(2, obj.password_state);
		cursor.get(3, obj.password);
	}
	return ret;
}

/**
* @-SQLGenAccess
* @func User DbDao::getUserById
* @return int64 id, string name, int password_state, string password
* @sql
*      SELECT id, name, password_state, password FROM users WHERE id=:id(int64)
*/
DbDao::User DbDao::getUserById(int64_t id)
{
	if(!_getUserById.prepared())
	{
		_getUserById=db.prepare("SELECT id, name, password_state, password FROM users WHERE id=?");
	}
	_getUserById.bind(id);
	auto& cursor=_getUserById.cursor();
	User ret = { false, 0, "", 0, "" };
	if(cursor.next())
	{
		ret.exists=true;
		cursor.get(0, ret.id);
		cursor.get(1, ret.name);
		cursor.get(2, ret.password_state);
		cursor.get(3, ret.password);
	}
	_getUserById.reset();
	return ret;
}

/**
* @-SQLGenAccess
* @func User DbDao::getUserByName
* @return int64 id, string name, int password_state, string password
* @sql
*      SELECT id, name, password_state, password FROM users WHERE name=:name(string)
*/
DbDao::User DbDao::getUserByName(const std::string& name)
{
	if(!_getUserByName.prepared())
	{
		_getUserByName=db.prepare("SELECT id, name, password_state, password FROM users WHERE name=?");
	}
	_getUserByName.bind(name);
	auto& cursor=_getUserByName.cursor();
	User ret = { false, 0, "", 0, "" };
	if(cursor.next())
	{
		ret.exists=true;
		cursor.get(0, ret.id);
		cursor.get(1, ret.name);
		cursor.get(2, ret.password_state);
		cursor.get(3, ret.password);
	}
	_getUserByName.reset();
	return ret;
}

/**
* @-SQLGenAccess
* @func int64_t DbDao::addUser
* @return int64_raw id
* @sql
*      INSERT INTO users (name, password_state, password, system) VALUES (:name(string), :password_state(int), :password(string), :system(int)) RETURNING id
*/
int64_t DbDao::addUser(const std::string& name, int password_state, const std::string& password, int system)
{
	if(!_addUser.prepared())
	{
		_addUser=db.prepare("INSERT INTO users (name, password_state, password, system) VALUES (?, ?, ?, ?) RETURNING id");
	}
	_addUser.bind(name);
	_addUser.bind(password_state);
	_addUser.bind(password);
	_addUser.bind(system);
	auto& cursor=_addUser.cursor();
	assert(cursor.next());
	int64_t ret;
	cursor.get(0, ret);
	_addUser.reset();
	return ret;
}

/**
* @-SQLGenAccess
* @func optional<int64_t> DbDao::hasUser
* @return int64 id
* @sql
*	SELECT id FROM users LIMIT 1;
*/
std::optional<int64_t> DbDao::hasUser()
{
	if(!_hasUser.prepared())
	{
		_hasUser=db.prepare("SELECT id FROM users LIMIT 1;");
	}
	auto& cursor=_hasUser.cursor();
	if(!cursor.next())
	{
		return {};
	}
	int64_t ret;
	cursor.get(0, ret);
	return ret;
}

/**
* @-SQLGenAccess
* @func int64_t DbDao::addAccessKey
* @return int64_raw id
* @sql
*      INSERT INTO access_keys (user_id, description, key, secret_key, system) VALUES (:user_id(int64), :description(string), :key(string), :secret_key(string), :system(int)) RETURNING id
*/
int64_t DbDao::addAccessKey(int64_t user_id, const std::string& description, const std::string& key, const std::string& secret_key, int system)
{
	if(!_addAccessKey.prepared())
	{
		_addAccessKey=db.prepare("INSERT INTO access_keys (user_id, description, key, secret_key, system) VALUES (?, ?, ?, ?, ?) RETURNING id");
	}
	_addAccessKey.bind(user_id);
	_addAccessKey.bind(description);
	_addAccessKey.bind(key);
	_addAccessKey.bind(secret_key);
	_addAccessKey.bind(system);
	auto& cursor=_addAccessKey.cursor();
	assert(cursor.next());
	int64_t ret;
	cursor.get(0, ret);
	_addAccessKey.reset();
	return ret;
}

/**
* @-SQLGenAccess
* @func int64_t DbDao::addRole
* @return int64_raw id
* @sql
*      INSERT INTO roles (name, system) VALUES (:name(string), :system(int)) RETURNING id
*/
int64_t DbDao::addRole(const std::string& name, int system)
{
	if(!_addRole.prepared())
	{
		_addRole=db.prepare("INSERT INTO roles (name, system) VALUES (?, ?) RETURNING id");
	}
	_addRole.bind(name);
	_addRole.bind(system);
	auto& cursor=_addRole.cursor();
	assert(cursor.next());
	int64_t ret;
	cursor.get(0, ret);
	_addRole.reset();
	return ret;
}

/**
* @-SQLGenAccess
* @func int64_t DbDao::addPolicy
* @return int64_raw id
* @sql
*      INSERT INTO policies (name, description, ver, data, system) VALUES (:name(string), :description(string), :ver(int), :data(string), :system(int)) RETURNING id
*/
int64_t DbDao::addPolicy(const std::string& name, const std::string& description, int ver, const std::string& data, int system)
{
	if(!_addPolicy.prepared())
	{
		_addPolicy=db.prepare("INSERT INTO policies (name, description, ver, data, system) VALUES (?, ?, ?, ?, ?) RETURNING id");
	}
	_addPolicy.bind(name);
	_addPolicy.bind(description);
	_addPolicy.bind(ver);
	_addPolicy.bind(data);
	_addPolicy.bind(system);
	auto& cursor=_addPolicy.cursor();
	assert(cursor.next());
	int64_t ret;
	cursor.get(0, ret);
	_addPolicy.reset();
	return ret;
}


/**
* @-SQLGenAccess
* @func int64_t DbDao::addUserRole
* @return int64_raw id
* @sql
*      INSERT INTO user_roles (user_id, role_id, system) VALUES (:user_id(int64), :role_id(int64), :system(int)) RETURNING id
*/
int64_t DbDao::addUserRole(int64_t user_id, int64_t role_id, int system)
{
	if(!_addUserRole.prepared())
	{
		_addUserRole=db.prepare("INSERT INTO user_roles (user_id, role_id, system) VALUES (?, ?, ?) RETURNING id");
	}
	_addUserRole.bind(user_id);
	_addUserRole.bind(role_id);
	_addUserRole.bind(system);
	auto& cursor=_addUserRole.cursor();
	assert(cursor.next());
	int64_t ret;
	cursor.get(0, ret);
	_addUserRole.reset();
	return ret;
}

/**
* @-SQLGenAccess
* @func int64_t DbDao::addRolePolicy
* @return int64_raw id
* @sql
*      INSERT INTO role_policies (role_id, policy_id, system) VALUES (:role_id(int64), :policy_id(int64), :system(int)) RETURNING id
*/
int64_t DbDao::addRolePolicy(int64_t role_id, int64_t policy_id, int system)
{
	if(!_addRolePolicy.prepared())
	{
		_addRolePolicy=db.prepare("INSERT INTO role_policies (role_id, policy_id, system) VALUES (?, ?, ?) RETURNING id");
	}
	_addRolePolicy.bind(role_id);
	_addRolePolicy.bind(policy_id);
	_addRolePolicy.bind(system);
	auto& cursor=_addRolePolicy.cursor();
	assert(cursor.next());
	int64_t ret;
	cursor.get(0, ret);
	_addRolePolicy.reset();
	return ret;
}


/**
* @-SQLGenAccess
* @func void DbDao::deleteUser
* @sql
*      DELETE FROM users WHERE id=:id(int64)
*/
void DbDao::deleteUser(int64_t id)
{
	if(!_deleteUser.prepared())
	{
		_deleteUser=db.prepare("DELETE FROM users WHERE id=?");
	}
	_deleteUser.bind(id);
	_deleteUser.write();
	_deleteUser.reset();
}

/**
* @-SQLGenAccess
* @func vector<AccessKey> DbDao::getAccessKeys
* @return int64 id, int64 user_id, int64 created, string description, string key, string secret_key
* @sql
*      SELECT id, user_id, created, description, key, secret_key FROM access_keys
*/
std::vector<DbDao::AccessKey> DbDao::getAccessKeys()
{
	if(!_getAccessKeys.prepared())
	{
		_getAccessKeys=db.prepare("SELECT id, user_id, created, description, key, secret_key FROM access_keys");
	}
	auto& cursor=_getAccessKeys.cursor();
	std::vector<DbDao::AccessKey> ret;
	while(cursor.next())
	{
		ret.emplace_back();
		DbDao::AccessKey& obj=ret.back();
		cursor.get(0, obj.id);
		cursor.get(1, obj.user_id);
		cursor.get(2, obj.created);
		cursor.get(3, obj.description);
		cursor.get(4, obj.key);
		cursor.get(5, obj.secret_key);
	}
	return ret;
}

/**
* @-SQLGenAccess
* @func vector<Bucket> DbDao::getBuckets
* @return int64 id, string name
* @sql
*      SELECT id, name FROM buckets
*/
std::vector<DbDao::Bucket> DbDao::getBuckets()
{
	if(!_getBuckets.prepared())
	{
		_getBuckets=db.prepare("SELECT id, name FROM buckets");
	}
	auto& cursor=_getBuckets.cursor();
	std::vector<DbDao::Bucket> ret;
	while(cursor.next())
	{
		ret.emplace_back();
		DbDao::Bucket& obj=ret.back();
		cursor.get(0, obj.id);
		cursor.get(1, obj.name);
	}
	return ret;
}

/**
* @-SQLGenAccess
* @func void DbDao::addBucket
* @sql
*      INSERT INTO buckets (id, name) VALUES (:id(int64), :name(string))
*/
void DbDao::addBucket(int64_t id, const std::string& name)
{
	if(!_addBucket.prepared())
	{
		_addBucket=db.prepare("INSERT INTO buckets (id, name) VALUES (?, ?)");
	}
	_addBucket.bind(id);
	_addBucket.bind(name);
	_addBucket.write();
	_addBucket.reset();
}

/**
* @-SQLGenAccess
* @func int64_t DbDao::getMaxBucketId
* @return int64_raw id
* @sql
*      SELECT MAX(id) AS id FROM buckets
*/
int64_t DbDao::getMaxBucketId()
{
	if(!_getMaxBucketId.prepared())
	{
		_getMaxBucketId=db.prepare("SELECT MAX(id) AS id FROM buckets");
	}
	auto& cursor=_getMaxBucketId.cursor();
	assert(cursor.next());
	int64_t ret;
	cursor.get(0, ret);
	return ret;
}


//eof