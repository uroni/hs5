#pragma once

#include <sqlite-cpp-sqlgen/DatabaseQuery.h>
#include <vector>
#include <string>

class DbDao
{
    sqlgen::Database& db = getStaticDb();

    //@-SQLGenVariablesBegin
	sqlgen::DatabaseQuery _getUsers;
	sqlgen::DatabaseQuery _getUserById;
	sqlgen::DatabaseQuery _getUserByName;
	sqlgen::DatabaseQuery _addUser;
	sqlgen::DatabaseQuery _hasUser;
	sqlgen::DatabaseQuery _addAccessKey;
	sqlgen::DatabaseQuery _addRole;
	sqlgen::DatabaseQuery _addPolicy;
	sqlgen::DatabaseQuery _addUserRole;
	sqlgen::DatabaseQuery _addRolePolicy;
	sqlgen::DatabaseQuery _deleteUser;
	sqlgen::DatabaseQuery _getAccessKeys;
	sqlgen::DatabaseQuery _getBuckets;
	sqlgen::DatabaseQuery _addBucket;
	sqlgen::DatabaseQuery _getMaxBucketId;
	sqlgen::DatabaseQuery _deleteBucket;
	//@-SQLGenVariablesEnd

public:

	static sqlgen::Database& getStaticDb();

	sqlgen::Database& getDb() {
		return db;
	}

	static void init();

	int64_t getUserVersion();

	void setUserVersion(int64_t newVersion);

    //@-SQLGenFunctionsBegin
	struct AccessKey
	{
		int64_t id;
		int64_t user_id;
		int64_t created;
		std::string description;
		std::string key;
		std::string secret_key;
	};
	struct Bucket
	{
		int64_t id;
		std::string name;
	};
	struct User
	{
		bool exists;
		int64_t id;
		std::string name;
		int password_state;
		std::string password;
	};


	std::vector<User> getUsers();
	User getUserById(int64_t id);
	User getUserByName(const std::string& name);
	int64_t addUser(const std::string& name, int password_state, const std::string& password, int system);
	std::optional<int64_t> hasUser();
	int64_t addAccessKey(int64_t user_id, const std::string& description, const std::string& key, const std::string& secret_key, int system);
	int64_t addRole(const std::string& name, int system);
	int64_t addPolicy(const std::string& name, const std::string& description, int ver, const std::string& data, int system);
	int64_t addUserRole(int64_t user_id, int64_t role_id, int system);
	int64_t addRolePolicy(int64_t role_id, int64_t policy_id, int system);
	void deleteUser(int64_t id);
	std::vector<AccessKey> getAccessKeys();
	std::vector<Bucket> getBuckets();
	void addBucket(int64_t id, const std::string& name);
	int64_t getMaxBucketId();
	void deleteBucket(int64_t id);
	//@-SQLGenFunctionsEnd
};