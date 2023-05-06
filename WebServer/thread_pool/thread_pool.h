#ifndef THREADPOOL
#define THREADPOOL



#include <list>
#include "../lock/locker.h"
#include "../log/log.h"


template<typename T>
class thread_pool {
private:

	locker lock;	//������ʱ���
	int max_request;	//���������
	int cur_request;	//��ǰ������

	list<T*> requests;	//�����б�

	sem isrquest;	//�ź������ȴ�������
	
	pthread_t* mthreads;	//�߳�id����
	int thread_count;		//�߳�����

public:

	thread_pool() { init(); }

	void init();	//��ʼ��


	bool append(T* request);

	~thread_pool() {
		delete[] mthreads;
	}


private:


	

	/*�����߳����еĺ����������ϴӹ���������ȡ������ִ��֮*/
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

	isrquest = sem(max_request);

	thread_count = 30;
	mthreads = new pthread_t[thread_count];

	for (int i = 0; i < thread_count; i++)
	{
		if (pthread_create(mthreads + i, NULL, worker, this) != 0)
		{
			delete[] mthreads;
			LOG_ERROR("�̴߳���ʧ��");
			throw std::exception();
		}
		if (pthread_detach(mthreads[i]))
		{
			delete[] mthreads;
			LOG_ERROR("�̷߳���ʧ��");
			throw std::exception();
		}
	}

}

template<typename T>
inline bool thread_pool<T>::append(T* request)
{
	//�����������Ƿ�����

	lock.lock();
	if (cur_request >= max_request) {
		lock.unlock();
		return false;
	}

	//����������������б���
	requests.push_back(request);
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

		//ȡ������
		T* request = requests.front();
		requests.pop_front();
		lock.unlock();

		if (request == nullptr)continue;
		//��������
		request->process();
	}
}

#endif // !THREADPOOL