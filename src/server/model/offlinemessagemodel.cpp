#include "offlinemessagemodel.hpp"
#include "db.h"

// 存储用户的离线消息
void OfflineMsgModel::insert(int userId, string msg)
{
    // 组织sql语句
    char sql[1024] = {0};
    snprintf(sql, sizeof(sql), "insert into offlinemessage values(%d, '%s')", userId, msg.c_str());

    MySQL mysql;
    if (mysql.connect())
    {
        mysql.update(sql);
    }
}

// 删除用户的离线消息
void OfflineMsgModel::remove(int userId)
{
    // 组织sql语句
    char sql[1024] = {0};
    snprintf(sql, sizeof(sql), "delete from offlinemessage where userid = %d", userId);

    MySQL mysql;
    if (mysql.connect())
    {
        mysql.update(sql);
    }
}

// 查询用户的离线消息
vector<string> OfflineMsgModel::query(int userId)
{
    // 组织sql语句
    char sql[1024] = {0};
    snprintf(sql, sizeof(sql), "select message from offlinemessage where userid = %d", userId);

    vector<string> vec;
    MySQL mysql;
    if (mysql.connect())
    {
        MYSQL_RES *res = mysql.query(sql);
        if (res != nullptr)
        {
            // 把userid用户的所有离线消息放入vec中返回
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res)) != nullptr)
            {
                vec.push_back(row[0]);
            }
            mysql_free_result(res);
            return vec;
        }
    }
    return vec;
}
