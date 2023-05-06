#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H
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
#include <map>

#include "../lock/locker.h"
#include "../sql_pool/sql_pool.h"
#include "../log/log.h"

class http_conn
{
public:
    //�ļ����ƵĴ�С
    static const int FILENAME_LEN = 200;
    //���������Ĵ�С
    static const int READ_BUFFER_SIZE = 2048;
    //д�������Ĵ�С
    static const int WRITE_BUFFER_SIZE = 1024;
    //���ĵ����󷽷���get��post
    enum METHOD
    {
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATH
    };
    //��״̬����״̬
    enum CHECK_STATE
    {
        //����������
        CHECK_STATE_REQUESTLINE = 0,
        //��������ͷ
        CHECK_STATE_HEADER,
        //������Ϣ�壬����post����ʱ������Ϣ��
        CHECK_STATE_CONTENT
    };
    //��״̬����״̬
    enum LINE_STATUS
    {
        //������ȡһ��
        LINE_OK = 0,
        //�����﷨�д���
        LINE_BAD,
        //��ȡ���в�����
        LINE_OPEN
    };
    //���Ľ����Ľ��
    enum HTTP_CODE
    {
        //������������Ҫ��������������
        NO_REQUEST,
        //��ȡ����������http����
        GET_REQUEST,
        //�������﷨����
        BAD_REQUEST,
        //������Դ������
        NO_RESOURCE,
        //������Դ��ֹ����
        FORBIDDEN_REQUEST,
        //������Դ��������
        FILE_REQUEST,
        //�������ڲ�����
        INTERNAL_ERROR,
        //�ر�����
        CLOSED_CONNECTION
    };

public:
    http_conn() {}
    ~http_conn() {}

public:
    //������г�ʼ������
    void init(int sockfd, const sockaddr_in& addr);
    //�ر�http����
    void close_conn(bool real_close = true);
    

    //���ڴ���ͻ��˵�����
    void process();

    
    //��ȡ�ͻ��˷��͹�������Ϣ
    bool read_once();
    //��Ӧ����д�뺯��
    bool write();

    sockaddr_in* get_address()
    {
        return &m_address;
    }
    //���ݿ��ʼ��    --  ���ڻ�ȡ���ݿ��е��û���������
    void initmysql_result();


    int timer_flag;
    //����ʱ����Ϊ1ֱ��������������Ϊ0���Ӷ�ȷ����󲻻�Ծʱ��
    int improv;


private:
    void init();
    //��ȡ�ͻ��˷��͹�������Ϣ������
    HTTP_CODE process_read();
    //д����Ӧ����
    bool process_write(HTTP_CODE ret);

    //��״̬�����������е�����������
    HTTP_CODE parse_request_line(char* text);
    //��״̬�����������е�����ͷ
    HTTP_CODE parse_headers(char* text);
    //��״̬�����������е���������
    HTTP_CODE parse_content(char* text);

    //������Ӧ����
    HTTP_CODE do_request();

    //��ȡ��һ��δ���������
    char* get_line() { return m_read_buf + m_start_line; };

    //��״̬����ȡһ�У������������ĵ���һ����
    LINE_STATUS parse_line();

    void unmap();

    //������Ӧ���ĵĸ�������
    //��ӵ���Ӧ������
    bool add_response(const char* format, ...);
    //����ı�����������
    bool add_content(const char* content);
    //���״̬��
    bool add_status_line(int status, const char* title);
    //�����Ϣͷ
    bool add_headers(int content_length);
    //����ı����ͣ�html
    bool add_content_type();
    //�����Ӧ���ĳ���
    bool add_content_length(int content_length);
    //����Ƿ񱣳�����
    bool add_linger();
    //��ӿ���
    bool add_blank_line();

public:
    static int m_epollfd;
    static int m_user_count;
    MYSQL* mysql;
    int m_state;  //��Ϊ0, дΪ1

private:
    int m_sockfd;
    sockaddr_in m_address;

    //��������
    char m_read_buf[READ_BUFFER_SIZE];
    //�����������ݵ���һ���ֽڵ�λ��
    long m_read_idx;
    //��ǰ���ڽ��������ݵ�λ��
    long m_checked_idx;
    //��ǰ�Ѿ��������ַ�����
    int m_start_line;

    //д������
    char m_write_buf[WRITE_BUFFER_SIZE];
    //д�����������ݵĳ���
    int m_write_idx;

    //��״̬����״̬
    CHECK_STATE m_check_state;

    //���󷽷�
    METHOD m_method;

    //�������ı����еĶ�Ӧ����
    char m_real_file[FILENAME_LEN];
    char* m_url;
    char* m_version;
    char* m_host;
    long m_content_length;
    bool m_linger;

    //��ȡ���������ļ��ĵ�ַ
    char* m_file_address;
    //������ļ�������
    struct stat m_file_stat;

    /*
    m_iv[0] ���ڴ洢д�������е����ݣ��� HTTP ��Ӧͷ����Ӧ���ġ��� m_iv[1] ���ڴ洢������ļ������ݡ�
    ��������ʹ�� mmap ������������ļ�ӳ�䵽�ڴ��У�����ӳ�����ڴ��ַ���ļ���С�ֱ�洢�� m_iv[1].iov_base �� m_iv[1].iov_len �С�
    ������������ writev ����ʱ������һ���Խ�д�������е����ݺ�������ļ������ݷ��͸��ͻ��ˡ�
    */
    struct iovec m_iv[2];
    //��Ҫ�����������
    int m_iv_count;

    //�Ƿ����õ�POST
    int cgi;
    //�洢����ͷ����
    char* m_string;

    //ʣ�෢�͵�����
    int bytes_to_send;
    //�ѷ��͵�����
    int bytes_have_send;


    char* doc_root;

    map<string, string> m_users;

    

    char sql_user[100];
    char sql_passwd[100];
    char sql_name[100];
};

#endif
