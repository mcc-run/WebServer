#include "webserver.h"

WebServer::WebServer()
{
    //http_conn类对象
    users = new http_conn[MAX_FD];

    //root文件夹路径
    char server_path[200];
    getcwd(server_path, 200);
    char root[6] = "/root";
    m_root = (char*)malloc(strlen(server_path) + strlen(root) + 1);
    strcpy(m_root, server_path);
    strcat(m_root, root);

    //定时器
    users_timer = new client_data[MAX_FD];
}

WebServer::~WebServer()
{
    close(m_epollfd);
    close(m_listenfd);
    close(m_pipefd[1]);
    close(m_pipefd[0]);
    delete[] users;
    delete[] users_timer;
    delete m_pool;
}

void WebServer::init()
{
    m_port = 8888;
}



void WebServer::log_write()
{
    log::getinstance()->init();
}

void WebServer::Sql_pool()
{
    //初始化数据库连接池
    m_connPool = sql_pool::getinstance();
    m_connPool->init();

    //初始化数据库读取表
    users->initmysql_result();
}

void WebServer::threads_pool()
{
    //线程池
    m_pool = new thread_pool<http_conn>();
}

void WebServer::eventListen()
{
    //网络编程基础步骤
    m_listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(m_listenfd >= 0);

    
    //当套接字关闭时，内核将延迟关闭连接，直到所有剩余的数据都发送完毕或超时（本例中超时时间为1秒）。
    struct linger tmp = { 1, 1 };
    setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    

    int ret = 0;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(m_port);

    int flag = 1;
    setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    ret = bind(m_listenfd, (struct sockaddr*)&address, sizeof(address));
    assert(ret >= 0);
    ret = listen(m_listenfd, 5);
    assert(ret >= 0);

    utils.init(TIMESLOT);

    //epoll创建内核事件表
    epoll_event events[MAX_EVENT_NUMBER];
    m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);

    utils.addfd(m_epollfd, m_listenfd, false);
    http_conn::m_epollfd = m_epollfd;

    //用于主线程与子线程之间传递消息，当定时器到时是，子线程会将信息写入fd通知主线程，让主线程调用定时清理非活动连接
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_pipefd);
    assert(ret != -1);
    utils.setnonblocking(m_pipefd[1]);
    utils.addfd(m_epollfd, m_pipefd[0], false);

    utils.addsig(SIGPIPE, SIG_IGN);
    utils.addsig(SIGALRM, utils.sig_handler, false);
    utils.addsig(SIGTERM, utils.sig_handler, false);

    alarm(TIMESLOT);

    //工具类,信号和描述符基础操作
    Utils::u_pipefd = m_pipefd;
    Utils::u_epollfd = m_epollfd;
}

void WebServer::timer(int connfd, struct sockaddr_in client_address)
{
    //根据connfd初始化对应位置的http
    users[connfd].init(connfd, client_address);

    //初始化client_data数据
    users_timer[connfd].address = client_address;
    users_timer[connfd].sockfd = connfd;
    //创建定时器，设置回调函数和超时时间，绑定用户数据，将定时器添加到链表中
    util_timer* timer = new util_timer;
    timer->user_data = &users_timer[connfd];
    timer->cb_func = cb_func;
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;
    //将定时器设置回客户端数据
    users_timer[connfd].timer = timer;
    //将当前定时器添加至定时器链表中
    utils.m_timer_lst.add_timer(timer);
}

//若有数据传输，则将定时器往后延迟3个单位
//并对新的定时器在链表上的位置进行调整
void WebServer::adjust_timer(util_timer* timer)
{
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;
    utils.m_timer_lst.adjust_timer(timer);
    string info = "adjust timer once";
    LOG_INFO(info);
}

void WebServer::deal_timer(util_timer* timer, int sockfd)
{
    timer->cb_func(&users_timer[sockfd]);
    if (timer)
    {
        utils.m_timer_lst.del_timer(timer);
    }
    string info = "关闭文件描述符：" + to_string(users_timer[sockfd].sockfd);
    LOG_INFO(info);
}

//处理新的客户端连接
bool WebServer::dealclinetdata()
{
    struct sockaddr_in client_address;
    socklen_t client_addrlength = sizeof(client_address);
    cout << "123456" << endl;
    while (1)
    {
        int connfd = accept(m_listenfd, (struct sockaddr*)&client_address, &client_addrlength);
        if (connfd < 0)
        {
            string err = "链接错误，错误编码是" + to_string(errno);
            LOG_ERROR(err);
            break;
        }
        if (http_conn::m_user_count >= MAX_FD)
        {
            utils.show_error(connfd, "Internal server busy");
            string err = "Internal server busy";
            LOG_ERROR(err);
            break;
        }
        timer(connfd, client_address);
    }
    return false;
    
    return true;
}

bool WebServer::dealwithsignal(bool& timeout, bool& stop_server)
{
    int ret = 0;
    int sig;
    char signals[1024];
    ret = recv(m_pipefd[0], signals, sizeof(signals), 0);
    if (ret == -1)
    {
        return false;
    }
    else if (ret == 0)
    {
        return false;
    }
    else
    {
        for (int i = 0; i < ret; ++i)
        {
            switch (signals[i])
            {
            case SIGALRM:
            {
                timeout = true;
                break;
            }
            case SIGTERM:
            {
                stop_server = true;
                break;
            }
            }
        }
    }
    return true;
}

void WebServer::dealwithread(int sockfd)
{
    util_timer* timer = users_timer[sockfd].timer;

    //proactor
    if (users[sockfd].read_once())
    {
        string info = "处理客户端" + string(inet_ntoa(users[sockfd].get_address()->sin_addr));
        LOG_INFO(info);

        //若监测到读事件，将该事件放入请求队列
        m_pool->append(users + sockfd);

        if (timer)
        {
            adjust_timer(timer);
        }
    }
    else
    {
        deal_timer(timer, sockfd);
    }
    
}

void WebServer::dealwithwrite(int sockfd)
{
    util_timer* timer = users_timer[sockfd].timer;
    
    //proactor
    if (users[sockfd].write())
    {
        string info = "发送数据给客户端" + string(inet_ntoa(users[sockfd].get_address()->sin_addr));
        LOG_INFO(info);

        if (timer)
        {
            adjust_timer(timer);
        }
    }
    else
    {
        deal_timer(timer, sockfd);
    }
    
}

void WebServer::eventLoop()
{
    //是否需要处理定时任务
    bool timeout = false;
    bool stop_server = false;

    while (!stop_server)
    {
        int number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
        if (number < 0 && errno != EINTR)
        {
            string err = "epoll failure";
            LOG_ERROR(err);
            break;
        }

        for (int i = 0; i < number; i++)
        {
            int sockfd = events[i].data.fd;

            //处理新到的客户连接
            if (sockfd == m_listenfd)
            {
                bool flag = dealclinetdata();
                if (false == flag)
                    continue;
            }
            //EPOLLHUP用于检测对端是否关闭连接，关闭连接
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                //服务器端关闭连接，移除对应的定时器
                util_timer* timer = users_timer[sockfd].timer;
                deal_timer(timer, sockfd);
            }
            //处理信号
            else if ((sockfd == m_pipefd[0]) && (events[i].events & EPOLLIN))
            {
                bool flag = dealwithsignal(timeout, stop_server);
                if (false == flag) {
                    string err = "dealclientdata failure";
                    LOG_ERROR(err);
                }
            }
            //处理客户连接上接收到的数据
            else if (events[i].events & EPOLLIN)
            {
                dealwithread(sockfd);
            }
            else if (events[i].events & EPOLLOUT)
            {
                dealwithwrite(sockfd);
            }
        }
        if (timeout)
        {
            utils.timer_handler();
            string info = "timer tick";
            LOG_INFO(info);

            timeout = false;
        }
    }
}