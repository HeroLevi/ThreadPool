#include "ThreadPool.h"

ThreadPool* ThreadPool::InitThreadPool(int max,int min,int queue_max)
{
	int err;
	ThreadPool* pool;

	if((pool = (ThreadPool*)malloc(sizeof(ThreadPool))) == NULL)
		return NULL;
	pool->shutdown = TRUE;

	pool->thread_max = max;
	pool->thread_min = min;
	pool->thread_alive = 0;
	pool->thread_busy = 0;
	pool->thread_kill = 0;

	if((pool->task = (task_t*)malloc(sizeof(task_t))) == NULL)
		return NULL;

	pool->queue_max = max;
	pool->queue_head = 0;
	pool->queue_tail = 0;
	pool->queue_cur = 0;

	if(pthread_cond_init(&pool->not_empty,NULL) != 0 || \
			pthread_cond_init(&pool->not_full,NULL) != 0 || \
			pthread_mutex_init(&pool->lock_task,NULL) != 0)
	{
		perror("pool_cond_mutex_init faild");
		return NULL;
	}
	if((pool->arr_pthread_tid = (pthread_t *)malloc(sizeof(pthread_t)*max)) == NULL)
	{
		perror("pool->arr_pthread_tid malloc faild");
		return NULL;
	}
	bzero(pool->arr_pthread_tid,sizeof(pthread_t)*max);

	pool->manage_tid = 0;
	if((err = pthread_create(&pool->manage_tid,NULL,ManagerThreadPool,(void*)pool)) > 0)
	{
		printf("create pthread faild err = %s\n",strerror(err));
		return NULL;
	}

	for(int i=0;i<min;++i)
	{
		if((err = pthread_create(&pool->arr_pthread_tid[i],NULL,ConsumeTask,(void*)pool)) > 0)
		{
			printf("create pthread faild err = %s\n",strerror(err));
			return NULL;
		}
		++pool->thread_alive;
	}
	return pool;
}

int ThreadPool::DestroyThreadPool(ThreadPool* pool)
{
	int err;
	pthread_mutex_lock(&pool->lock_task);
	pool->shutdown = FALSE;
	pthread_mutex_unlock(&pool->lock_task);
	for(int i=0;i<pool->thread_alive;++i)
	{
		pthread_cond_signal(&pool->not_empty);
	}
	sleep(TIME_OUT);
	if((err = pthread_mutex_destroy(&pool->lock_task)) > 0)
	{
		printf("destroy pool->mutex faild err = %s\n",strerror(err));
		return FALSE;
	}
	if((err = pthread_cond_destroy(&pool->not_full)) > 0)
	{
		printf("destroy pool->not_full failed err = %s\n",strerror(err));
		return FALSE;
	}
	if((err = pthread_cond_destroy(&pool->not_empty)) > 0)
	{
		printf("destroy pool->not_empty failed err = %s\n",strerror(err));
		return FALSE;
	}

	free(pool->arr_pthread_tid);
	free(pool->task);
	free(pool);
	return TRUE;
}

void* ThreadPool::ManagerThreadPool(void* arg)
{
	if(arg == NULL)
		return NULL;
	ThreadPool* pool = (ThreadPool*)arg;
	int alive;
	int busy;

	while(pool->shutdown)
	{
		pthread_mutex_lock(&pool->lock_task);
		alive = pool->thread_alive;
		busy = pool->thread_busy;
		pthread_mutex_unlock(&pool->lock_task);

		if(((float)busy / alive * 100 >= (float)70) &&\
				(pool->thread_max >= alive + pool->thread_min))
		{
			int err;
			int add = 0;
			for(int i=0;i<pool->thread_max;++i)
			{
				pthread_mutex_lock(&pool->lock_task);
				if(pool->arr_pthread_tid[i] == 0 || !if_thread_alive(pool->arr_pthread_tid[i]))
				{
					if((err = pthread_create(&pool->arr_pthread_tid[i],NULL,ConsumeTask,(void*)pool)) > 0)
					{
						printf("create pthread failed err =%s\n",strerror(err));
						return NULL;
					}
					++pool->thread_alive;
					++add;
				}
				pthread_mutex_unlock(&pool->lock_task);
				if(add == DEF_LIMIT)
					break;
			}
		}
		if(busy * 2 < alive - busy && alive > pool->thread_min)
		{
			pthread_mutex_lock(&pool->lock_task);
			pool->thread_kill = DEF_LIMIT;
			pthread_mutex_unlock(&pool->lock_task);
			for(int j=0;j<DEF_LIMIT;++j)
			{
				pthread_cond_signal(&pool->not_empty);
			}
		}
		sleep(TIME_OUT);
	}
	return NULL;
}

int ThreadPool::Addtask(ThreadPool* pool,void* (*task)(void*),void* arg)
{
	if(pool == NULL || task == NULL)
		return FALSE;
	if(pool->shutdown == FALSE)
		return FALSE;
	pthread_mutex_lock(&pool->lock_task);
	if(pool->queue_cur == pool->queue_max)
		pthread_cond_wait(&pool->not_full,&pool->lock_task);
	if(pool->shutdown == FALSE)
	{
		pthread_mutex_unlock(&pool->lock_task);
		return FALSE;
	}
	pool->task[pool->queue_head].arg = arg;
	pool->task[pool->queue_head].task = task;
	pool->queue_head = (pool->queue_head + 1) % pool->queue_max;
	++pool->queue_cur;

	pthread_cond_signal(&pool->not_empty);
	pthread_mutex_unlock(&pool->lock_task);
	return TRUE;
}

void* ThreadPool::ConsumeTask(void* arg)
{
	if(arg == NULL)
		return NULL;
	ThreadPool* pool = (ThreadPool*)arg;
	task_t task;
	while(pool->shutdown != FALSE)
	{
		pthread_mutex_lock(&pool->lock_task);
		if(pool->queue_cur == 0)
			pthread_cond_wait(&pool->not_empty,&pool->lock_task);
		if(pool->shutdown == FALSE)
		{
			pthread_mutex_unlock(&pool->lock_task);
			pthread_exit(NULL);
		}
		if(pool->thread_kill > 0 && pool->thread_alive > pool->thread_min)
		{
			--pool->thread_alive;
			--pool->thread_kill;
			pthread_mutex_unlock(&pool->lock_task);
			pthread_exit(NULL);
		}
		task.task = pool->task[pool->queue_tail].task;
		task.arg = pool->task[pool->queue_tail].arg;

		pool->queue_tail = (pool->queue_tail +1) % pool->queue_max;
		--pool->queue_cur;
		pthread_cond_signal(&pool->not_full);
		++pool->thread_busy;
		pthread_mutex_unlock(&pool->lock_task);

		(*task.task)(task.arg);
		pthread_mutex_lock(&pool->lock_task);
		--pool->thread_busy;
		pthread_mutex_unlock(&pool->lock_task);
	}
	return NULL;
}

int ThreadPool::if_thread_alive(pthread_t tid)
{
	if((pthread_kill(tid,0)) == -1)
	{
		if(errno == ESRCH)
			return FALSE;
	}
	return TRUE;
}

