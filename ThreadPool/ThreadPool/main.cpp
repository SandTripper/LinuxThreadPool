#include <cstdio>
#include "threadpool.h"

void taskFunc(void* arg)
{
	int num = *(int*)arg;
	printf("thread %ld is working, number = %d\n", pthread_self(), num );
	sleep(1);
}

int main()
{
	//创建线程池
	puts("good");
	ThreadPool* pool = new ThreadPool(3,10, 100);
	for (int i = 0; i < 100; i++)
	{
		int* num = new int;
		*num = i + 100;
		pool->addTask(taskFunc, num);
	}
	sleep(30);

	delete pool;
	return 0;
}