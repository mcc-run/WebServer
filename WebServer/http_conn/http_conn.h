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
    //文件名称的大小
    static const int FILENAME_LEN = 200;
    //读缓冲区的大小
    static const int READ_BUFFER_SIZE = 2048;
    //写缓冲区的大小
    static const int WRITE_BUFFER_SIZE = 1024;
    //报文的请求方法，get或post
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
    //主状态机的状态
    enum CHECK_STATE
    {
        //解析请求行
        CHECK_STATE_REQUESTLINE = 0,
        //解析请求头
        CHECK_STATE_HEADER,
        //解析消息体，仅当post请求时才有消息体
        CHECK_STATE_CONTENT
    };
    //从状态机的状态
    enum LINE_STATUS
    {
        //完整读取一行
        LINE_OK = 0,
        //报文语法有错误
        LINE_BAD,
        //读取的行不完整
        LINE_OPEN
    };
    //报文解析的结果
    enum HTTP_CODE
    {
        //请求不完整，需要继续请求报文数据
        NO_REQUEST,
        //获取到了完整的http请求
        GET_REQUEST,
        //请求报文语法有误
        BAD_REQUEST,
        //请求资源不存在
        NO_RESOURCE,
        //请求资源禁止访问
        FORBIDDEN_REQUEST,
        //请求资源正常访问
        FILE_REQUEST,
        //服务器内部错误
        INTERNAL_ERROR,
        //关闭连接
        CLOSED_CONNECTION
    };

public:
    http_conn() {}
    ~http_conn() {}

public:
    //对类进行初始化操作
    void init(int sockfd, const sockaddr_in& addr);
    //关闭http连接
    void close_conn(bool real_close = true);
    

    //用于处理客户端的请求
    void process();

    
    //读取客户端发送过来的信息
    bool read_once();
    //响应报文写入函数
    bool write();

    sockaddr_in* get_address()
    {
        return &m_address;
    }
    //数据库初始化    --  用于获取数据库中的用户名和密码
    void initmysql_result();


    int timer_flag;
    //工作时设置为1直到工作结束设置为0，从而确保最后不活跃时间
    int improv;


private:
    void init();
    //读取客户端发送过来的信息并处理
    HTTP_CODE process_read();
    //写入响应报文
    bool process_write(HTTP_CODE ret);

    //主状态机解析报文中的请求行数据
    HTTP_CODE parse_request_line(char* text);
    //主状态机解析报文中的请求头
    HTTP_CODE parse_headers(char* text);
    //主状态机解析报文中的请求数据
    HTTP_CODE parse_content(char* text);

    //生成响应报文
    HTTP_CODE do_request();

    //获取第一个未处理的数据
    char* get_line() { return m_read_buf + m_start_line; };

    //从状态机读取一行，分析是请求报文的哪一部分
    LINE_STATUS parse_line();

    void unmap();

    //生成响应报文的各个部分
    //添加到相应报文中
    bool add_response(const char* format, ...);
    //添加文本，请求数据
    bool add_content(const char* content);
    //添加状态行
    bool add_status_line(int status, const char* title);
    //添加消息头
    bool add_headers(int content_length);
    //添加文本类型，html
    bool add_content_type();
    //添加相应报文长度
    bool add_content_length(int content_length);
    //添加是否保持连接
    bool add_linger();
    //添加空行
    bool add_blank_line();

public:
    static int m_epollfd;
    static int m_user_count;
    
    int m_state;  //读为0, 写为1

private:
    int m_sockfd;   //连接对应的文件描述符
    sockaddr_in m_address;

    //读缓冲区
    char m_read_buf[READ_BUFFER_SIZE];
    //缓冲区中数据的下一个字节的位置
    long m_read_idx;
    //当前正在解析的数据的位置
    long m_checked_idx;
    //当前已经解析的字符个数
    int m_start_line;

    //写缓冲区
    char m_write_buf[WRITE_BUFFER_SIZE];
    //写缓冲区中数据的长度
    int m_write_idx;

    //主状态机的状态
    CHECK_STATE m_check_state;

    //请求方法
    METHOD m_method;

    //解析出的报文中的对应变量
    char m_real_file[FILENAME_LEN];
    char* m_url;
    char* m_version;
    char* m_host;
    long m_content_length;
    bool m_linger;

    //读取服务器上文件的地址
    char* m_file_address;
    //请求的文件的属性
    struct stat m_file_stat;

    /*
    m_iv[0] 用于存储写缓冲区中的数据，即 HTTP 响应头和响应正文。而 m_iv[1] 用于存储请求的文件的内容。
    服务器会使用 mmap 函数将请求的文件映射到内存中，并将映射后的内存地址和文件大小分别存储在 m_iv[1].iov_base 和 m_iv[1].iov_len 中。
    当服务器调用 writev 函数时，它会一次性将写缓冲区中的数据和请求的文件的内容发送给客户端。
    */
    struct iovec m_iv[2];
    //需要传输的数据数
    int m_iv_count;

    //是否启用的POST
    int cgi;
    //存储请求头数据
    char* m_string;

    //剩余发送的数据
    int bytes_to_send;
    //已发送的数据
    int bytes_have_send;


    char* doc_root;

    map<string, string> m_users;

    

    char sql_user[100];
    char sql_passwd[100];
    char sql_name[100];
};

#endif
