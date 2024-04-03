#include "redis.hpp"
#include <iostream>

Redis::Redis() : _publish_context(nullptr), _subscribe_context(nullptr)
{
}

Redis::~Redis()
{
    if (_publish_context != nullptr)
    {
        redisFree(_publish_context);
    }

    if (_subscribe_context != nullptr)
    {
        redisFree(_subscribe_context);
    }
}

// 连接Redis服务器
bool Redis::connect()
{
    // 负责publish发布消息的上下文连接
    _publish_context = redisConnect("127.0.0.1", 6379);
    if (_publish_context == nullptr)
    {
        cerr << "connect redis failed!" << endl;
        return false;
    }

    // 负责subscribe订阅消息的上下文连接
    _subscribe_context = redisConnect("127.0.0.1", 6379);
    if (_subscribe_context == nullptr)
    {
        cerr << "connect redis failed!" << endl;
        return false;
    }

    // 密码验证
    redisReply *reply;

    reply = (redisReply *)redisCommand(_publish_context, "AUTH %s", "123456");
    if (reply->type == REDIS_REPLY_ERROR)
    {
        cerr << "authentication failed!" << endl;
    }
    else
    {
        cerr << "authentication succeeded!" << endl;
    }
    freeReplyObject(reply);

    reply = (redisReply *)redisCommand(_subscribe_context, "AUTH %s", "123456");
    if (reply->type == REDIS_REPLY_ERROR)
    {
        cerr << "authentication failed!" << endl;
    }
    else
    {
        cerr << "authentication succeeded!" << endl;
    }
    freeReplyObject(reply);

    // 独立线程中接收订阅通道的消息
    // 监听通道上的事件，有消息给业务层进行上报
    // 为什么单独的线程监听订阅通道？
    // 1. subscribe是阻塞等待响应，Chatserver向消息队列订阅后还需要继续提供服务，线程不能因为订阅被阻塞（不能订阅时阻塞的等待这个通道上发生消息）
    // 2. 1个ChatServer上只有4个线程(1 IO + 3 worker)，如果每次订阅1个通道这个线程就会阻塞住，3个worker线程最多订阅3个通道，无法接受
    // 3. 新的线程专门以阻塞的方式等待相应的通道是否有事件发生，发生则向业务层上报
    thread t([&]()
             { observer_channel_message(); });
    t.detach();

    cout << "connect redis-server success!" << endl;
    return true;
}

// 向Redis指定的通道channel发布消息
bool Redis::publish(int channel, string message)
{
    // PUBLISH命令一执行立刻响应，不会阻塞当前线程
    // 相当于publish 键 值
    // redis 127.0.0.1:6379> PUBLISH runoobChat "Redis PUBLISH test"
    redisReply *reply = (redisReply *)redisCommand(_publish_context, "PUBLISH %d %s", channel, message.c_str());
    if (reply == nullptr)
    {
        cerr << "publish command failed!" << endl;
        return false;
    }

    // 返回值是动态生成的结构体，用完需要free释放资源
    freeReplyObject(reply);
    return true;
}

// 向Redis指定的通道subscribe订阅消息
bool Redis::subscribe(int channel)
{
    // redisCommand：redisAppendCommand+redisBufferWrite+redisGetReply
    // 1. 把SUBSCRIBE命令组装好后写到本地缓存
    // 2. 从本地缓存把命令发送到Redis Server上
    // 3. redisGetReply：以阻塞的方式等待远端的响应
    // 这里只调用前2个函数，告诉Redis Server要订阅这个通道，不会阻塞等待Redis Server的响应

    // SUBSCRIBE命令本身会造成线程阻塞等待通道里面发生消息，这里只做订阅通道，不接收通道消息
    // 通道消息的接收专门在observer_channel_message函数中的独立线程中进行
    // 只负责发送命令，不阻塞接收Redis server响应消息，否则和notifyMsg线程抢占响应资源
    // redis 127.0.0.1:6379> SUBSCRIBE runoobChat
    if (REDIS_ERR == redisAppendCommand(_subscribe_context, "SUBSCRIBE %d", channel))
    {
        cerr << "subscribe command failed!" << endl;
        return false;
    }
    // redisBufferWrite可以循环发送缓冲区，直到缓冲区数据发送完毕（done被置为1）
    int done = 0;
    while (!done)
    {
        if (REDIS_ERR == redisBufferWrite(_subscribe_context, &done))
        {
            cerr << "subscribe command failed!" << endl;
            return false;
        }
    }
    // redisGetReply

    return true;
}

// 向redis指定的通道unsubscribe取消订阅消息
bool Redis::unsubscribe(int channel)
{
    // redisCommand 会先把命令缓存到context中，然后调用RedisAppendCommand发送给redis
    // redis执行subscribe是阻塞，不会响应，不会给我们一个reply
    if (REDIS_ERR == redisAppendCommand(_subscribe_context, "UNSUBSCRIBE %d", channel))
    {
        cerr << "unsubscribe command failed!" << endl;
        return false;
    }
    // redisBufferWrite可以循环发送缓冲区，直到缓冲区数据发送完毕（done被置为1）
    int done = 0;
    while (!done)
    {
        if (REDIS_ERR == redisBufferWrite(_subscribe_context, &done))
        {
            cerr << "unsubscribe command failed!" << endl;
            return false;
        }
    }

    return true;
}

// 独立线程中接收订阅通道的消息
void Redis::observer_channel_message()
{
    // 以循环阻塞的方式等待_subscribe_context上下文上是否有消息发生
    redisReply *reply = nullptr;
    while (REDIS_OK == redisGetReply(_subscribe_context, (void **)&reply))
    {
        // 订阅收到的消息是一个带三元素的数组，0.message, 1.通道号，2.消息
        if (reply != nullptr && reply->element[2] != nullptr && reply->element[2]->str != nullptr)
        {
            // 调用回调操作，给业务层上报通道上发生的消息(通道号，通道上的数据)
            _notify_message_handler(atoi(reply->element[1]->str), reply->element[2]->str);
        }

        freeReplyObject(reply);
    }

    cerr << "----------------------- oberver_channel_message quit --------------------------" << endl;
}

// 初始化向业务层上报通道消息的回调对象
void Redis::init_notify_handler(redis_handler handler)
{
    _notify_message_handler = handler;
}