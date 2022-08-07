#pragma once
#include <pthread.h>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <deque>
#include <map>

//任务结构体
struct Task
{
	void (*function)(void* arg);
	void* arg;
};

//工作线程运行函数
void* worker(void* arg);

//管理函数
void* manager(void* arg);

//线程池结构体

class ThreadPool
{
public:

	//创建线程池并初始化
	ThreadPool(int min, int max, int queueSize);

	//销毁线程池
	~ThreadPool();

	//添加任务
	void addTask(void(*func)(void*),void *arg);

	//获取当前在工作的线程的个数

	int getBusyNum();


	//获取存活的线程的个数

	int getLiveNum();




	//任务队列
	std::deque<Task>*taskQ;
	int queueSize;		//当前任务队列大小
	int queueCapacity;	//任务队列最大值;

	pthread_t managerID; //管理者线程ID
	pthread_t* threadIDs; //工作线程的ID
	std::map< pthread_t, int>*IndofthreadIDs;
	std::deque<int>*diedThIDs;//未工作的工作线程在threadIDs里的索引

	int minNum; //最小线程数
	int maxNum; //最大线程数
	int busyNum; //当前在工作的线程的个数
	int liveNum; //当前存活的线程个数
	int exitNum; //要销毁的线程个数
	pthread_mutex_t mutexPool; //锁整个线程池
	pthread_mutex_t mutexBusy; //锁busyNum变量
	pthread_cond_t notFull;		//任务队列是否满了
	pthread_cond_t notEmpty;	//任务队列是否空了

	int shutdown; //是否销毁线程池,是为1，不是为0
};


//线程退出函数
void threadExit(ThreadPool* pool);