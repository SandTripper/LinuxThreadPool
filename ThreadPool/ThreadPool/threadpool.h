#pragma once
#include <pthread.h>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <deque>
#include <map>

//����ṹ��
struct Task
{
	void (*function)(void* arg);
	void* arg;
};

//�����߳����к���
void* worker(void* arg);

//������
void* manager(void* arg);

//�̳߳ؽṹ��

class ThreadPool
{
public:

	//�����̳߳ز���ʼ��
	ThreadPool(int min, int max, int queueSize);

	//�����̳߳�
	~ThreadPool();

	//�������
	void addTask(void(*func)(void*),void *arg);

	//��ȡ��ǰ�ڹ������̵߳ĸ���

	int getBusyNum();


	//��ȡ�����̵߳ĸ���

	int getLiveNum();




	//�������
	std::deque<Task>*taskQ;
	int queueSize;		//��ǰ������д�С
	int queueCapacity;	//����������ֵ;

	pthread_t managerID; //�������߳�ID
	pthread_t* threadIDs; //�����̵߳�ID
	std::map< pthread_t, int>*IndofthreadIDs;
	std::deque<int>*diedThIDs;//δ�����Ĺ����߳���threadIDs�������

	int minNum; //��С�߳���
	int maxNum; //����߳���
	int busyNum; //��ǰ�ڹ������̵߳ĸ���
	int liveNum; //��ǰ�����̸߳���
	int exitNum; //Ҫ���ٵ��̸߳���
	pthread_mutex_t mutexPool; //�������̳߳�
	pthread_mutex_t mutexBusy; //��busyNum����
	pthread_cond_t notFull;		//��������Ƿ�����
	pthread_cond_t notEmpty;	//��������Ƿ����

	int shutdown; //�Ƿ������̳߳�,��Ϊ1������Ϊ0
};


//�߳��˳�����
void threadExit(ThreadPool* pool);