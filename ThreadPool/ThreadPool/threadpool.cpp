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
		//��ǰ��������Ƿ�Ϊ��
		while (pool->queueSize==0&& !pool->shutdown)
		{
			//û�����������߳�
			pthread_cond_wait(&pool->notEmpty, &pool->mutexPool);

			//�ж��Ƿ�Ҫ�����Լ�
			if (pool->exitNum > 0)
			{
				pool->exitNum--;
				pool->liveNum--;
				pthread_mutex_unlock(&pool->mutexPool);//������ֹ����
				threadExit(pool);
				pthread_exit(NULL);
			}

		}
		//�ж��̳߳��Ƿ񱻹ر�
		if (pool->shutdown)
		{
			pthread_mutex_unlock(&pool->mutexPool);//������ֹ����
			threadExit(pool);
			pthread_exit(NULL);
		}

		//���������ȡ������
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

		//���ú���
		task.function(task.arg);
		//�ͷŲ���ռ�õ��ڴ�
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
		//���Ƶ����Ϊ3s
		sleep(3);

		//ȡ���̳߳��ж���������������͵�ǰ�̴߳����߳���
		pthread_mutex_lock(&pool->mutexPool);
		int queueSize = pool->queueSize;
		int liveNum = pool->liveNum;
		pthread_mutex_unlock(&pool->mutexPool);

		//ȡ�����ڹ������̵߳�����
		pthread_mutex_lock(&pool->mutexBusy);
		int busyNum = pool->busyNum;
		pthread_mutex_unlock(&pool->mutexBusy);

		//����߳�
		//����ĸ���>�����̸߳���&&�����߳���<����߳���
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

		//�����߳�
		//æ���߳�*2<�����߳���&&�����߳���>��С�߳���

		if (busyNum * 2 < liveNum && liveNum > pool->minNum)
		{
			pthread_mutex_lock(&pool->mutexPool);
			pool->exitNum = NUMBER;
			pthread_mutex_unlock(&pool->mutexPool);

			//�ù������߳���ɱ
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

	//��ʼ��threadID����

	diedThIDs = new std::deque<int>(maxnum);

	//��ʼ����Ϣ����

	taskQ = new std::deque<Task>;
	
	//��ʼ��IDת����
	IndofthreadIDs = new std::map<pthread_t, int>;

	if (pthread_mutex_init(&mutexPool, NULL) != 0 ||
		pthread_mutex_init(&mutexBusy, NULL) != 0 ||
		pthread_cond_init(&notEmpty, NULL) != 0 ||
		pthread_cond_init(&notFull, NULL) != 0)
	{
		//��ʼ����������������ʧ��
		throw "mutex or condition init fail";
	}

	shutdown = 0;

	//�����߳�

	pthread_create(&managerID, NULL, manager, this);
;
	for (int i = 0; i < minnum; ++i)
	{
		pthread_create(&threadIDs[i], NULL, worker, this);
	}

	//ʣ�µ�δ�����Ĺ����߳������������
	for (int i = minnum; i < maxnum; ++i)
	{
		diedThIDs->push_back(i);
	}
	
	
}

ThreadPool::~ThreadPool()
{
	//�ر��̳߳�
	shutdown = 1;

	//�������չ������߳�
	pthread_join(managerID, NULL);

	//�����������������߳�
	int livenum = liveNum;
	for (int i = 0; i < livenum; i++)
	{
		pthread_cond_signal(&notEmpty);
	}

	//�ͷŶ��ڴ�
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
		//�����������߳�
		pthread_cond_wait(&notFull, &mutexPool);
	}

	if (shutdown)
	{
		pthread_mutex_unlock(&mutexPool);
		return;
	}

	//�������
	Task task;
	task.function = func;
	task.arg = arg;
	taskQ->push_back(task);
	queueSize++;

	pthread_mutex_unlock(&mutexPool);
	//���������Ĺ����߳�
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


