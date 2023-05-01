#include <mysql.h>
#include "../lock/mutex_queue.h"

using namespace std;

class sql_pool
{
private:

	sql_pool(){}

	mutex_queue<MYSQL*>* queue;

	locker lock;

	bool isDestory;

	int max_count;

public:

	static sql_pool* getinstance() {
		static sql_pool instance;
		return &instance;
	}
	
	void init();	//初始化数据库池

	MYSQL* GetConnection();				 //获取数据库连接
	bool ReleaseConnection(MYSQL* conn); //释放连接
	int GetFreeConn();					 //获取连接
	void DestroyPool();					 //销毁所有连接

};

class sql_pool_RAII
{
public:
	sql_pool_RAII(MYSQL** conn);
	~sql_pool_RAII();

private:
	MYSQL* con;
};

