#include "chatservice.hpp"
#include "public.hpp"
#include <muduo/base/Logging.h>
#include <vector>

using namespace muduo;
using namespace std;

// 获取单例对象的接口函数
ChatService *ChatService::instance()
{
    static ChatService service;
    return &service;
}

ChatService::ChatService()
{
    // 注册各类消息和对应的消息处理方法
    _msgHandlerMap.insert({LOGIN_MSG, std::bind(&ChatService::loginHandler, this, _1, _2, _3)});

    _msgHandlerMap.insert({LOGINOUT_MSG, std::bind(&ChatService::loginout, this, _1, _2, _3)});
    _msgHandlerMap.insert({REGISTER_MSG, std::bind(&ChatService::registerHandler, this, _1, _2, _3)});
    _msgHandlerMap.insert({ONE_CHAT_MSG, std::bind(&ChatService::oneChatHandler, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_FRIEND_MSG, std::bind(&ChatService::addFriendHandler, this, _1, _2, _3)});
    // 群组业务管理相关事件处理回调注册
    _msgHandlerMap.insert({CREATE_GROUP_MSG, std::bind(&ChatService::createGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_GROUP_MSG, std::bind(&ChatService::addGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({GROUP_CHAT_MSG, std::bind(&ChatService::groupChat, this, _1, _2, _3)});

    // 连接Redis服务器
    if (_redis.connect())
    {
        // 设置上报通道消息的回调方法
        // Redis发现通道上有消息发生时，会给相应的服务器进行上报
        _redis.init_notify_handler(std::bind(&ChatService::redis_subscribe_message_handler, this, _1, _2));
    }
}

// 服务端异常终止，业务重置方法
void ChatService::reset()
{
    // 将所有online状态的用户，设置成offline
    _userModel.resetState();
}

// 获取消息对应的处理器
MsgHandler ChatService::getHandler(int msgId)
{
    // 记录错误日志，msgid没有对应的事件处理回调
    auto it = _msgHandlerMap.find(msgId);
    if (it == _msgHandlerMap.end())
    {
        // 返回一个默认的处理器（lambda匿名函数，仅仅用作提示）
        return [=](const TcpConnectionPtr &, json &, Timestamp)
        {
            LOG_ERROR << "msgid: " << msgId << " can not find handler!";
        };
    }
    return _msgHandlerMap[msgId];
}

// 登录业务
void ChatService::loginHandler(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int id = js["id"].get<int>();
    string password = js["password"];

    User user = _userModel.query(id);
    if (user.getId() == id && user.getPassword() == password)
    {
        if (user.getState() == "online")
        {
            // 该用户已经登录，不允许重复登录
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 2;
            response["errmsg"] = "this account is using, input another!";
            conn->send(response.dump());
        }
        else
        {
            // 登录成功，记录用户连接信息
            // 需要考虑线程安全问题
            // 为了保证并发性，尽量缩小锁的粒度
            // 这里增加连接操作是临界区代码段，出作用域自动释放锁
            {
                lock_guard<mutex> lock(_connMutex);
                _userConnMap.insert({id, conn});
            }

            // id用户登录成功后，向Redis订阅channel(id)
            _redis.subscribe(id);

            // 登录成功，更新用户状态信息 state offline => online
            user.setState("online");
            _userModel.updateState(user); // 这句话就将数据更新反映到数据库中了，体现了业务模块和数据模块分开的思想

            // 合成返回json，都是局部变量，线程之间栈是隔离的，不会产生冲突
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 0;
            response["id"] = user.getId();
            response["name"] = user.getName();

            // 查询该用户是否有离线消息
            vector<string> vec = _offlineMsgModel.query(id);
            if (!vec.empty())
            {
                // json可以直接跟vector容器之间进行序列化和反序列化
                response["offlinemsg"] = vec;
                // 读取该用户的离线消息后，把该用户的所有离线消息删除掉
                _offlineMsgModel.remove(id);
            }
            else
            {
                LOG_INFO << "无离线消息";
            }

            // 查询该用户的好友信息并返回（自定义类型）
            vector<User> userVec = _friendModel.query(id);
            if (!userVec.empty())
            {
                vector<string> vec2;
                for (auto &user : userVec)
                {
                    json js;
                    js["id"] = user.getId();
                    js["name"] = user.getName();
                    js["state"] = user.getState();
                    vec2.push_back(js.dump());
                }
                response["friends"] = vec2;
            }

            // 查询用户的群组信息
            vector<Group> groupuserVec = _groupModel.queryGroups(id);
            if (!groupuserVec.empty())
            {
                // group:[{groupid:[xxx, xxx, xxx, xxx]}]
                vector<string> groupV;
                for (Group &group : groupuserVec)
                {
                    json grpjson;
                    grpjson["id"] = group.getId();
                    grpjson["groupname"] = group.getName();
                    grpjson["groupdesc"] = group.getDesc();
                    vector<string> userV;
                    for (GroupUser &user : group.getUsers())
                    {
                        json js;
                        js["id"] = user.getId();
                        js["name"] = user.getName();
                        js["state"] = user.getState();
                        js["role"] = user.getRole();
                        userV.push_back(js.dump());
                    }
                    grpjson["users"] = userV;
                    groupV.push_back(grpjson.dump());
                }
                response["groups"] = groupV;
            }

            conn->send(response.dump());
        }
    }
    else
    {
        // 登录失败，该用户不存在或者存在但是密码错误
        json response;
        response["msgid"] = LOGIN_MSG_ACK;
        response["errno"] = 1;
        response["errmsg"] = "incorrect id or password!";
        conn->send(response.dump());
    }
}

// 注册业务
void ChatService::registerHandler(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    string name = js["name"];
    string password = js["password"];

    User user;
    user.setName(name);
    user.setPassword(password);
    bool state = _userModel.insert(user);

    if (state)
    {
        // 注册成功
        json response;
        response["msgid"] = REGISTER_MSG_ACK;
        response["errno"] = 0;
        response["id"] = user.getId();
        conn->send(response.dump());
    }
    else
    {
        // 注册失败
        json response;
        response["msgid"] = REGISTER_MSG_ACK;
        response["errno"] = 1;
        // 注册已经失败，不需要在json返回id
        conn->send(response.dump());
    }
}

// 处理注销业务
void ChatService::loginout(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(userid);
        if (it != _userConnMap.end())
        {
            _userConnMap.erase(it);
        }
    }

    // 用户注销（下线），在Redis中取消订阅通道
    _redis.unsubscribe(userid);

    // 更新用户的状态信息
    // User user(userid, "", "", "offline");
    User user(userid);
    user.setState("offline");
    _userModel.updateState(user);
}

// 处理客户端异常退出
void ChatService::clientCloseExceptionHandler(const TcpConnectionPtr &conn)
{
    User user;
    {
        lock_guard<mutex> lock(_connMutex);
        for (auto it = _userConnMap.begin(); it != _userConnMap.end(); ++it)
        {
            if (it->second == conn)
            {
                // 从map表删除用户的连接信息
                user.setId(it->first);
                _userConnMap.erase(it);
                break;
            }
        }
    }

    // 在Redis中取消订阅通道
    _redis.unsubscribe(user.getId());

    // 更新用户的状态信息
    // 如果用户不合法（未登录/找到），不必再向数据库进行请求
    if (user.getId() != -1)
    {
        user.setState("offline");
        _userModel.updateState(user);
    }
}

// 一对一聊天业务
void ChatService::oneChatHandler(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    // 需要接收信息的用户ID
    int toId = js["toid"].get<int>();

    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(toId);
        // 确认toId是在线状态，转发消息（服务器主动推送消息给toId用户）
        if (it != _userConnMap.end())
        {
            // 直接发送消息
            it->second->send(js.dump());
            // return和‘}’，lock离开作用域，调用析构函数，释放锁
            return;
        }
    }

    // 查询toId是否在线
    User user = _userModel.query(toId);
    if (user.getState() == "online")
    {
        // 用户不在本机，但状态在线，说明在其他主机上
        // 向对端用户id命名的通道发布消息
        _redis.publish(toId, js.dump());
        return;
    }

    // toId不在线，存储离线消息
    _offlineMsgModel.insert(toId, js.dump());
}

// 添加好友业务
void ChatService::addFriendHandler(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userId = js["id"].get<int>();
    int friendId = js["friendid"].get<int>();

    // 这里可以根据实际需求验证friendId是否存在
    // 存储好友信息
    _friendModel.insert(userId, friendId);
}

// 创建群组业务
void ChatService::createGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userId = js["id"].get<int>();
    string name = js["groupname"];
    string desc = js["groupdesc"];

    // 存储新创建的群组消息
    Group group(-1, name, desc);
    if (_groupModel.createGroup(group))
    {
        // 存储群组创建人信息
        // role：更好的做法是在model层定义一些常量，然后在业务层使用
        _groupModel.addGroup(userId, group.getId(), "creator");
    }
}

// 加入群组业务
void ChatService::addGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userId = js["id"].get<int>();
    int groupId = js["groupid"].get<int>();
    _groupModel.addGroup(userId, groupId, "normal");
}

// 群组聊天业务
void ChatService::groupChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userId = js["id"].get<int>();
    int groupId = js["groupid"].get<int>();
    vector<int> userIdVec = _groupModel.queryGroupUsers(userId, groupId);

    lock_guard<mutex> lock(_connMutex);
    for (int id : userIdVec)
    {
        auto it = _userConnMap.find(id);
        if (it != _userConnMap.end())
        {
            // 群友在线，转发群消息
            it->second->send(js.dump());
        }
        else
        {
            // 查询toId是否在线
            User user = _userModel.query(id);
            if (user.getState() == "online")
            {
                _redis.publish(id, js.dump());
            }
            else
            {
                // 群友不在线，转储离线消息
                _offlineMsgModel.insert(id, js.dump());
            }
        }
    }
}

// 从redis消息队列中获取订阅的消息，这里channel其实就是id
void ChatService::redis_subscribe_message_handler(int channel, string message)
{
    // 用户在线
    lock_guard<mutex> lock(_connMutex);
    auto it = _userConnMap.find(channel);
    if (it != _userConnMap.end())
    {
        // 直接转发消息
        it->second->send(message);
        return;
    }

    // 向通道发布消息、从通道取消息的过程中，接收方用户下线
    // 用户不在线，转储离线消息
    _offlineMsgModel.insert(channel, message);
}
