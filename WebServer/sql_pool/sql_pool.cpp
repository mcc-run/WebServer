#include "sql_pool.h"

void sql_pool::init()
{
	//初始化私有变量
	max_count = 30;

	isDestory = false;

	queue = new mutex_queue<MYSQL*>(max_count);

	//初始化数据库池
	for (int i = 0; i < max_count; i++)
	{
		MYSQL* con = NULL;
		con = mysql_init(con);

		if (con == NULL)
		{
			cout << "数据库初始化失败" << endl;
		}
		con = mysql_real_connect(con, "localhost", "root", "1234", "WebServer", 3306, NULL, 0);

		if (con == NULL)
		{
			cout << "数据库连接失败" << endl;
			exit(1);
		}
		queue->push(con);
	}

	cout << "数据库初始化成功" << endl;

}

MYSQL* sql_pool::GetConnection()
{
	lock.lock();
	if (isDestory) {
		lock.unlock();
		return nullptr;
	}
	lock.unlock();
	MYSQL* conn = nullptr;
	queue->pop(conn);
	return conn;
}

bool sql_pool::ReleaseConnection(MYSQL* conn)
{
	lock.lock();
	if (isDestory) {
		mysql_close(conn);
		lock.unlock();
		return true;
	}
	lock.unlock();
	queue->push(conn);
	return true;
}

int sql_pool::GetFreeConn()
{
	return 0;
}

void sql_pool::DestroyPool()
{
	lock.lock();
	isDestory = true;
	lock.unlock();
	while (queue->size())
	{
		MYSQL* con = nullptr;
		queue->pop(con);
		mysql_close(con);
	}
}

sql_pool_RAII::sql_pool_RAII(MYSQL** conn)
{
	*conn = sql_pool::getinstance()->GetConnection();
	con = *conn;
}

sql_pool_RAII::~sql_pool_RAII()
{
	sql_pool::getinstance()->ReleaseConnection(con);
}