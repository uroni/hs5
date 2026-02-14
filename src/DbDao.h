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
	sqlgen::DatabaseQuery _removeUser;
	sqlgen::DatabaseQuery _hasUser;
	sqlgen::DatabaseQuery _addAccessKey;
	sqlgen::DatabaseQuery _addRole;
	sqlgen::DatabaseQuery _removeRole;
	sqlgen::DatabaseQuery _addPolicy;
	sqlgen::DatabaseQuery _removePolicy;
	sqlgen::DatabaseQuery _addUserRole;
	sqlgen::DatabaseQuery _removeUserRole;
	sqlgen::DatabaseQuery _addRolePolicy;
	sqlgen::DatabaseQuery _removeRolePolicy;
	sqlgen::DatabaseQuery _deleteUser;
	sqlgen::DatabaseQuery _getAccessKey;
	sqlgen::DatabaseQuery _getAccessKeys;
	sqlgen::DatabaseQuery _getAccessKeysOfUser;
	sqlgen::DatabaseQuery _removeAccessKey;
	sqlgen::DatabaseQuery _getBuckets;
	sqlgen::DatabaseQuery _addBucket;
	sqlgen::DatabaseQuery _getMaxBucketId;
	sqlgen::DatabaseQuery _deleteBucket;
	sqlgen::DatabaseQuery _getPolicyStatements;
	sqlgen::DatabaseQuery _getPolicyStatementActions;
	sqlgen::DatabaseQuery _getPolicyStatementResources;
	sqlgen::DatabaseQuery _getPolicies;
	sqlgen::DatabaseQuery _changePolicy;
	sqlgen::DatabaseQuery _deletePolicyStatements;
	sqlgen::DatabaseQuery _getPoliciesOfRole;
	sqlgen::DatabaseQuery _getPolicy;
	sqlgen::DatabaseQuery _getRoles;
	sqlgen::DatabaseQuery _getRolesByUserId;
	sqlgen::DatabaseQuery _getRole;
	sqlgen::DatabaseQuery _getRolePolicies;
	sqlgen::DatabaseQuery _getRolePolicyById;
	sqlgen::DatabaseQuery _getUserRoles;
	sqlgen::DatabaseQuery _addPolicyStatement;
	sqlgen::DatabaseQuery _addPolicyStatementAction;
	sqlgen::DatabaseQuery _addPolicyStatementResource;
	sqlgen::DatabaseQuery _changeUserPassword;
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
	struct Policy
	{
		int64_t id;
		std::string name;
		std::string description;
		int64_t created;
		int64_t modified;
		int ver;
		std::string data;
		int system;
	};
	struct PolicyOfRole
	{
		int64_t id;
		int64_t policy_id;
		std::string name;
		std::string description;
		int64_t created;
		int64_t modified;
		int ver;
		std::string data;
		int system;
	};
	struct PolicyStatement
	{
		int64_t id;
		std::string sid;
		int effect;
	};
	struct Role
	{
		int64_t id;
		std::string name;
		int system;
	};
	struct RolePolicy
	{
		int64_t id;
		int64_t policy_id;
		int64_t role_id;
		int system;
	};
	struct User
	{
		bool exists;
		int64_t id;
		std::string name;
		int password_state;
		std::string password;
		int system;
	};
	struct UserRole
	{
		int64_t role_id;
		int system;
	};


	std::vector<User> getUsers();
	std::optional<User> getUserById(int64_t id);
	User getUserByName(const std::string& name);
	int64_t addUser(const std::string& name, int password_state, const std::string& password, int system);
	void removeUser(int64_t id);
	std::optional<int64_t> hasUser();
	int64_t addAccessKey(int64_t user_id, const std::string& description, const std::string& key, const std::string& secret_key, int system);
	int64_t addRole(const std::string& name, int system);
	void removeRole(int64_t id);
	int64_t addPolicy(const std::string& name, const std::string& description, int ver, const std::string& data, int system);
	void removePolicy(int64_t id);
	int64_t addUserRole(int64_t user_id, int64_t role_id, int system);
	void removeUserRole(int64_t id);
	int64_t addRolePolicy(int64_t role_id, int64_t policy_id, int system);
	void removeRolePolicy(int64_t id);
	void deleteUser(int64_t id);
	std::optional<AccessKey> getAccessKey(int64_t id);
	std::vector<AccessKey> getAccessKeys();
	std::vector<AccessKey> getAccessKeysOfUser(int64_t user_id);
	void removeAccessKey(int64_t id);
	std::vector<Bucket> getBuckets();
	void addBucket(int64_t id, const std::string& name);
	int64_t getMaxBucketId();
	void deleteBucket(int64_t id);
	std::vector<PolicyStatement> getPolicyStatements(int64_t policy_id);
	std::vector<int> getPolicyStatementActions(int64_t statement_id);
	std::vector<std::string> getPolicyStatementResources(int64_t statement_id);
	std::vector<Policy> getPolicies();
	void changePolicy(const std::string& data, int64_t id);
	void deletePolicyStatements(int64_t policy_id);
	std::vector<PolicyOfRole> getPoliciesOfRole(int64_t role_id);
	std::optional<Policy> getPolicy(int64_t id);
	std::vector<Role> getRoles();
	std::vector<Role> getRolesByUserId(int64_t user_id);
	std::optional<Role> getRole(int64_t id);
	std::vector<RolePolicy> getRolePolicies(int64_t role_id);
	std::optional<RolePolicy> getRolePolicyById(int64_t id);
	std::vector<UserRole> getUserRoles(int64_t user_id);
	int64_t addPolicyStatement(int64_t policy_id, const std::string& sid, int effect);
	int64_t addPolicyStatementAction(int64_t statement_id, int action);
	int64_t addPolicyStatementResource(int64_t statement_id, const std::string& resource);
	void changeUserPassword(const std::string& password, int64_t user_id);
	//@-SQLGenFunctionsEnd
};