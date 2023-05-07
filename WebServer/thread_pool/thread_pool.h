﻿#ifndef THREADPOOL
#define THREADPOOL



#include <list>
#include "../lock/locker.h"
#include "../log/log.h"


template<typename T>
class thread_pool {
private:

	locker lock;	//互斥访问变量
	int max_request;	//最大请求数
	int cur_request;	//当前请求数

	list<T*> requests;	//请求列表

	sem isrquest;	//信号量，等待请求到来
	
	pthread_t* mthreads;	//线程id数组
	int thread_count;		//线程数量

public:

	thread_pool() { init(); }

	void init();	//初始化


	bool append(T* request);

	~thread_pool() {
		delete[] mthreads;
	}


private:

	/*工作线程运行的函数，它不断从工作队列中取出任务并执行之*/
	static void* worker(void* arg);
	void run();

};

template<typename T>
inline void thread_pool<T>::init()
{
	lock.lock();
	max_request = 120;
	cur_request = 0;
	lock.unlock();

	isrquest = sem(0);

	thread_count = 30;
	mthreads = new pthread_t[thread_count];

	for (int i = 0; i < thread_count; i++)
	{
		if (pthread_create(mthreads + i, NULL, worker, this) != 0)
		{
			delete[] mthreads;
			LOG_ERROR("线程创建失败");
			throw std::exception();
		}
		if (pthread_detach(mthreads[i]))
		{
			delete[] mthreads;
			LOG_ERROR("线程分离失败");
			throw std::exception();
		}
	}

}

template<typename T>
inline bool thread_pool<T>::append(T* request)
{
	//检查请求队列是否已满

	lock.lock();
	if (cur_request >= max_request) {
		lock.unlock();
		return false;
	}

	//将请求添加至请求列表中
	requests.push_back(request);
	cur_request++;
	lock.unlock();
	isrquest.post();
	return true;
}

template<typename T>
inline void* thread_pool<T>::worker(void* arg)
{
	thread_pool* pool = (thread_pool*)arg;
	pool->run();
	return nullptr;
}

template<typename T>
inline void thread_pool<T>::run()
{
	while (true)
	{
		isrquest.wait();
		lock.lock();
		if (mthreads == nullptr)return;
		if (cur_request <= 0) {
			lock.unlock();
			continue;
		}

		//取出任务
		T* request = requests.front();
		requests.pop_front();
		lock.unlock();

		if (request == nullptr)continue;
		//处理请求
		request->process();
	}
}

#endif // !THREADPOOL