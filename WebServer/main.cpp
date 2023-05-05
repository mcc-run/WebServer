#include <iostream>
#include "log/log.h"
#include "http_conn/http_conn.h"

using namespace std;

int main()
{

	sql_pool::getinstance()->init();

	log::getinstance()->init();

	http_conn user;
	user.initmysql_result();

    return 0;
}

/*
	1、创建监听事件
	2、这意味着当套接字关闭时，内核将延迟关闭连接，直到所有剩余的数据都发送完毕或超时（本例中超时时间为1秒）。
	3、设置地址
	4、创建epoll
	5、将监听事件添加到epoll中
	6、创建主线程和子线程之间通信的socketpair，当时钟信号到达是通知主线程清理不活跃的连接
	7、注册时钟信号捕捉函数
	8、启动时钟

*/

/*
	初始化 是否关闭服务器、定时清理不活跃连接两个变量

	1、处理新的链接
	2、检测客户端是否关闭链接	若关闭从epoll中摘除，关闭文件描述符、从定时器中摘除
	3、接收到时钟信号，清楚不活跃的连接
	4、从客户端接收数据
	5、发送数据给客户端

*/