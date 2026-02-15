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

void upgrade1_2(sqlgen::Database& db)
{
	db.write(R"""(
		CREATE TABLE policy_statements (
			id INTEGER PRIMARY KEY,
			policy_id INTEGER NOT NULL REFERENCES policies(id) ON DELETE CASCADE,
			sid TEXT NOT NULL,
			effect INTEGER NOT NULL
		) STRICT;
	)""");

	db.write(R"""(
		CREATE TABLE policy_statement_actions (
			id INTEGER PRIMARY KEY,
			statement_id INTEGER NOT NULL REFERENCES policy_statements(id) ON DELETE CASCADE,
			action INTEGER NOT NULL
		) STRICT;
	)""");

	// TODO: Rename policy_resources => policy_statement_resources
	db.write(R"""(
		CREATE TABLE policy_resources (
			id INTEGER PRIMARY KEY,
			statement_id INTEGER NOT NULL REFERENCES policy_statements(id) ON DELETE CASCADE,
			resource TEXT NOT NULL
		) STRICT;
	)""");

	db.write(R"""(
		CREATE TABLE groups (
			id INTEGER PRIMARY KEY,
			name TEXT NOT NULL,
			created INTEGER NOT NULL DEFAULT (unixepoch())
		) STRICT;
	)""");

	db.write(R"""(
		CREATE TABLE user_groups (
			id INTEGER PRIMARY KEY,
			user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
			group_id INTEGER NOT NULL REFERENCES groups(id) ON DELETE CASCADE,
			created INTEGER NOT NULL DEFAULT (unixepoch()),
			UNIQUE(user_id, group_id)
		) STRICT;
	)""");

	db.write(R"""(
		CREATE TABLE group_policies (
			id INTEGER PRIMARY KEY,
			group_id INTEGER NOT NULL REFERENCES groups(id) ON DELETE CASCADE,
			policy_id INTEGER NOT NULL REFERENCES policies(id) ON DELETE CASCADE,
			created INTEGER NOT NULL DEFAULT (unixepoch()),
			UNIQUE(group_id, policy_id)
		) STRICT;
	)""");
}

const std::map<int64_t, upgrade_func_t*> upgrade_funcs {
	{0, upgrade0_1},
	{1, upgrade1_2}
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
* @return int64 id, string name, int password_state, string password, int system
* @sql
*      SELECT id, name, password_state, password, system FROM users
*/
std::vector<DbDao::User> DbDao::getUsers()
{
	if(!_getUsers.prepared())
	{
		_getUsers=db.prepare("SELECT id, name, password_state, password, system FROM users");
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
		cursor.get(4, obj.system);
	}
	cursor.shutdown();
	return ret;
}

/**
* @-SQLGenAccess
* @func optional<User> DbDao::getUserById
* @return int64 id, string name, int password_state, string password, int system
* @sql
*      SELECT id, name, password_state, password, system FROM users WHERE id=:id(int64)
*/
std::optional<DbDao::User> DbDao::getUserById(int64_t id)
{
	if(!_getUserById.prepared())
	{
		_getUserById=db.prepare("SELECT id, name, password_state, password, system FROM users WHERE id=?");
	}
	_getUserById.bind(id);
	auto& cursor=_getUserById.cursor();
	if(cursor.next())
	{
		User ret;
		cursor.get(0, ret.id);
		cursor.get(1, ret.name);
		cursor.get(2, ret.password_state);
		cursor.get(3, ret.password);
		cursor.get(4, ret.system);
		_getUserById.reset();
		return ret;
	}
	_getUserById.reset();
	return {};
}

/**
* @-SQLGenAccess
* @func User DbDao::getUserByName
* @return int64 id, string name, int password_state, string password, int system
* @sql
*      SELECT id, name, password_state, password, system FROM users WHERE name=:name(string)
*/
DbDao::User DbDao::getUserByName(const std::string& name)
{
	if(!_getUserByName.prepared())
	{
		_getUserByName=db.prepare("SELECT id, name, password_state, password, system FROM users WHERE name=?");
	}
	_getUserByName.bind(name);
	auto& cursor=_getUserByName.cursor();
	DbDao::User ret = { false, 0, "", 0, "", 0 };
	if(cursor.next())
	{
		ret.exists=true;
		cursor.get(0, ret.id);
		cursor.get(1, ret.name);
		cursor.get(2, ret.password_state);
		cursor.get(3, ret.password);
		cursor.get(4, ret.system);
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
	const auto hasNext = cursor.next();
	assert(hasNext);
	int64_t ret;
	cursor.get(0, ret);
	_addUser.reset();
	return ret;
}

/**
* @-SQLGenAccess
* @func void DbDao::removeUser
* @sql
*      DELETE FROM users WHERE id=:id(int64)
*/
void DbDao::removeUser(int64_t id)
{
	if(!_removeUser.prepared())
	{
		_removeUser=db.prepare("DELETE FROM users WHERE id=?");
	}
	_removeUser.bind(id);
	_removeUser.write();
	_removeUser.reset();
}


/**
* @-SQLGenAccess
* @func optional<int64> DbDao::hasUser
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
		cursor.shutdown();
		return {};
	}
	int64_t ret;
	cursor.get(0, ret);
	cursor.shutdown();
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
	const auto hasNext = cursor.next();
	assert(hasNext);
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
	const auto hasNext = cursor.next();
	assert(hasNext);
	int64_t ret;
	cursor.get(0, ret);
	_addRole.reset();
	return ret;
}

/**
* @-SQLGenAccess
* @func void DbDao::removeRole
* @sql
*      DELETE FROM roles WHERE id=:id(int64)
*/
void DbDao::removeRole(int64_t id)
{
	if(!_removeRole.prepared())
	{
		_removeRole=db.prepare("DELETE FROM roles WHERE id=?");
	}
	_removeRole.bind(id);
	_removeRole.write();
	_removeRole.reset();
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
	const auto hasNext = cursor.next();
	assert(hasNext);
	int64_t ret;
	cursor.get(0, ret);
	_addPolicy.reset();
	return ret;
}

/**
* @-SQLGenAccess
* @func void DbDao::removePolicy
* @sql
*      DELETE FROM policies WHERE id=:id(int64)
*/
void DbDao::removePolicy(int64_t id)
{
	if(!_removePolicy.prepared())
	{
		_removePolicy=db.prepare("DELETE FROM policies WHERE id=?");
	}
	_removePolicy.bind(id);
	_removePolicy.write();
	_removePolicy.reset();
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
	const auto hasNext = cursor.next();
	assert(hasNext);
	int64_t ret;
	cursor.get(0, ret);
	_addUserRole.reset();
	return ret;
}

/**
* @-SQLGenAccess
* @func void DbDao::removeUserRole
* @sql
*      DELETE FROM user_roles WHERE id=:id(int64)
*/
void DbDao::removeUserRole(int64_t id)
{
	if(!_removeUserRole.prepared())
	{
		_removeUserRole=db.prepare("DELETE FROM user_roles WHERE id=?");
	}
	_removeUserRole.bind(id);
	_removeUserRole.write();
	_removeUserRole.reset();
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
	const auto hasNext = cursor.next();
	assert(hasNext);
	int64_t ret;
	cursor.get(0, ret);
	_addRolePolicy.reset();
	return ret;
}


/**
* @-SQLGenAccess
* @func void DbDao::removeRolePolicy
* @sql
*      DELETE FROM role_policies WHERE id=:id(int64)
*/
void DbDao::removeRolePolicy(int64_t id)
{
	if(!_removeRolePolicy.prepared())
	{
		_removeRolePolicy=db.prepare("DELETE FROM role_policies WHERE id=?");
	}
	_removeRolePolicy.bind(id);
	_removeRolePolicy.write();
	_removeRolePolicy.reset();
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
* @func optional<AccessKey> DbDao::getAccessKey
* @return int64 id, int64 user_id, int64 created, string description, string key, string secret_key
* @sql
*      SELECT id, user_id, created, description, key, secret_key FROM access_keys WHERE id=:id(int64)
*/
std::optional<DbDao::AccessKey> DbDao::getAccessKey(int64_t id)
{
	if(!_getAccessKey.prepared())
	{
		_getAccessKey=db.prepare("SELECT id, user_id, created, description, key, secret_key FROM access_keys WHERE id=?");
	}
	_getAccessKey.bind(id);
	auto& cursor=_getAccessKey.cursor();
	if(cursor.next())
	{
		AccessKey ret;
		cursor.get(0, ret.id);
		cursor.get(1, ret.user_id);
		cursor.get(2, ret.created);
		cursor.get(3, ret.description);
		cursor.get(4, ret.key);
		cursor.get(5, ret.secret_key);
		_getAccessKey.reset();
		return ret;
	}
	_getAccessKey.reset();
	return {};
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
	cursor.shutdown();
	return ret;
}



/**
* @-SQLGenAccess
* @func vector<AccessKey> DbDao::getAccessKeysOfUser
* @return int64 id, int64 user_id, int64 created, string description, string key, string secret_key
* @sql
*      SELECT id, user_id, created, description, key, secret_key FROM access_keys WHERE user_id=:user_id(int64)
*/
std::vector<DbDao::AccessKey> DbDao::getAccessKeysOfUser(int64_t user_id)
{
	if(!_getAccessKeysOfUser.prepared())
	{
		_getAccessKeysOfUser=db.prepare("SELECT id, user_id, created, description, key, secret_key FROM access_keys WHERE user_id=?");
	}
	_getAccessKeysOfUser.bind(user_id);
	auto& cursor=_getAccessKeysOfUser.cursor();
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
	_getAccessKeysOfUser.reset();
	return ret;
}

/**
* @-SQLGenAccess
* @func void DbDao::removeAccessKey
* @sql
*      DELETE FROM access_keys WHERE id=:id(int64)
*/
void DbDao::removeAccessKey(int64_t id)
{
	if(!_removeAccessKey.prepared())
	{
		_removeAccessKey=db.prepare("DELETE FROM access_keys WHERE id=?");
	}
	_removeAccessKey.bind(id);
	_removeAccessKey.write();
	_removeAccessKey.reset();
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
	cursor.shutdown();
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
	const auto hasNext = cursor.next();
	assert(hasNext);
	int64_t ret;
	cursor.get(0, ret);
	cursor.shutdown();
	return ret;
}

/**
* @-SQLGenAccess
* @func void DbDao::deleteBucket
* @sql
*      DELETE FROM buckets WHERE id=:id(int64)
*/
void DbDao::deleteBucket(int64_t id)
{
	if(!_deleteBucket.prepared())
	{
		_deleteBucket=db.prepare("DELETE FROM buckets WHERE id=?");
	}
	_deleteBucket.bind(id);
	_deleteBucket.write();
	_deleteBucket.reset();
}

/**
* @-SQLGenAccess
* @func vector<PolicyStatement> DbDao::getPolicyStatements
* @return int64 id, string sid, int effect
* @sql
*      SELECT id, sid, effect FROM policy_statements WHERE policy_id=:policy_id(int64)
*/
std::vector<DbDao::PolicyStatement> DbDao::getPolicyStatements(int64_t policy_id)
{
	if(!_getPolicyStatements.prepared())
	{
		_getPolicyStatements=db.prepare("SELECT id, sid, effect FROM policy_statements WHERE policy_id=?");
	}
	_getPolicyStatements.bind(policy_id);
	auto& cursor=_getPolicyStatements.cursor();
	std::vector<DbDao::PolicyStatement> ret;
	while(cursor.next())
	{
		ret.emplace_back();
		DbDao::PolicyStatement& obj=ret.back();
		cursor.get(0, obj.id);
		cursor.get(1, obj.sid);
		cursor.get(2, obj.effect);
	}
	_getPolicyStatements.reset();
	return ret;
}

/**
* @-SQLGenAccess
* @func vector<int> DbDao::getPolicyStatementActions
* @return int action
* @sql
*      SELECT action FROM policy_statement_actions WHERE statement_id=:statement_id(int64)
*/
std::vector<int> DbDao::getPolicyStatementActions(int64_t statement_id)
{
	if(!_getPolicyStatementActions.prepared())
	{
		_getPolicyStatementActions=db.prepare("SELECT action FROM policy_statement_actions WHERE statement_id=?");
	}
	_getPolicyStatementActions.bind(statement_id);
	auto& cursor=_getPolicyStatementActions.cursor();
	std::vector<int> ret;
	while(cursor.next())
	{
		ret.emplace_back();
		cursor.get(0, ret.back());
	}
	_getPolicyStatementActions.reset();
	return ret;
}

/**
* @-SQLGenAccess
* @func vector<string> DbDao::getPolicyStatementResources
* @return string resource
* @sql
*      SELECT resource FROM policy_resources WHERE statement_id=:statement_id(int64)
*/
std::vector<std::string> DbDao::getPolicyStatementResources(int64_t statement_id)
{
	if(!_getPolicyStatementResources.prepared())
	{
		_getPolicyStatementResources=db.prepare("SELECT resource FROM policy_resources WHERE statement_id=?");
	}
	_getPolicyStatementResources.bind(statement_id);
	auto& cursor=_getPolicyStatementResources.cursor();
	std::vector<std::string> ret;
	while(cursor.next())
	{
		ret.emplace_back();
		cursor.get(0, ret.back());
	}
	_getPolicyStatementResources.reset();
	return ret;
}

/**
* @-SQLGenAccess
* @func vector<Policy> DbDao::getPolicies
* @return int64 id, string name, string description, int64 created, int64 modified, int ver, string data, int system
* @sql
*      SELECT id, name, description, created, modified, ver, data, system FROM policies
*/
std::vector<DbDao::Policy> DbDao::getPolicies()
{
	if(!_getPolicies.prepared())
	{
		_getPolicies=db.prepare("SELECT id, name, description, created, modified, ver, data, system FROM policies");
	}
	auto& cursor=_getPolicies.cursor();
	std::vector<DbDao::Policy> ret;
	while(cursor.next())
	{
		ret.emplace_back();
		DbDao::Policy& obj=ret.back();
		cursor.get(0, obj.id);
		cursor.get(1, obj.name);
		cursor.get(2, obj.description);
		cursor.get(3, obj.created);
		cursor.get(4, obj.modified);
		cursor.get(5, obj.ver);
		cursor.get(6, obj.data);
		cursor.get(7, obj.system);
	}
	cursor.shutdown();
	return ret;
}

/**
* @-SQLGenAccess
* @func void DbDao::changePolicy
* @sql
*      UPDATE policies SET data=:data(string), modified=(unixepoch()) WHERE id=:id(int64)
*/
void DbDao::changePolicy(const std::string& data, int64_t id)
{
	if(!_changePolicy.prepared())
	{
		_changePolicy=db.prepare("UPDATE policies SET data=?, modified=(unixepoch()) WHERE id=?");
	}
	_changePolicy.bind(data);
	_changePolicy.bind(id);
	_changePolicy.write();
	_changePolicy.reset();
}

/**
* @-SQLGenAccess
* @func void DbDao::deletePolicyStatements
* @sql
*      DELETE FROM policy_statements WHERE policy_id=:policy_id(int64)
*/
void DbDao::deletePolicyStatements(int64_t policy_id)
{
	if(!_deletePolicyStatements.prepared())
	{
		_deletePolicyStatements=db.prepare("DELETE FROM policy_statements WHERE policy_id=?");
	}
	_deletePolicyStatements.bind(policy_id);
	_deletePolicyStatements.write();
	_deletePolicyStatements.reset();
}

/**
* @-SQLGenAccess
* @func vector<PolicyOfRole> DbDao::getPoliciesOfRole
* @return int64 id, int64 policy_id, string name, string description, int64 created, int64 modified, int ver, string data, int system
* @sql
*      SELECT rp.id AS id, p.id AS policy_id, name, description, p.created AS created, modified, ver, data, p.system AS system
*		FROM (policies p INNER JOIN role_policies rp ON p.id=rp.policy_id)
*		WHERE rp.role_id=:role_id(int64)
*/
std::vector<DbDao::PolicyOfRole> DbDao::getPoliciesOfRole(int64_t role_id)
{
	if(!_getPoliciesOfRole.prepared())
	{
		_getPoliciesOfRole=db.prepare("SELECT rp.id AS id, p.id AS policy_id, name, description, p.created AS created, modified, ver, data, p.system AS system FROM (policies p INNER JOIN role_policies rp ON p.id=rp.policy_id) WHERE rp.role_id=?");
	}
	_getPoliciesOfRole.bind(role_id);
	auto& cursor=_getPoliciesOfRole.cursor();
	std::vector<DbDao::PolicyOfRole> ret;
	while(cursor.next())
	{
		ret.emplace_back();
		DbDao::PolicyOfRole& obj=ret.back();
		cursor.get(0, obj.id);
		cursor.get(1, obj.policy_id);
		cursor.get(2, obj.name);
		cursor.get(3, obj.description);
		cursor.get(4, obj.created);
		cursor.get(5, obj.modified);
		cursor.get(6, obj.ver);
		cursor.get(7, obj.data);
		cursor.get(8, obj.system);
	}
	_getPoliciesOfRole.reset();
	return ret;
}

/**
* @-SQLGenAccess
* @func optional<Policy> DbDao::getPolicy
* @return int64 id, string name, string description, int64 created, int64 modified, int ver, string data
* @sql
*      SELECT id, name, description, created, modified, ver, data FROM policies WHERE id=:id(int64)
*/
std::optional<DbDao::Policy> DbDao::getPolicy(int64_t id)
{
	if(!_getPolicy.prepared())
	{
		_getPolicy=db.prepare("SELECT id, name, description, created, modified, ver, data FROM policies WHERE id=?");
	}
	_getPolicy.bind(id);
	auto& cursor=_getPolicy.cursor();
	if(cursor.next())
	{
		Policy ret;
		cursor.get(0, ret.id);
		cursor.get(1, ret.name);
		cursor.get(2, ret.description);
		cursor.get(3, ret.created);
		cursor.get(4, ret.modified);
		cursor.get(5, ret.ver);
		cursor.get(6, ret.data);
		_getPolicy.reset();
		return ret;
	}
	_getPolicy.reset();
	return {};
}

/**
* @-SQLGenAccess
* @func vector<Role> DbDao::getRoles
* @return int64 id, string name, int system
* @sql
*      SELECT id, name, system FROM roles
*/
std::vector<DbDao::Role> DbDao::getRoles()
{
	if(!_getRoles.prepared())
	{
		_getRoles=db.prepare("SELECT id, name, system FROM roles");
	}
	auto& cursor=_getRoles.cursor();
	std::vector<DbDao::Role> ret;
	while(cursor.next())
	{
		ret.emplace_back();
		DbDao::Role& obj=ret.back();
		cursor.get(0, obj.id);
		cursor.get(1, obj.name);
		cursor.get(2, obj.system);
	}
	cursor.shutdown();
	return ret;
}

/**
* @-SQLGenAccess
* @func vector<UserRole> DbDao::getRolesByUserId
* @return int64 id, int64 role_id, string name, int system
* @sql
*      SELECT ur.id AS id, r.id AS role_id, name, ur.system AS system 
*		FROM (roles r INNER JOIN user_roles ur ON r.id=ur.role_id) WHERE ur.user_id=:user_id(int64)
*/
std::vector<DbDao::UserRole> DbDao::getRolesByUserId(int64_t user_id)
{
	if(!_getRolesByUserId.prepared())
	{
		_getRolesByUserId=db.prepare("SELECT ur.id AS id, r.id AS role_id, name, ur.system AS system  FROM (roles r INNER JOIN user_roles ur ON r.id=ur.role_id) WHERE ur.user_id=?");
	}
	_getRolesByUserId.bind(user_id);
	auto& cursor=_getRolesByUserId.cursor();
	std::vector<DbDao::UserRole> ret;
	while(cursor.next())
	{
		ret.emplace_back();
		DbDao::UserRole& obj=ret.back();
		cursor.get(0, obj.id);
		cursor.get(1, obj.role_id);
		cursor.get(2, obj.name);
		cursor.get(3, obj.system);
	}
	_getRolesByUserId.reset();
	return ret;
}

/**
* @-SQLGenAccess
* @func optional<Role> DbDao::getRole
* @return int64 id, string name, int system
* @sql
*      SELECT id, name, system FROM roles WHERE id=:id(int64)
*/
std::optional<DbDao::Role> DbDao::getRole(int64_t id)
{
	if(!_getRole.prepared())
	{
		_getRole=db.prepare("SELECT id, name, system FROM roles WHERE id=?");
	}
	_getRole.bind(id);
	auto& cursor=_getRole.cursor();
	if(cursor.next())
	{
		Role ret;
		cursor.get(0, ret.id);
		cursor.get(1, ret.name);
		cursor.get(2, ret.system);
		_getRole.reset();
		return ret;
	}
	_getRole.reset();
	return {};
}

/**
* @-SQLGenAccess
* @func vector<RolePolicy> DbDao::getRolePolicies
* @return int64 id, int64 policy_id, int64 role_id, int system
* @sql
*      SELECT id, policy_id, role_id, system FROM role_policies WHERE role_id=:role_id(int64)
*/
std::vector<DbDao::RolePolicy> DbDao::getRolePolicies(int64_t role_id)
{
	if(!_getRolePolicies.prepared())
	{
		_getRolePolicies=db.prepare("SELECT id, policy_id, role_id, system FROM role_policies WHERE role_id=?");
	}
	_getRolePolicies.bind(role_id);
	auto& cursor=_getRolePolicies.cursor();
	std::vector<DbDao::RolePolicy> ret;
	while(cursor.next())
	{
		ret.emplace_back();
		DbDao::RolePolicy& obj=ret.back();
		cursor.get(0, obj.id);
		cursor.get(1, obj.policy_id);
		cursor.get(2, obj.role_id);
		cursor.get(3, obj.system);
	}
	_getRolePolicies.reset();
	return ret;
}

/**
* @-SQLGenAccess
* @func optional<RolePolicy> DbDao::getRolePolicyById
* @return int64 id, int64 policy_id, int64 role_id, int system
* @sql
*      SELECT id, policy_id, role_id, system FROM role_policies WHERE id=:id(int64)
*/
std::optional<DbDao::RolePolicy> DbDao::getRolePolicyById(int64_t id)
{
	if(!_getRolePolicyById.prepared())
	{
		_getRolePolicyById=db.prepare("SELECT id, policy_id, role_id, system FROM role_policies WHERE id=?");
	}
	_getRolePolicyById.bind(id);
	auto& cursor=_getRolePolicyById.cursor();
	if(cursor.next())
	{
		RolePolicy ret;
		cursor.get(0, ret.id);
		cursor.get(1, ret.policy_id);
		cursor.get(2, ret.role_id);
		cursor.get(3, ret.system);
		_getRolePolicyById.reset();
		return ret;
	}
	_getRolePolicyById.reset();
	return {};
}

/**
* @-SQLGenAccess
* @func vector<UserRole> DbDao::getUserRoles
* @return int64 role_id, int system
* @sql
*      SELECT role_id, system FROM user_roles WHERE user_id=:user_id(int64)
*/
std::vector<DbDao::UserRole> DbDao::getUserRoles(int64_t user_id)
{
	if(!_getUserRoles.prepared())
	{
		_getUserRoles=db.prepare("SELECT role_id, system FROM user_roles WHERE user_id=?");
	}
	_getUserRoles.bind(user_id);
	auto& cursor=_getUserRoles.cursor();
	std::vector<DbDao::UserRole> ret;
	while(cursor.next())
	{
		ret.emplace_back();
		DbDao::UserRole& obj=ret.back();
		cursor.get(0, obj.role_id);
		cursor.get(1, obj.system);
	}
	_getUserRoles.reset();
	return ret;
}

/**
* @-SQLGenAccess
* @func optional<UserRole> DbDao::getUserRole
* @return int64 role_id, int system
* @sql
*      SELECT role_id, system FROM user_roles WHERE id=:id(int64)
*/
std::optional<DbDao::UserRole> DbDao::getUserRole(int64_t id)
{
	if(!_getUserRole.prepared())
	{
		_getUserRole=db.prepare("SELECT role_id, system FROM user_roles WHERE id=?");
	}
	_getUserRole.bind(id);
	auto& cursor=_getUserRole.cursor();
	if(cursor.next())
	{
		UserRole ret;
		cursor.get(0, ret.role_id);
		cursor.get(1, ret.system);
		_getUserRole.reset();
		return ret;
	}
	_getUserRole.reset();
	return {};
}

/**
* @-SQLGenAccess
* @func int64_t DbDao::addPolicyStatement
* @return int64_raw id
* @sql
*      INSERT INTO policy_statements (policy_id, sid, effect) VALUES (:policy_id(int64), :sid(string), :effect(int)) RETURNING id
*/
int64_t DbDao::addPolicyStatement(int64_t policy_id, const std::string& sid, int effect)
{
	if(!_addPolicyStatement.prepared())
	{
		_addPolicyStatement=db.prepare("INSERT INTO policy_statements (policy_id, sid, effect) VALUES (?, ?, ?) RETURNING id");
	}
	_addPolicyStatement.bind(policy_id);
	_addPolicyStatement.bind(sid);
	_addPolicyStatement.bind(effect);
	auto& cursor=_addPolicyStatement.cursor();
	const auto hasNext = cursor.next();
	assert(hasNext);
	int64_t ret;
	cursor.get(0, ret);
	_addPolicyStatement.reset();
	return ret;
}

/**
* @-SQLGenAccess
* @func int64_t DbDao::addPolicyStatementAction
* @return int64_raw id
* @sql
*      INSERT INTO policy_statement_actions (statement_id, action) VALUES (:statement_id(int64), :action(int)) RETURNING id
*/
int64_t DbDao::addPolicyStatementAction(int64_t statement_id, int action)
{
	if(!_addPolicyStatementAction.prepared())
	{
		_addPolicyStatementAction=db.prepare("INSERT INTO policy_statement_actions (statement_id, action) VALUES (?, ?) RETURNING id");
	}
	_addPolicyStatementAction.bind(statement_id);
	_addPolicyStatementAction.bind(action);
	auto& cursor=_addPolicyStatementAction.cursor();
	const auto hasNext = cursor.next();
	assert(hasNext);
	int64_t ret;
	cursor.get(0, ret);
	_addPolicyStatementAction.reset();
	return ret;
}

/**
* @-SQLGenAccess
* @func int64_t DbDao::addPolicyStatementResource
* @return int64_raw id
* @sql
*      INSERT INTO policy_resources (statement_id, resource) VALUES (:statement_id(int64), :resource(string)) RETURNING id
*/
int64_t DbDao::addPolicyStatementResource(int64_t statement_id, const std::string& resource)
{
	if(!_addPolicyStatementResource.prepared())
	{
		_addPolicyStatementResource=db.prepare("INSERT INTO policy_resources (statement_id, resource) VALUES (?, ?) RETURNING id");
	}
	_addPolicyStatementResource.bind(statement_id);
	_addPolicyStatementResource.bind(resource);
	auto& cursor=_addPolicyStatementResource.cursor();
	const auto hasNext = cursor.next();
	assert(hasNext);
	int64_t ret;
	cursor.get(0, ret);
	_addPolicyStatementResource.reset();
	return ret;
}

/**
* @-SQLGenAccess
* @func void DbDao::changeUserPassword
* @sql
*      UPDATE users SET password=:password(string) WHERE id=:user_id(int64)
*/
void DbDao::changeUserPassword(const std::string& password, int64_t user_id)
{
	if(!_changeUserPassword.prepared())
	{
		_changeUserPassword=db.prepare("UPDATE users SET password=? WHERE id=?");
	}
	_changeUserPassword.bind(password);
	_changeUserPassword.bind(user_id);
	_changeUserPassword.write();
	_changeUserPassword.reset();
}



//eof