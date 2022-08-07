#include "threadpool.h"

const int NUMBER = 2;

void threadExit(ThreadPool* pool)
{
	pthread_t tid = pthread_self();

		pthread_mutex_lock(&pool->mutexPool);
		auto it = pool->IndofthreadIDs->find(tid);
		{
			pool->threadIDs[it->second] = 0;
			pool->diedThIDs->push_back(it->second);
			pool->IndofthreadIDs->erase(it);
		}
		pthread_mutex_unlock(&pool->mutexPool);

}

void* worker(void* arg)
{
	ThreadPool* pool = (ThreadPool*)arg;
	while (1)
	{
		pthread_mutex_lock(&pool->mutexPool);
		//当前任务队列是否为空
		while (pool->queueSize==0&& !pool->shutdown)
		{
			//没有任务，阻塞线程
			pthread_cond_wait(&pool->notEmpty, &pool->mutexPool);

			//判断是否要销毁自己
			if (pool->exitNum > 0)
			{
				pool->exitNum--;
				pool->liveNum--;
				pthread_mutex_unlock(&pool->mutexPool);//解锁防止死锁
				threadExit(pool);
				pthread_exit(NULL);
			}

		}
		//判断线程池是否被关闭
		if (pool->shutdown)
		{
			pthread_mutex_unlock(&pool->mutexPool);//解锁防止死锁
			threadExit(pool);
			pthread_exit(NULL);
		}

		//从任务队列取出任务
		Task task;
		task.function = pool->taskQ->front().function;
		task.arg = pool->taskQ->front().arg;
		pool->taskQ->pop_front();
		pool->queueSize--;
		pthread_cond_signal(&pool->notFull);
		pthread_mutex_unlock(&pool->mutexPool);

		pthread_mutex_lock(&pool->mutexBusy);
		pool->busyNum++;
		pthread_mutex_unlock(&pool->mutexBusy);

		//调用函数
		task.function(task.arg);
		//释放参数占用的内存
		delete task.arg;
		task.arg = NULL;

		puts("thread end working");

		pthread_mutex_lock(&pool->mutexBusy);
		pool->busyNum--;
		pthread_mutex_unlock(&pool->mutexBusy);
	}
	return NULL;
}

void* manager(void* arg)
{
	ThreadPool* pool = (ThreadPool*)arg;

	while (!pool->shutdown)
	{
		//检测频率设为3s
		sleep(3);

		//取出线程池中队列里任务的数量和当前线程存活的线程数
		pthread_mutex_lock(&pool->mutexPool);
		int queueSize = pool->queueSize;
		int liveNum = pool->liveNum;
		pthread_mutex_unlock(&pool->mutexPool);

		//取出正在工作的线程的数量
		pthread_mutex_lock(&pool->mutexBusy);
		int busyNum = pool->busyNum;
		pthread_mutex_unlock(&pool->mutexBusy);

		//添加线程
		//任务的个数>存活的线程个数&&存活的线程数<最大线程数
		if (queueSize > liveNum && liveNum < pool->maxNum)
		{
			pthread_mutex_lock(&pool->mutexPool);
			for (int i = 0; i < NUMBER && !pool->diedThIDs->empty(); i++)
			{
				pthread_create(&pool->threadIDs[pool->diedThIDs->front()], NULL, worker, pool);
				pool->diedThIDs->pop_front();
				pool->liveNum++;
			}
			pthread_mutex_unlock(&pool->mutexPool);
		}

		//销毁线程
		//忙的线程*2<存活的线程数&&存活的线程数>最小线程数

		if (busyNum * 2 < liveNum && liveNum > pool->minNum)
		{
			pthread_mutex_lock(&pool->mutexPool);
			pool->exitNum = NUMBER;
			pthread_mutex_unlock(&pool->mutexPool);

			//让工作的线程自杀
			for (int i = 0; i < NUMBER; i++)
			{
				pthread_cond_signal(&pool->notEmpty);
			}
		}

	}

	return NULL;
}

ThreadPool::ThreadPool(int minnum, int maxnum, int queuecapacity)
{
	threadIDs = new pthread_t[maxnum];
	memset(threadIDs, 0, sizeof(pthread_t) * maxnum);

	minNum = minnum;
	maxNum = maxnum;
	busyNum = 0;
	liveNum = minnum;
	exitNum = 0;
	queueCapacity = queuecapacity;
	queueSize = 0;

	//初始化threadID队列

	diedThIDs = new std::deque<int>(maxnum);

	//初始化消息队列

	taskQ = new std::deque<Task>;
	
	//初始化ID转索引
	IndofthreadIDs = new std::map<pthread_t, int>;

	if (pthread_mutex_init(&mutexPool, NULL) != 0 ||
		pthread_mutex_init(&mutexBusy, NULL) != 0 ||
		pthread_cond_init(&notEmpty, NULL) != 0 ||
		pthread_cond_init(&notFull, NULL) != 0)
	{
		//初始化锁或者条件变量失败
		throw "mutex or condition init fail";
	}

	shutdown = 0;

	//创建线程

	pthread_create(&managerID, NULL, manager, this);
;
	for (int i = 0; i < minnum; ++i)
	{
		pthread_create(&threadIDs[i], NULL, worker, this);
	}

	//剩下的未创建的工作线程索引放入队列
	for (int i = minnum; i < maxnum; ++i)
	{
		diedThIDs->push_back(i);
	}
	
	
}

ThreadPool::~ThreadPool()
{
	//关闭线程池
	shutdown = 1;

	//阻塞回收管理者线程
	pthread_join(managerID, NULL);

	//唤醒阻塞的消费者线程
	int livenum = liveNum;
	for (int i = 0; i < livenum; i++)
	{
		pthread_cond_signal(&notEmpty);
	}

	//释放堆内存
	delete taskQ;
	delete threadIDs;
	delete diedThIDs;
	pthread_mutex_destroy(&mutexBusy);
	pthread_mutex_destroy(&mutexPool);
	pthread_cond_destroy(&notEmpty);
	pthread_cond_destroy(&notFull);
}

void ThreadPool::addTask(void(*func)(void*), void* arg)
{
	pthread_mutex_lock(&mutexPool);
	while (queueSize == queueCapacity && !shutdown)
	{
		//阻塞生产者线程
		pthread_cond_wait(&notFull, &mutexPool);
	}

	if (shutdown)
	{
		pthread_mutex_unlock(&mutexPool);
		return;
	}

	//添加任务
	Task task;
	task.function = func;
	task.arg = arg;
	taskQ->push_back(task);
	queueSize++;

	pthread_mutex_unlock(&mutexPool);
	//唤醒阻塞的工作线程
	pthread_cond_signal(&notEmpty);


}

int ThreadPool::getBusyNum()
{
	pthread_mutex_lock(&mutexBusy);
	int ans = busyNum;
	pthread_mutex_unlock(&mutexBusy);
	return ans;
}

int ThreadPool::getLiveNum()
{
	pthread_mutex_lock(&mutexPool);
	int ans = liveNum;
	pthread_mutex_unlock(&mutexPool);
	return ans;
}


