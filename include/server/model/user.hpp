#ifndef USER_H
#define USER_H

#include <string>
using namespace std;

// 匹配User表的ORM类
// 类和数据库中的表一一对应
class User
{

public:
    User(int id = -1, string name = "", string pwd = "", string state = "offline") : _id(id), _name(name), _password(pwd), _state(state)
    {
    }

    void setId(int id) { _id = id; }
    void setName(const string &name) { _name = name; }
    void setPassword(const string &pwd) { _password = pwd; }
    void setState(const string &state) { _state = state; }

    int getId() { return _id; }
    string getName() { return _name; }
    string getPassword() { return _password; }
    string getState() { return _state; }

protected:
    int _id;
    string _name;
    string _password;
    string _state;
};

#endif // USER_H