#ifndef _THREADPOOL_H
#define _THREADPOOL_H

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <ctype.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>

#define FALSE      0
#define TRUE       1
#define DEF_LIMIT 10
#define TIME_OUT   3

typedef struct
{
	void* (*task)(void*);
	void* arg;
}task_t;

class ThreadPool
{
public:
	int shutdown;//线程池开关
	int thread_max;//最大线程数
	int thread_min;//最小线程数
	int thread_alive;//活着的线程数量
	int thread_busy;//忙线程数
	int thread_kill;

	int queue_max;
	int queue_head;
	int queue_tail;
	int queue_cur;
	task_t* task;
	
	pthread_cond_t not_empty;
	pthread_cond_t not_full;
	pthread_mutex_t lock_task;
	
	pthread_t* arr_pthread_tid;
	pthread_t manage_tid;
	
public:	
	ThreadPool()
	{
		
	}
	~ThreadPool()
	{

	}
public:
	static ThreadPool* InitThreadPool(int max,int min,int queue_max);
	static int DestroyThreadPool(ThreadPool*);
	static void* ManagerThreadPool(void*);
	static int Addtask(ThreadPool*,void* (*task)(void*),void* arg);
	static void* ConsumeTask(void*);
	static int if_thread_alive(pthread_t tid);
};
#endif
