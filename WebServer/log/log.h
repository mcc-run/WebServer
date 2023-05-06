#ifndef LOG
#define LOG



#include "../sql_pool/sql_pool.h"
#include <fcntl.h>
#include <unistd.h>

#define LOG_DEBUG(text) {log::getinstance()->write_log(0, text);}
#define LOG_INFO(text)  {log::getinstance()->write_log(1, text);}
#define LOG_WARN(text)  {log::getinstance()->write_log(2, text);}
#define LOG_ERROR(text) {log::getinstance()->write_log(3, text);}


class log
{
private:

	mutex_queue<string>* queue;		//日志信息存放队列

	int curline;	//当前日志文件已有行数
	int maxline;	//单个日志文件最大行数

	int day_count;	//当天已记录的日志文件个数

	string dir;		//日志文件存储默认路径

	int filefd;		//日志文件的fd

	locker lock;
	

public:

	static log* getinstance() {
		static log instance;
		return &instance;
	}

	//异步写日志公有方法
	static void* flush_log_thread(void* args)
	{
		log::getinstance()->write_file();
		return nullptr;
	}

	void init();	//初始化日志

	bool write_log(int level,const char* text);	//将信息写入日志队列中

private:

	log(){}

	~log();

	void write_file();	//将日志写入至文件中

	void get_new_log();	//生成新的日志文件

	tm* get_time();	//获取年月日时分秒

	void write_sql();	//将最新的日志信息写入至数据库中

};

#endif // !LOG