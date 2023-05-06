#include "log.h"

void log::init()
{
	//初始化私有变量
	maxline = 2000;
	dir = "LOG/";

	lock.lock();
	cout << "10 lock" << endl;
	curline = 0;
	day_count = 0;
	filefd = -1;
	lock.unlock();
	cout << "14 unlock" << endl;
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
	cout << atoi(row[1]) << " " << atoi(row[2]) << " " << atoi(row[3]) << endl;
	if (time->tm_year != atoi(row[1]) || time->tm_mon != atoi(row[2]) || time->tm_mday != atoi(row[3])) {
		get_new_log();
	}
	else {
		//当天已经创建日志，打开新的日志
		string filename = dir + "WebServer" + to_string(time->tm_year) + "." + to_string(time->tm_mon) + "." + to_string(time->tm_mday);
		if (atoi(row[6])-1 > 0)filename += to_string(atoi(row[6])-1);

		lock.lock();
		cout << "47 lock" << endl;
		filefd = open(filename.c_str(), O_RDWR);
		lock.unlock();
		cout << "50 unlock" << endl;

		if (filefd == -1) {
			//文件打开失败
			get_new_log();
		}
		else
		{
			lock.lock();
			cout << "59 lock" << endl;
			curline = atoi(row[5]);
			day_count = atoi(row[6]);
			lock.unlock();
			cout << "63 unlock" << endl;
		}
	}

	//创建写线程将日志信息写入日志文件中
	pthread_t tid;
	pthread_create(&tid, NULL, flush_log_thread, NULL);
}

bool log::write_log(int level,const char* text)
{
	lock.lock();
	cout << "75 lock" << endl;
	if (filefd == -1)return false;
	lock.unlock();
	cout << "78 unlock" << endl;
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
		cout << "124 lock" << endl;
		cout << curline << " " << maxline << " " << (curline >= maxline) << endl;
		if (curline >= maxline) {
			lock.unlock();
			cout << "127 unlock" << endl;
			get_new_log();
			lock.lock();
			cout << "130 lock" << endl;
		}
		cout << filefd << endl;
		if (filefd == -1)
		{
			lock.unlock();
			cout << "135 unlock" << endl;
			continue;
		}
		cout << (queue == nullptr) << endl;
		if (queue == nullptr)return;
		lock.unlock();
		cout << "140 unlock" << endl;
		string log;
		queue->pop(log);

		lock.lock();
		cout << "145 lock" << endl;
		int fd = filefd;
		lock.unlock();
		cout << "148 unlock" << endl;
		write(fd, log.c_str(), log.length());
	}
}

void log::get_new_log()
{
	lock.lock();
	cout << "156 lock" << endl;
	if (filefd != -1) {
		close(filefd);
		lock.unlock();
		cout << "160 unlock" << endl;
		write_sql();
	}
	else lock.unlock();
	cout << "164 unlock" << endl;

	tm* time = get_time();
	string filename = dir + "WebServer" + to_string(time->tm_year) + "." + to_string(time->tm_mon) + "." + to_string(time->tm_mday);
	lock.lock();
	cout << "169 lock" << endl;
	if (day_count)filename += to_string(day_count);
	lock.unlock();
	cout << "172 unlock" << endl;
	int fd = open(filename.c_str(), O_CREAT | O_WRONLY, 777);
	if (fd == -1) {
		std::cerr << "创建新的日志文件失败\n";
		return;
	}
	else
	{
		lock.lock();
		cout << "181 lock" << endl;
		filefd = fd;
		curline = 0;
		day_count++;
		lock.unlock();
		cout << "186 unlock" << endl;
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
	cout << "205 lock" << endl;
	int cur = curline;
	int count = day_count;
	lock.unlock();
	cout << "209 unlock" << endl;

	tm* time = get_time();
	string sql = "INSERT INTO log(year,month,day,name,cur_line,day_count) VALUES(";
	sql += to_string(time->tm_year) + ",";
	sql += to_string(time->tm_mon) + ",";
	sql += to_string(time->tm_mday) + "," + "\" \"" + ",";
	sql += to_string(cur) + ",";
	sql += to_string(count) + ")";

	cout << sql << endl;
	
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
