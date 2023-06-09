﻿#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <sys/epoll.h>

#include "../thread_pool/thread_pool.h"
#include "../http_conn/http_conn.h"
#include "../timer/lst_timer.h"

const int MAX_FD = 65536;           //最大文件描述符
const int MAX_EVENT_NUMBER = 10000; //最大事件数
const int TIMESLOT = 5;             //最小超时单位

class WebServer
{
public:
    WebServer();
    ~WebServer();

    void init();

    void threads_pool();
    void Sql_pool();
    void log_write();
    void eventListen();
    void eventLoop();
    void timer(int connfd, struct sockaddr_in client_address);
    void adjust_timer(util_timer* timer);
    //删除定时器
    void deal_timer(util_timer* timer, int sockfd);
    bool dealclinetdata();
    bool dealwithsignal(bool& timeout, bool& stop_server);
    void dealwithread(int sockfd);
    void dealwithwrite(int sockfd);

public:
    //基础
    int m_port;
    char* m_root;

    int m_pipefd[2];
    int m_epollfd;
    http_conn* users;

    //数据库相关
    sql_pool* m_connPool;

    //线程池相关
    thread_pool<http_conn>* m_pool;
    int m_thread_num;

    //epoll_event相关
    epoll_event events[MAX_EVENT_NUMBER];

    int m_listenfd;
    int m_OPT_LINGER;
    

    //定时器相关
    client_data* users_timer;
    Utils utils;
};
#endif
