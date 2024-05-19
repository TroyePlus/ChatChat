# ChatChat

A clustered chat server based on the muduo network library in Linux.

## 项目特点

- 基于 [muduo](https://github.com/chenshuo/muduo) 网络库开发高性能网络模块，提供高效的网络 I/O 服务
- 使用 [JSON for Modern C++](https://github.com/nlohmann/json) 实现通信数据的序列化和反序列化，方便数据传输和解析
- 使用 Nginx 的 TCP 负载均衡功能，提高服务器并发处理能力
- 使用 Redis 的发布-订阅功能实现消息队列，解决集群跨服务器通信问题
- 封装 MySQL 接口，实现用户数据持久化
- 模块化设计，易于扩展和维护
- 使用 CMake 构建集成编译环境

## 功能列表
- 网络模块
  - 基于 muduo 注册连接相关和读写事件相关的回调函数
  - 回调各类消息对应的事件处理器，解耦网络模块和业务模块
- 业务模块
  - 注册各类消息和对应的事件处理器
  - 使用 `unordered_map` 存储在线用户的通信连接 `_userConnMap`
  - 使用 `lock_guard` 保证 `_userConnMap` 的线程安全
  - 注册功能
  - 登录功能
    - 根据 `user` 表中 `state` 字段防止重复登录
    - 登录成功后记录连接信息，更新状态信息，查询并显示该用户的离线消息、好友信息和群组信息
  - 点对点聊天功能
    - 接收方在线，服务器推送消息
    - 接收方离线，存储离线消息
  - 群组聊天功能
    - 给出群组 ID 和消息内容
    - 查询群组中除发送方以外的所有用户
    - 根据是否在线推送消息或存储离线消息
  - 服务端异常退出处理
    - 使用 Linux 的信号处理函数捕捉 `CTRL + C` 信号，将所有用户置为离线状态
  - 客户端异常退出处理
    - 删除连接信息，更新用户的状态信息
- 数据模块
- 客户端
  - `main` 线程用于接收用户输入，负责发送数据
  - 子线程作为接收线程
- 负载均衡
  - 把 `client` 的请求按照负载算法分发到具体的业务服务器 `ChatServer` 上
  - 和 `ChatServer` 保持心跳机制，监测 `ChatServer` 故障
  - 平滑启动，在不中断服务的情况下，动态识别新添加的服务器信息
  - 重新加载配置文件 `/usr/local/nginx/sbin/nginx -s reload` ![image](https://github.com/TroyePlus/ChatChat/assets/45449485/061f5a22-4ea0-4357-a8f9-62239c68306e)
- 消息队列
  - 使用 [hiredis](https://github.com/redis/hiredis) 与 Redis 进行交互
  - 客户端根据 `userId` 向 Redis 订阅通道消息
  - 当不同服务器上注册的用户需要通信时，向接收方 `userId` 对应的通道发布消息
  - 独立线程中接收订阅通道的消息，消息发生后调用回调操作给业务层上报消息![image](https://github.com/TroyePlus/ChatChat/assets/45449485/fb706c92-d8d6-4fa8-befe-524eee098c7f)
    
## 开发环境

- [boost](https://www.boost.org/)
- [muduo](https://github.com/chenshuo/muduo)
- [hiredis](https://github.com/redis/hiredis)
- `Redis`  6.0.16
- `MySQL`  8.0.36
- `Nginx`  1.24.0
- `CMake`  3.22.1

## 项目构建

### 构建

```bash
./build.sh
```

### 运行

```bash
cd ./bin
./ChatServer ip port
./ChatClient ip port
```

## 问题记录

### Solved

1. Redis 验证失败
  - 原因：Redis 设置了密码验证
  - 解决方法：连接成功再调用 `redisCommand(_context, "AUTH %s", "pwd");` 进行密码验证，无论验证成功与否都要释放 `reply`
2. 新注册的用户错误显示先前登录用户的好友和群组信息
  - 原因：下一用户登录成功且返回有好友和群组信息时才清空上一用户的好友和群组信息。
  - 解决方法：用户登出时就清空好友和群组信息。

### Pending

1. JSON 库处理中文符号报错，导致服务端异常退出，用户状态未更新
 - 思路：对相关异常进行捕获并处理

## TODO

1. ~实现数据库连接池，提高数据库访问性能和效率~
2. 使用 [Protobuf](https://github.com/protocolbuffers/protobuf) 序列化格式，提高数据传输效率

