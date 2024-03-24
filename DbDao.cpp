#include "DbDao.h"
#include <assert.h>

/**
* @-SQLGenAccess
* @func vector<User> DbDao::getUsers
* @return int64 id, string name, string password
* @sql
*      SELECT id, name, password FROM users
*/
std::vector<DbDao::User> DbDao::getUsers()
{
	if(!q_getUsers.prepared())
	{
		q_getUsers=db.prepare("SELECT id, name, password FROM users");
	}
	auto& cursor=q_getUsers.cursor();
	std::vector<DbDao::User> ret;
	while(cursor.next())
	{
		ret.emplace_back();
		DbDao::User& obj=ret.back();
		cursor.get(0, obj.id);
		cursor.get(1, obj.name);
		cursor.get(2, obj.password);
	}
	return ret;
}

/**
* @-SQLGenAccess
* @func User DbDao::getUserById
* @return int64 id, string name, string password
* @sql
*      SELECT id, name, password FROM users WHERE id=:id(int64)
*/
DbDao::User DbDao::getUserById(int64_t id)
{
	if(!q_getUserById.prepared())
	{
		q_getUserById=db.prepare("SELECT id, name, password FROM users WHERE id=?");
	}
	q_getUserById.bind(id);
	auto& cursor=q_getUserById.cursor();
	User ret = { false, 0, "", "" };
	if(cursor.next())
	{
		ret.exists=true;
		cursor.get(0, ret.id);
		cursor.get(1, ret.name);
		cursor.get(2, ret.password);
	}
	q_getUserById.reset();
	return ret;
}

/**
* @-SQLGenAccess
* @func User DbDao::getUserByName
* @return int64 id, string name, string password
* @sql
*      SELECT id, name, password FROM users WHERE name=:name(string)
*/
DbDao::User DbDao::getUserByName(const std::string& name)
{
	if(!q_getUserByName.prepared())
	{
		q_getUserByName=db.prepare("SELECT id, name, password FROM users WHERE name=?");
	}
	q_getUserByName.bind(name);
	auto& cursor=q_getUserByName.cursor();
	User ret = { false, 0, "", "" };
	if(cursor.next())
	{
		ret.exists=true;
		cursor.get(0, ret.id);
		cursor.get(1, ret.name);
		cursor.get(2, ret.password);
	}
	q_getUserByName.reset();
	return ret;
}

/**
* @-SQLGenAccess
* @func int64_t DbDao::addUser
* @return int64_raw id
* @sql
*      INSERT INTO users (name, password) VALUES (:name(string), :password(string)) RETURNING id
*/
int64_t DbDao::addUser(const std::string& name, const std::string& password)
{
	if(!q_addUser.prepared())
	{
		q_addUser=db.prepare("INSERT INTO users (name, password) VALUES (?, ?) RETURNING id");
	}
	q_addUser.bind(name);
	q_addUser.bind(password);
	auto& cursor=q_addUser.cursor();
	assert(cursor.next());
	int64_t ret;
	cursor.get(0, ret);
	q_addUser.reset();
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
	if(!q_deleteUser.prepared())
	{
		q_deleteUser=db.prepare("DELETE FROM users WHERE id=?");
	}
	q_deleteUser.bind(id);
	q_deleteUser.write();
	q_deleteUser.reset();
}

//eof