#include <string>
#include <filesystem>
#include "TcpServer.hpp"
#include "MyLog.hpp"

namespace fs = std::filesystem;

ThreadPool* tp = nullptr;

class EchoServer
{
public:
    EchoServer(EventLoop *loop, const InetAddress &addr, const std::string &name)
        : server_(loop, addr, name)
        , loop_(loop)
    {
        // 注册回调函数
        server_.setConnectionCallback(
            std::bind(&EchoServer::onConnection, this, std::placeholders::_1));
        
        server_.setMessageCallback(
            std::bind(&EchoServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

        // 设置合适的subloop线程数量
        server_.setThreadNum(3);
    }
    void start()
    {
        server_.start();
    }

private:
    // 连接建立或断开的回调函数
    void onConnection(const TcpConnectionPtr &conn)   
    {
        if (conn->connected())
        {
            mylog::GetLogger("asynclogger")->Info("Connection UP : %s", conn->peerAddress().toIpPort().c_str());
        }
        else
        {
            mylog::GetLogger("asynclogger")->Info("Connection DOWN : %s", conn->peerAddress().toIpPort().c_str());
        }
    }

    // 可读写事件回调
    void onMessage(const TcpConnectionPtr &conn, Buffer *buf, Timestamp time)
    {
        std::string msg = buf->retrieveAllAsString();
        conn->send(msg);
        // conn->shutdown();   // 关闭写端 底层响应EPOLLHUP => 执行closeCallback_
    }
    TcpServer server_;
    EventLoop *loop_;

};

int main() {
    tp = new ThreadPool(mylog::Util::JsonData::GetJsonData().thread_count);

    const std::string log_dir = "../logfile";
    if (fs::exists(log_dir)) {
        fs::remove_all(log_dir);
    }
    fs::create_directories(log_dir);

    std::shared_ptr<mylog::LoggerBuilder> Glb(new mylog::LoggerBuilder());
    Glb->BuildLoggerName("asynclogger");
    const std::string file_flush_path = log_dir + "/FileFlush.log";
    mylog::LoggerManager::GetInstance().AddLogger(Glb->Build());

    EventLoop loop;
    InetAddress addr(8080);
    EchoServer server(&loop, addr, "EchoServer");
    server.start();
    loop.loop();
    delete(tp);
    return 0;
}