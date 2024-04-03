#include "friendmodel.hpp"
#include "db.h"

void FriendModel::insert(int userId, int friendId)
{
    // 组织sql语句
    char sql[1024] = {0};
    snprintf(sql, sizeof(sql), "insert into friend values(%d, %d)", userId, friendId);

    MySQL mysql;
    if (mysql.connect())
    {
        mysql.update(sql);
    }
}

vector<User> FriendModel::query(int userId)
{
    // 1.组装sql语句
    // User表和Friend联合查询
    char sql[1024] = {0};
    snprintf(sql, sizeof(sql), "select a.id, a.name, a.state from user a inner join friend b on b.friendid = a.id where b.userid = %d", userId);

    vector<User> vec;
    MySQL mysql;
    if (mysql.connect())
    {
        MYSQL_RES *res = mysql.query(sql);
        if (res != nullptr)
        {
            // 把userid用户的所有好友信息放入vec中返回
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res)) != nullptr)
            {
                User user;
                user.setId(atoi(row[0]));
                user.setName(row[1]);
                user.setState(row[2]);
                vec.push_back(user);
            }
            mysql_free_result(res);
            return vec;
        }
    }
    return vec;
}