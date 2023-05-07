#include "log.h"

void log::init()
{
	//初始化私有变量
	maxline = 2000;
	dir = "LOG/";
	queue = new mutex_queue<string>();

	lock.lock();
	curline = 0;
	day_count = 0;
	filefd = -1;
	lock.unlock();
	//从数据库中获取最新的日志文件信息
	MYSQL* conn =  nullptr;
	sql_pool_RAII getcon(&conn);

	if (conn == nullptr) {
		cout << "未获得数据库链接" << endl;
		return;
	}

	//从数据库中查询最新的一行数据
	if (mysql_query(conn, "SELECT * FROM log ORDER BY id DESC LIMIT 1")) {
		cout << "日志数据查询失败！" << endl;
		get_new_log();
		return;
	}

	auto res = mysql_use_result(conn);
	auto row = mysql_fetch_row(res);
	
	//若今天未创建日志，则创建新的日志
	tm* time = get_time();
	
	if (time->tm_year != atoi(row[1]) || time->tm_mon != atoi(row[2]) || time->tm_mday != atoi(row[3])) {
		get_new_log();
	}
	else {
		//当天已经创建日志，打开新的日志
		string filename = dir + "WebServer" + to_string(time->tm_year) + "." + to_string(time->tm_mon) + "." + to_string(time->tm_mday);
		if (atoi(row[6])-1 > 0)filename += to_string(atoi(row[6])-1);

		lock.lock();
		filefd = open(filename.c_str(), O_RDWR | O_APPEND);
		lock.unlock();

		if (filefd == -1) {
			//文件打开失败
			get_new_log();
		}
		else
		{
			lock.lock();
			curline = atoi(row[5]);
			day_count = atoi(row[6]);
			lock.unlock();
		}
	}

	//创建写线程将日志信息写入日志文件中
	pthread_t tid;
	pthread_create(&tid, NULL, flush_log_thread, NULL);
}

bool log::write_log(int level,const char* text)
{
	lock.lock();
	if (filefd == -1)return false;
	lock.unlock();
	tm* time = get_time();
	string log = to_string(time->tm_hour) + ":" + to_string(time->tm_min) + ":" + to_string(time->tm_sec) + " ";

	switch (level)
	{
	case 0:
		log += "[debug] : ";
		break;
	case 1:
		log += "[info] : ";
		break;
	case 2:
		log += "[warn] : ";
		break;
	case 3:
		log += "[erro] : ";
		break;
	default:
		log += "[info] : ";
		break;
	}

	log += text;
	log += "\n";

	queue->push(log);
	return true;
}

log::~log()
{
	//将今天的日志信息写入至文件
	delete queue;
	if (filefd != -1) {
		close(filefd);
		write_sql();
	}
}

void log::write_file()
{
	
	while (true)
	{
		lock.lock();
		if (curline >= maxline) {
			lock.unlock();
			get_new_log();
			lock.lock();
		}
		if (filefd == -1)
		{
			lock.unlock();
			continue;
		}
		if (queue == nullptr) {
			lock.unlock();
			return;
		}
		lock.unlock();
		string log;
		queue->pop(log);

		lock.lock();
		write(filefd, log.c_str(), log.length());
		curline++;
		lock.unlock();
	}
}

void log::get_new_log()
{
	lock.lock();
	if (filefd != -1) {
		close(filefd);
		lock.unlock();
		write_sql();
	}
	else lock.unlock();

	tm* time = get_time();
	string filename = dir + "WebServer" + to_string(time->tm_year) + "." + to_string(time->tm_mon) + "." + to_string(time->tm_mday);
	lock.lock();
	if (day_count)filename += to_string(day_count);
	lock.unlock();
	int fd = open(filename.c_str(), O_CREAT | O_WRONLY | O_APPEND, 777);
	if (fd == -1) {
		std::cerr << "创建新的日志文件失败\n";
		return;
	}
	else
	{
		lock.lock();
		filefd = fd;
		curline = 0;
		day_count++;
		lock.unlock();
	}
	cout << "创建日志成功！" << filename << endl;
	write_sql();
}

tm* log::get_time()
{
	//获取年月日时分秒
	time_t t = time(NULL);
	tm * time =localtime(&t);
	time->tm_year += 1900;
	time->tm_mon += 1;
	return time;
}

void log::write_sql()
{
	lock.lock();
	int cur = curline;
	int count = day_count;
	lock.unlock();

	tm* time = get_time();
	string sql = "INSERT INTO log(year,month,day,name,cur_line,day_count) VALUES(";
	sql += to_string(time->tm_year) + ",";
	sql += to_string(time->tm_mon) + ",";
	sql += to_string(time->tm_mday) + "," + "\" \"" + ",";
	sql += to_string(cur) + ",";
	sql += to_string(count) + ")";

	
	MYSQL* conn = nullptr;
	sql_pool_RAII getcon(&conn);

	if (conn == nullptr) {
		cout << "未获得数据库链接,日志写入失败" << endl;
		return;
	}

	if (mysql_query(conn, sql.c_str())) {
		cout << "写入数据失败" << endl;
	}
	else cout << "写入数据成功" << endl;

}
