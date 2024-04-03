#ifndef GROUPUSER_H
#define GROUPUSER_H

#include "user.hpp"

// 群组用户，多了role表示角色信息，继承User类，复用User的其他信息
class GroupUser : public User
{
public:
    GroupUser() = default;
    void setRole(const string &role) { _role = role; }
    string getRole() { return _role; }

private:
    string _role; // 群组中用户的角色信息
};

#endif // GROUPUSER_H