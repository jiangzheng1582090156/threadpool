#ifndef __THREADPOOL_H__
#define __THREADPOOL_H__

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <signal.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>

#define true 1
#define false 0
#define DEFAULT_THREAD_VARY 10
#define MIN_WAIT_TASK_NUM 10
#define DEFAULT_WAIT_TIME 5

typedef struct
{
        void *(*function)(void *); /* 函数指针. 回调函数 */
        void *arg;                 /* 回调函数的参数    */
} threadpool_task_t;

/* 描述线程池相关信息 */
typedef struct threadpool_t
{
        pthread_mutex_t         lock;                    /* 锁住本结构体 */
        pthread_mutex_t         thread_counter;          /* 锁住忙线程数目 -- busy_thr_num */
        pthread_cond_t          queue_not_full;          /* 当任务队列为满的时候, 添加任务的线程阻塞, 等待此条件变量 */
        pthread_cond_t          queue_not_empty;         /* 当任务队列不为空的时候, 通知等待任务的线程 */

        pthread_t               *threads;                /* 存放线程池中的每个线程的id */
        pthread_t               adjust_tid;              /* 存放管理者线程, 进行线程的增加删除 */
        threadpool_task_t       *task_queue;             /* 任务队列 */

        int                     min_thr_num;             /* 线程池最大线程数 */
        int                     max_thr_num;             /* 线程池最小线程数 */
        int                     live_thr_num;            /* 当前存活线程数 */
        int                     busy_thr_num;            /* 忙碌的线程数 */
        int                     wait_exit_thr_num;       /* 要销毁的线程数目 */

        int                     queue_front;             /* task队列队头下标 */
        int                     queue_rear;              /* task队列队尾下标 */
        int                     queue_size;              /* task队列中的实际任务数 */
        int                     queue_max_size;          /* task队列中的最大任务数 */

        int                     shutdown;                /* 标志位线程池的使用状态 */
} threadpool_t;

/**
 * threadpool_create - create a threadpool
 * @min_thr_num: the min threads
 * @max_thr_num: the max threads
 * @queue_max_size: max task queue size
 */
threadpool_t *threadpool_create(int min_thr_num, int max_thr_num, int queue_max_size);

/**
 * threadpool_add_task - add task to the thread pool
 * @pool: the thread pool
 * @function:the callback function
 * @arg:the callback function parameter
 */
void threadpool_add_task(threadpool_t *pool, void *(*function)(), void *arg);
/**
 * threadpool_thread - thread callback function
 * @arg: incomming parameter
 */
void *threadpool_thread(void *arg);

/**
 * adjust_thread - adjust thread callback function
 * @arg: incomming parameter
 */
void *adjust_thread(void *arg);

void threadpool_destory(threadpool_t *pool);

void threadpool_free(threadpool_t *pool);

int is_thread_alive(pthread_t thrid);


#endif  // __THREADPOOL_H__