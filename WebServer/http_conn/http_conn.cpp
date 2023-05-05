#include "http_conn.h"


#include <fstream>

//����http��Ӧ��һЩ״̬��Ϣ
const char* ok_200_title = "OK";
const char* error_400_title = "Bad Request";
const char* error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char* error_403_title = "Forbidden";
const char* error_403_form = "You do not have permission to get file form this server.\n";
const char* error_404_title = "Not Found";
const char* error_404_form = "The requested file was not found on this server.\n";
const char* error_500_title = "Internal Error";
const char* error_500_form = "There was an unusual problem serving the request file.\n";

locker m_lock;
static map<string, string> users;

void http_conn::initmysql_result()
{
    //�ȴ����ӳ���ȡһ������
    MYSQL* mysql = NULL;
    sql_pool_RAII mysqlcon(&mysql);

    //��user���м���username��passwd���ݣ������������
    if (mysql_query(mysql, "SELECT username,passwd FROM user"))
    {
        string info = mysql_error(mysql);
        LOG_ERROR(info);
    }

    //�ӱ��м��������Ľ����
    MYSQL_RES* result = mysql_store_result(mysql);

    //���ؽ�����е�����
    int num_fields = mysql_num_fields(result);

    //���������ֶνṹ������
    MYSQL_FIELD* fields = mysql_fetch_fields(result);

    //�ӽ�����л�ȡ��һ�У�����Ӧ���û��������룬����map��
    while (MYSQL_ROW row = mysql_fetch_row(result))
    {
        if (row == NULL) { // �ж�fetch���������Ƿ�Ϊ��
            int errcode = mysql_errno(mysql); // ��ȡMySQL������
            const char* errmsg = mysql_error(mysql); // ��ȡMySQL������Ϣ
            cerr << "mysql_fetch_row() error: " << errcode << " " << errmsg << endl;
            break; // �˳�whileѭ��
        }
        string temp1(row[0]);
        string temp2(row[1]);
        cout << temp1 << " " << temp2 << endl;
        users[temp1] = temp2;
    }
}

//���ļ����������÷�����
int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

//���ں��¼���ע����¼���ETģʽ��ѡ����EPOLLONESHOT
void addfd(int epollfd, int fd, bool one_shot)
{
    epoll_event event{};
    event.data.fd = fd;

    //���������� EPOLLRDHUP �¼������Զ˹ر����ӻ�ر�д����ʱ��epoll ʵ���᷵��һ�� EPOLLRDHUP �¼���
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;

    if (one_shot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

//���ں�ʱ���ɾ��������
void removefd(int epollfd, int fd)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

//���¼�����ΪEPOLLONESHOP    ev��ʾҪ������ʱ������
void modfd(int epollfd, int fd, int ev)
{
    epoll_event event{};
    event.data.fd = fd;

    event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;

    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

int http_conn::m_user_count = 0;
int http_conn::m_epollfd = -1;

//�ر����ӣ��ر�һ�����ӣ��ͻ�������һ
void http_conn::close_conn(bool real_close)
{
    if (real_close && (m_sockfd != -1))
    {
        printf("close %d\n", m_sockfd);
        removefd(m_epollfd, m_sockfd);
        m_sockfd = -1;
        m_user_count--;
    }
}

void http_conn::work()
{
    if (m_state) {
        //д
        write();
    }
    else
    {
        //��
        process();
    }
}

//��ʼ������,�ⲿ���ó�ʼ���׽��ֵ�ַ
void http_conn::init(int sockfd, const sockaddr_in& addr)
{
    m_sockfd = sockfd;
    m_address = addr;

    addfd(m_epollfd, sockfd, true);
    m_user_count++;

    init();
}

//��ʼ���½��ܵ�����
//check_stateĬ��Ϊ����������״̬
void http_conn::init()
{
    mysql = NULL;
    bytes_to_send = 0;
    bytes_have_send = 0;
    m_check_state = CHECK_STATE_REQUESTLINE;
    m_linger = false;
    m_method = GET;
    m_url = 0;
    m_version = 0;
    m_content_length = 0;
    m_host = 0;
    m_start_line = 0;
    m_checked_idx = 0;
    m_read_idx = 0;
    m_write_idx = 0;
    cgi = 0;
    m_state = 0;
    timer_flag = 0;
    improv = 0;

    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
    memset(m_real_file, '\0', FILENAME_LEN);
}

//��״̬�������ڷ�����һ������
//����ֵΪ�еĶ�ȡ״̬����LINE_OK,LINE_BAD,LINE_OPEN
http_conn::LINE_STATUS http_conn::parse_line()
{
    char temp;
    for (; m_checked_idx < m_read_idx; ++m_checked_idx)
    {
        temp = m_read_buf[m_checked_idx];
        //��ȡ��\r\n���ʾ��ȡ��һ�н���
        if (temp == '\r')
        {
            if ((m_checked_idx + 1) == m_read_idx)
                return LINE_OPEN;
            else if (m_read_buf[m_checked_idx + 1] == '\n')
            {
                m_read_buf[m_checked_idx++] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        else if (temp == '\n')
        {
            if (m_checked_idx > 1 && m_read_buf[m_checked_idx - 1] == '\r')
            {
                m_read_buf[m_checked_idx - 1] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

//ѭ����ȡ�ͻ����ݣ�ֱ�������ݿɶ���Է��ر�����
//������ET����ģʽ�£���Ҫһ���Խ����ݶ���
bool http_conn::read_once()
{
    if (m_read_idx >= READ_BUFFER_SIZE)
    {
        return false;
    }
    int bytes_read = 0;

    
    //ET������ --  ����һ���Զ�ȡ����������
    while (true)
    {
        bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
        if (bytes_read == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            return false;
        }
        else if (bytes_read == 0)
        {
            return false;
        }
        m_read_idx += bytes_read;
    }
    return true;
}

//����http�����У�������󷽷���Ŀ��url��http�汾��
http_conn::HTTP_CODE http_conn::parse_request_line(char* text)
{
    //��ȡtext�еĵ�һ���ո���Ʊ�����λ��
    m_url = strpbrk(text, " \t");
    if (!m_url)
    {
        return BAD_REQUEST;
    }
    //�ҵ���һ���ո���Ʊ����󻻳�\0�������method��ȡ����
    *m_url++ = '\0';

    //��ȡ��get������post����
    char* method = text;
    if (strcasecmp(method, "GET") == 0)
        m_method = GET;
    else if (strcasecmp(method, "POST") == 0)
    {
        m_method = POST;
        cgi = 1;
    }
    else
        return BAD_REQUEST;

    //����murl�п�ͷ�Ŀո���Ʊ���
    //strspn �����᷵�� m_url �ַ����п�ͷ�����Ŀո���Ʊ����ĸ���
    //����Ϻ���Ĵ��뽫murl����Ŀո���Ʊ�������\0�Ӷ���ȡ��Ҫ����Դ
    m_url += strspn(m_url, " \t");

    //��ȡhttp�İ汾
    m_version = strpbrk(m_url, " \t");
    if (!m_version)
        return BAD_REQUEST;
    *m_version++ = '\0';
    m_version += strspn(m_version, " \t");
    if (strcasecmp(m_version, "HTTP/1.1") != 0)
        return BAD_REQUEST;
    if (strncasecmp(m_url, "http://", 7) == 0)
    {
        m_url += 7;
        //��ȡmurl�е�һ��/��λ��
        m_url = strchr(m_url, '/');
    }

    //����murl�е�httpͷ���Ӷ���ȡ����Ҫ����Դ
    if (strncasecmp(m_url, "https://", 8) == 0)
    {
        m_url += 8;
        m_url = strchr(m_url, '/');
    }


    if (!m_url || m_url[0] != '/')
        return BAD_REQUEST;

    //��urlΪ/ʱ����ʾ�жϽ���
    if (strlen(m_url) == 1)
        strcat(m_url, "judge.html");

    //�����н�����ϣ�ת���ɽ�������ͷ
    m_check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

//����http�����һ��ͷ����Ϣ
http_conn::HTTP_CODE http_conn::parse_headers(char* text)
{
    //�ж��ǿ��л�������ͷ�����ǿ���������������ѽ�����ϣ����������ǰ�����ݣ��������ȡ��һ�����ݼ���������֪�����������б�ʾ������
    if (text[0] == '\0')
    {
        if (m_content_length != 0)
        {
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }

    else if (strncasecmp(text, "Connection:", 11) == 0)
    {
        text += 11;
        text += strspn(text, " \t");
        if (strcasecmp(text, "keep-alive") == 0)
        {
            m_linger = true;
        }
    }
    else if (strncasecmp(text, "Content-length:", 15) == 0)
    {
        text += 15;
        text += strspn(text, " \t");
        m_content_length = atol(text);
    }
    else if (strncasecmp(text, "Host:", 5) == 0)
    {
        text += 5;
        text += strspn(text, " \t");
        m_host = text;
    }
    else
    {
        string info = text;
        LOG_INFO(info);
    }
    return NO_REQUEST;
}

//�ж�http�����Ƿ���������
http_conn::HTTP_CODE http_conn::parse_content(char* text)
{
    if (m_read_idx >= (m_content_length + m_checked_idx))
    {
        text[m_content_length] = '\0';
        //POST���������Ϊ������û���������
        m_string = text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::process_read()
{
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char* text = 0;

    while ((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || ((line_status = parse_line()) == LINE_OK))
    {
        text = get_line();
        m_start_line = m_checked_idx;
        string info = text;
        LOG_INFO(text);
        switch (m_check_state)
        {
            //����������
        case CHECK_STATE_REQUESTLINE:
        {
            ret = parse_request_line(text);
            if (ret == BAD_REQUEST)
                return BAD_REQUEST;
            break;
        }
        //��������ͷ
        case CHECK_STATE_HEADER:
        {
            ret = parse_headers(text);
            if (ret == BAD_REQUEST)
                return BAD_REQUEST;
            else if (ret == GET_REQUEST)
            {
                return do_request();
            }
            break;
        }
        //����������
        case CHECK_STATE_CONTENT:
        {
            ret = parse_content(text);
            if (ret == GET_REQUEST)
                return do_request();
            line_status = LINE_OPEN;
            break;
        }
        default:
            return INTERNAL_ERROR;
        }
    }
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::do_request()
{
    strcpy(m_real_file, doc_root);
    int len = strlen(doc_root);
    //printf("m_url:%s\n", m_url);
    const char* p = strrchr(m_url, '/');

    //����cgi,ʵ�ֵ�¼��ע��У��
    if (cgi == 1 && (*(p + 1) == '2' || *(p + 1) == '3'))
    {

        //���ݱ�־�ж��ǵ�¼��⻹��ע����
        char flag = m_url[1];

        char* m_url_real = (char*)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/");
        strcat(m_url_real, m_url + 2);
        strncpy(m_real_file + len, m_url_real, FILENAME_LEN - len - 1);
        free(m_url_real);

        //���û�����������ȡ����
        //user=123&passwd=123
        char name[100], password[100];
        int i;
        for (i = 5; m_string[i] != '&'; ++i)
            name[i - 5] = m_string[i];
        name[i - 5] = '\0';

        int j = 0;
        for (i = i + 10; m_string[i] != '\0'; ++i, ++j)
            password[j] = m_string[i];
        password[j] = '\0';

        if (*(p + 1) == '3')
        {
            //�����ע�ᣬ�ȼ�����ݿ����Ƿ���������
            //û�������ģ�������������
            char* sql_insert = (char*)malloc(sizeof(char) * 200);
            strcpy(sql_insert, "INSERT INTO user(username, passwd) VALUES(");
            strcat(sql_insert, "'");
            strcat(sql_insert, name);
            strcat(sql_insert, "', '");
            strcat(sql_insert, password);
            strcat(sql_insert, "')");

            //������ݿ��Ƿ��Ѿ��и��û�����û��������µ�����
            if (users.find(name) == users.end())
            {
                m_lock.lock();
                int res = mysql_query(mysql, sql_insert);
                users.insert(pair<string, string>(name, password));
                m_lock.unlock();

                if (!res)
                    strcpy(m_url, "/log.html");
                else
                    strcpy(m_url, "/registerError.html");
            }
            else
                strcpy(m_url, "/registerError.html");
        }
        //����ǵ�¼��ֱ���ж�
        //���������������û����������ڱ��п��Բ��ҵ�������1�����򷵻�0
        else if (*(p + 1) == '2')
        {
            if (users.find(name) != users.end() && users[name] == password)
                strcpy(m_url, "/welcome.html");
            else
                strcpy(m_url, "/logError.html");
        }
    }

    //��תע��ҳ��
    if (*(p + 1) == '0')
    {
        char* m_url_real = (char*)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/register.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    //��ת��¼ҳ��
    else if (*(p + 1) == '1')
    {
        char* m_url_real = (char*)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/log.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    //ͼƬ����ҳ��
    else if (*(p + 1) == '5')
    {
        char* m_url_real = (char*)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/picture.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    //��Ƶ����ҳ��
    else if (*(p + 1) == '6')
    {
        char* m_url_real = (char*)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/video.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    //��עҳ��
    else if (*(p + 1) == '7')
    {
        char* m_url_real = (char*)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/fans.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    //welcomeҳ�棬����ҳ
    else
        strncpy(m_real_file + len, m_url, FILENAME_LEN - len - 1);

    //��ȡ�����ļ�������
    if (stat(m_real_file, &m_file_stat) < 0)
        return NO_RESOURCE;

    //�ж��ļ��Ƿ�ɶ�
    if (!(m_file_stat.st_mode & S_IROTH))
        return FORBIDDEN_REQUEST;

    //�ж��ļ��Ƿ���Ŀ¼
    if (S_ISDIR(m_file_stat.st_mode))
        return BAD_REQUEST;

    //��ֻ����ʽ���ļ�
    int fd = open(m_real_file, O_RDONLY);
    //�����ļ�ӳ��ȥ
    m_file_address = (char*)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);

    return FILE_REQUEST;
}

void http_conn::unmap()
{
    if (m_file_address)
    {
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = 0;
    }
}

bool http_conn::write()
{
    int temp = 0;

    //����Ӧ������Ҫ����
    if (bytes_to_send == 0)
    {
        modfd(m_epollfd, m_sockfd, EPOLLIN);
        init();
        return true;
    }

    while (1)
    {
        //����Ӧ���ĺ�����Ҫ�����ݷ��͸��ͻ���
        temp = writev(m_sockfd, m_iv, m_iv_count);

        //����ʧ��
        if (temp < 0)
        {
            //socket��д���������ˣ��ȴ���д
            if (errno == EAGAIN)
            {
                modfd(m_epollfd, m_sockfd, EPOLLOUT);
                return true;
            }
            unmap();
            return false;
        }

        bytes_have_send += temp;
        bytes_to_send -= temp;
        if (bytes_have_send >= m_iv[0].iov_len)
        {
            m_iv[0].iov_len = 0;
            m_iv[1].iov_base = m_file_address + (bytes_have_send - m_write_idx);
            m_iv[1].iov_len = bytes_to_send;
        }
        else
        {
            m_iv[0].iov_base = m_write_buf + bytes_have_send;
            m_iv[0].iov_len = m_iv[0].iov_len - bytes_have_send;
        }

        //����ȫ���������
        if (bytes_to_send <= 0)
        {
            unmap();
            modfd(m_epollfd, m_sockfd, EPOLLIN);
            m_state = 0;

            if (m_linger)
            {
                init();
                return true;
            }
            else
            {
                return false;
            }
        }
    }
}
bool http_conn::add_response(const char* format, ...)
{
    //����д������
    if (m_write_idx >= WRITE_BUFFER_SIZE)
        return false;
    //��ʼ���ɱ����format�б�
    va_list arg_list;
    va_start(arg_list, format);

    //������format�ӿɱ�����б�д�뻺��д������д�����ݵĳ���
    int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list);
    //�������������򱨴�
    if (len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx))
    {
        va_end(arg_list);
        return false;
    }
    m_write_idx += len;
    //��տɱ�����б�
    va_end(arg_list);

    string info = m_write_buf;
    LOG_INFO(info);

    return true;
}
bool http_conn::add_status_line(int status, const char* title)
{
    return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}

bool http_conn::add_headers(int content_len)
{
    return add_content_length(content_len) && add_linger() &&
        add_blank_line();
}
bool http_conn::add_content_length(int content_len)
{
    return add_response("Content-Length:%d\r\n", content_len);
}
bool http_conn::add_content_type()
{
    return add_response("Content-Type:%s\r\n", "text/html");
}
bool http_conn::add_linger()
{
    return add_response("Connection:%s\r\n", (m_linger == true) ? "keep-alive" : "close");
}
bool http_conn::add_blank_line()
{
    return add_response("%s", "\r\n");
}
bool http_conn::add_content(const char* content)
{
    return add_response("%s", content);
}
bool http_conn::process_write(HTTP_CODE ret)
{
    switch (ret)
    {
    case INTERNAL_ERROR:
    {
        add_status_line(500, error_500_title);
        add_headers(strlen(error_500_form));
        if (!add_content(error_500_form))
            return false;
        break;
    }
    case BAD_REQUEST:
    {
        add_status_line(404, error_404_title);
        add_headers(strlen(error_404_form));
        if (!add_content(error_404_form))
            return false;
        break;
    }
    case FORBIDDEN_REQUEST:
    {
        add_status_line(403, error_403_title);
        add_headers(strlen(error_403_form));
        if (!add_content(error_403_form))
            return false;
        break;
    }
    case FILE_REQUEST:
    {
        add_status_line(200, ok_200_title);
        if (m_file_stat.st_size != 0)
        {
            add_headers(m_file_stat.st_size);
            m_iv[0].iov_base = m_write_buf;
            m_iv[0].iov_len = m_write_idx;
            m_iv[1].iov_base = m_file_address;
            m_iv[1].iov_len = m_file_stat.st_size;
            m_iv_count = 2;
            bytes_to_send = m_write_idx + m_file_stat.st_size;
            return true;
        }
        else
        {
            //����������Դ��СΪ0���򷵻ؿհ�html�ļ�
            const char* ok_string = "<html><body></body></html>";
            add_headers(strlen(ok_string));
            if (!add_content(ok_string))
                return false;
        }
    }
    default:
        return false;
    }
    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_idx;
    m_iv_count = 1;
    bytes_to_send = m_write_idx;
    return true;
}
void http_conn::process()
{
    HTTP_CODE read_ret = process_read();
    //������������Ҫ������ȡ��������
    if (read_ret == NO_REQUEST)
    {
        modfd(m_epollfd, m_sockfd, EPOLLIN);
        return;
    }

    bool write_ret = process_write(read_ret);
    if (!write_ret)
    {
        close_conn();
    }
    m_state = 1;
    modfd(m_epollfd, m_sockfd, EPOLLOUT);
}