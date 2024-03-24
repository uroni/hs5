#pragma once

#include <sqlite-cpp-sqlgen/DatabaseQuery.h>
#include <vector>
#include <string>

class DbDao
{
    sqlgen::Database& db;

    //@-SQLGenVariablesBegin
	sqlgen::DatabaseQuery q_getUsers;
	sqlgen::DatabaseQuery q_getUserById;
	sqlgen::DatabaseQuery q_getUserByName;
	sqlgen::DatabaseQuery q_addUser;
	sqlgen::DatabaseQuery q_deleteUser;
	//@-SQLGenVariablesEnd

public:

    //@-SQLGenFunctionsBegin
	struct User
	{
		bool exists;
		int64_t id;
		std::string name;
		std::string password;
	};


	std::vector<User> getUsers();
	User getUserById(int64_t id);
	User getUserByName(const std::string& name);
	int64_t addUser(const std::string& name, const std::string& password);
	void deleteUser(int64_t id);
	//@-SQLGenFunctionsEnd
};