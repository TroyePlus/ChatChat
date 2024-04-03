#include "chatserver.hpp"
#include "json.hpp"
#include "chatservice.hpp"
#include <functional>
#include <string>

using namespace std;
using namespace placeholders;
using json = nlohmann::json;

// 初始化聊天服务器对象
ChatServer::ChatServer(EventLoop *loop,
                       const InetAddress &listenAddr,
                       const string &nameArg)
    : _server(loop, listenAddr, nameArg), _loop(loop)
{
    // 注册连接事件的回调函数
    _server.setConnectionCallback(std::bind(&ChatServer::onConnection, this, _1));

    // 注册消息事件的回调函数
    _server.setMessageCallback(std::bind(&ChatServer::onMessage, this, _1, _2, _3));

    // 设置subLoop线程数量
    _server.setThreadNum(4);
}

// 启动服务
void ChatServer::start()
{
    _server.start();
}

// 连接相关信息的回调函数
void ChatServer::onConnection(const TcpConnectionPtr &conn)
{
    // 客户端断开连接
    if (!conn->connected())
    {
        ChatService::instance()->clientCloseExceptionHandler(conn);
        conn->shutdown();
    }
}

// 读写事件相关信息的回调函数
void ChatServer::onMessage(const TcpConnectionPtr &conn,
                           Buffer *buffer,
                           Timestamp time)
{
    string buf = buffer->retrieveAllAsString();
    // 数据的反序列化
    json js = json::parse(buf);

    // 目的：完全解耦网络模块和业务模块的代码，避免在网络模块中直接调用业务模块的相关方法
    // 通过js["msgid"]来获取不同的业务处理器（事先绑定的回调方法）
    // js["msgid"].get<int>() 将js["msgid"]对应的值强制转换成int
    auto msgHandler = ChatService::instance()->getHandler(js["msgid"].get<int>());
    // 回调消息绑定好的事件处理器，来执行相应的业务处理
    msgHandler(conn, js, time);
}
