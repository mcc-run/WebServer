#include "webserver.h"

WebServer::WebServer()
{
    //http_conn�����
    users = new http_conn[MAX_FD];

    //root�ļ���·��
    char server_path[200];
    getcwd(server_path, 200);
    char root[6] = "/root";
    m_root = (char*)malloc(strlen(server_path) + strlen(root) + 1);
    strcpy(m_root, server_path);
    strcat(m_root, root);

    //��ʱ��
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
    //��ʼ�����ݿ����ӳ�
    m_connPool = sql_pool::getinstance();
    m_connPool->init();

    //��ʼ�����ݿ��ȡ��
    users->initmysql_result();
}

void WebServer::threads_pool()
{
    //�̳߳�
    m_pool = new thread_pool<http_conn>();
}

void WebServer::eventListen()
{
    //�����̻�������
    m_listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(m_listenfd >= 0);

    
    //���׽��ֹر�ʱ���ں˽��ӳٹر����ӣ�ֱ������ʣ������ݶ�������ϻ�ʱ�������г�ʱʱ��Ϊ1�룩��
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

    //epoll�����ں��¼���
    epoll_event events[MAX_EVENT_NUMBER];
    m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);

    utils.addfd(m_epollfd, m_listenfd, false);
    http_conn::m_epollfd = m_epollfd;

    //�������߳������߳�֮�䴫����Ϣ������ʱ����ʱ�ǣ����̻߳Ὣ��Ϣд��fd֪ͨ���̣߳������̵߳��ö�ʱ����ǻ����
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_pipefd);
    assert(ret != -1);
    utils.setnonblocking(m_pipefd[1]);
    utils.addfd(m_epollfd, m_pipefd[0], false);

    utils.addsig(SIGPIPE, SIG_IGN);
    utils.addsig(SIGALRM, utils.sig_handler, false);
    utils.addsig(SIGTERM, utils.sig_handler, false);

    alarm(TIMESLOT);

    //������,�źź���������������
    Utils::u_pipefd = m_pipefd;
    Utils::u_epollfd = m_epollfd;
}

void WebServer::timer(int connfd, struct sockaddr_in client_address)
{
    //����connfd��ʼ����Ӧλ�õ�http
    users[connfd].init(connfd, client_address);

    //��ʼ��client_data����
    users_timer[connfd].address = client_address;
    users_timer[connfd].sockfd = connfd;
    //������ʱ�������ûص������ͳ�ʱʱ�䣬���û����ݣ�����ʱ����ӵ�������
    util_timer* timer = new util_timer;
    timer->user_data = &users_timer[connfd];
    timer->cb_func = cb_func;
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;
    //����ʱ�����ûؿͻ�������
    users_timer[connfd].timer = timer;
    //����ǰ��ʱ���������ʱ��������
    utils.m_timer_lst.add_timer(timer);
}

//�������ݴ��䣬�򽫶�ʱ�������ӳ�3����λ
//�����µĶ�ʱ���������ϵ�λ�ý��е���
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
    string info = "�ر��ļ���������" + to_string(users_timer[sockfd].sockfd);
    LOG_INFO(info);
}

//�����µĿͻ�������
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
            string err = "���Ӵ��󣬴��������" + to_string(errno);
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
        string info = "����ͻ���" + string(inet_ntoa(users[sockfd].get_address()->sin_addr));
        LOG_INFO(info);

        //����⵽���¼��������¼������������
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
        string info = "�������ݸ��ͻ���" + string(inet_ntoa(users[sockfd].get_address()->sin_addr));
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
    //�Ƿ���Ҫ����ʱ����
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

            //�����µ��Ŀͻ�����
            if (sockfd == m_listenfd)
            {
                bool flag = dealclinetdata();
                if (false == flag)
                    continue;
            }
            //EPOLLHUP���ڼ��Զ��Ƿ�ر����ӣ��ر�����
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                //�������˹ر����ӣ��Ƴ���Ӧ�Ķ�ʱ��
                util_timer* timer = users_timer[sockfd].timer;
                deal_timer(timer, sockfd);
            }
            //�����ź�
            else if ((sockfd == m_pipefd[0]) && (events[i].events & EPOLLIN))
            {
                bool flag = dealwithsignal(timeout, stop_server);
                if (false == flag) {
                    string err = "dealclientdata failure";
                    LOG_ERROR(err);
                }
            }
            //����ͻ������Ͻ��յ�������
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