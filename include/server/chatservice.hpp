#ifndef CHATSERVICE_H
#define CHATSERVICE_H

#include "json.hpp"
#include "usermodel.hpp"
#include "offlinemessagemodel.hpp"
#include "friendmodel.hpp"
#include "groupmodel.hpp"
#include "redis.hpp"
#include <muduo/net/TcpConnection.h>
#include <unordered_map>
#include <functional>
#include <mutex>

using namespace muduo;
using namespace muduo::net;
// 表示处理消息的事件回调方法类型
using json = nlohmann::json;
using MsgHandler = std::function<void(const TcpConnectionPtr &, json &, Timestamp)>;

// 聊天服务器业务类
class ChatService
{
public:
    // ChatService 单例模式
    // 获取单例对象的接口函数
    static ChatService *instance();

    // 登录业务
    void loginHandler(const TcpConnectionPtr &conn, json &js, Timestamp time);
    // 处理注销业务
    void loginout(const TcpConnectionPtr &conn, json &js, Timestamp time);
    // 注册业务
    void registerHandler(const TcpConnectionPtr &conn, json &js, Timestamp time);

    // 一对一聊天业务
    void oneChatHandler(const TcpConnectionPtr &conn, json &js, Timestamp time);
    // 添加好友业务
    void addFriendHandler(const TcpConnectionPtr &conn, json &js, Timestamp time);

    // 创建群组业务
    void createGroup(const TcpConnectionPtr &conn, json &js, Timestamp time);
    // 加入群组业务
    void addGroup(const TcpConnectionPtr &conn, json &js, Timestamp time);
    // 群组聊天业务
    void groupChat(const TcpConnectionPtr &conn, json &js, Timestamp time);

    // 处理客户端异常退出
    void clientCloseExceptionHandler(const TcpConnectionPtr &conn);
    // 服务端异常终止，业务重置方法
    void reset();

    // 获取消息对应的处理器
    MsgHandler getHandler(int msgId);

    // 从redis消息队列中获取订阅的消息
    void redis_subscribe_message_handler(int channel, string message);

private:
    ChatService();

    // 存储消息id和其对应的业务处理方法
    std::unordered_map<int, MsgHandler> _msgHandlerMap;

    // 存储在线用户的通信连接，会随着用户上线/下线不断该笔，其访问需要注意线程安全
    std::unordered_map<int, TcpConnectionPtr> _userConnMap;

    // 定义互斥锁，保证_userConnMap的线程安全
    std::mutex _connMutex;

    // 数据操作类对象
    UserModel _userModel;
    OfflineMsgModel _offlineMsgModel;
    FriendModel _friendModel;
    GroupModel _groupModel;

    // Redis操作对象
    Redis _redis;
};

#endif // CHATSERVICE_H