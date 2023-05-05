#ifndef TIMER
#define TIMER

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <time.h>
#include "../log/log.h"
#include "../http_conn/http_conn.h"

class util_timer;

//������Դ
struct client_data
{
    //�ͻ���socket��ַ
    sockaddr_in address;
    //socket�ļ�������
    int sockfd;
    //��ʱ��
    util_timer* timer;
};

//��ʱ����
class util_timer
{
public:
    util_timer() : prev(NULL), next(NULL) {}

public:
    //����ʱ��
    time_t expire;

    //�ص�����
    void (*cb_func)(client_data*);
    //������Դ
    client_data* user_data;
    //ǰ��ʱ��
    util_timer* prev;
    //����ʱ��
    util_timer* next;
};

//��ʱ��������
class sort_timer_lst
{
public:
    sort_timer_lst();
    ~sort_timer_lst();

    //��Ӷ�ʱ��
    void add_timer(util_timer* timer);
    //������ʱ��λ�ã����������仯ʱ
    void adjust_timer(util_timer* timer);
    //ɾ����ʱ��
    void del_timer(util_timer* timer);
    //��ʱ��������
    void tick();

private:
    void add_timer(util_timer* timer, util_timer* lst_head);

    util_timer* head;
    util_timer* tail;
};

class Utils
{
public:
    Utils() {}
    ~Utils() {}

    void init(int timeslot);

    //���ļ����������÷�����
    int setnonblocking(int fd);

    //���ں��¼���ע����¼���ETģʽ��ѡ����EPOLLONESHOT
    void addfd(int epollfd, int fd, bool one_shot);

    //�źŴ�����
    static void sig_handler(int sig);

    //�����źź���
    void addsig(int sig, void(handler)(int), bool restart = true);

    //��ʱ�����������¶�ʱ�Բ��ϴ���SIGALRM�ź�
    void timer_handler();

    void show_error(int connfd, const char* info);

public:
    static int* u_pipefd;
    sort_timer_lst m_timer_lst;
    static int u_epollfd;
    int m_TIMESLOT;
};

//��ʱ���ص����� -- �ر��ļ���������http������-1
void cb_func(client_data* user_data);

#endif // !TIMER
