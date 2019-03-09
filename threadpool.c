#include "threadpool.h"

threadpool_t *threadpool_create(int min_thr_num, int max_thr_num, int queue_max_size)
{
        threadpool_t *pool = NULL;
        int i = 0;
        do {
                if ((pool = (threadpool_t *)malloc(sizeof(threadpool_t))) == NULL) {
                        fprintf(stderr, "malloc threadpool error\n");
                        break;
                }

                pool->min_thr_num = min_thr_num;
                pool->max_thr_num = max_thr_num;
                pool->busy_thr_num = 0;
                pool->live_thr_num = min_thr_num;
                pool->queue_size = 0;
                pool->queue_max_size = queue_max_size;
                pool->queue_front = 0;
                pool->queue_rear = 0;
                pool->shutdown = false;

                /* 给工作线程数组开辟空间 */
                pool->threads = (pthread_t *)malloc(sizeof(pthread_t) * max_thr_num);
                if (pool->threads == NULL) {
                        fprintf(stderr, "malloc threads error\n");
                        break;
                }
                memset(pool->threads, 0, sizeof(pthread_t) * max_thr_num);

                /* 开辟任务队列空间 */
                pool->task_queue = (threadpool_task_t *)malloc(sizeof(threadpool_task_t) * 
                        queue_max_size);
                        
                if (pool->task_queue == NULL) {
                        fprintf(stderr, "malloc task_queue error\n");
                        break;
                }

                /* 初始化信号量 条件变量 */
                if (pthread_mutex_init(&pool->lock, NULL) != 0 
                        || pthread_mutex_init(&pool->thread_counter, NULL) != 0 
                        || pthread_cond_init(&pool->queue_not_empty, NULL) != 0 
                        || pthread_cond_init(&pool->queue_not_full, NULL) != 0) {
                        fprintf(stderr, "init mutex or cond failed\n");
                        break;
                }

                /* 创建工作线程 */
                for (i = 0; i < min_thr_num; ++i) { 
                        pthread_create(&(pool->threads[i]), NULL, threadpool_thread, (void *)pool);
                        printf("start thread 0x%x\n", (unsigned int)pthread_self());
                       
                }
                /* 创建管理者线程 */
                pthread_create(&pool->adjust_tid, NULL, adjust_thread, (void *)pool);

                return pool;

        } while (0);

        threadpool_destory(pool);

        return NULL;
}

void *threadpool_thread(void *arg)
{
        threadpool_t *pool = (threadpool_t *)arg;
        threadpool_task_t task;

        while (true) {
                pthread_mutex_lock(&(pool->lock));

                /* 如果没有任务, 则阻塞在条件变量queue_not_empty上, 等待添加任务 */
                while ((pool->queue_size == 0) && (!(pool->shutdown))) {
                        printf("thread 0x%x is wating for task\n", (unsigned int)pthread_self());
                        pthread_cond_wait(&(pool->queue_not_empty), &(pool->lock));

                        /* 清楚指定数目的空闲线程 */
                        if (pool->wait_exit_thr_num > 0) {
                                pool->wait_exit_thr_num--;

                                if (pool->live_thr_num > pool->min_thr_num) {
                                        printf("thread 0x%x is exting\n", (unsigned int)pthread_self());
                                        pool->live_thr_num--;
                                        /* 释放锁 */
                                        pthread_mutex_unlock(&(pool->lock));
                                        pthread_exit(NULL);
                                }
                        }
                }
                /* 线程池关闭, 各个线程自己结束 */
                if (pool->shutdown) {
                        printf("thread 0x%x is exting\n", (unsigned int)pthread_self());
                        pthread_mutex_unlock(&(pool->lock));
                        pthread_exit(NULL);
                }
                /* 提取任务 */
                task.function = pool->task_queue[pool->queue_front].function;
                task.arg = pool->task_queue[pool->queue_front].arg;
                /* 出队列 */
                pool->queue_front = (pool->queue_front + 1) % pool->queue_max_size;
                pool->queue_size--;
                /* 通知可以有新的任务添加进来 */
                pthread_cond_broadcast(&(pool->queue_not_full));

                /* 释放pool->lock锁 */
                pthread_mutex_unlock(&(pool->lock));

                /* 执行任务 */
                pthread_mutex_lock(&(pool->thread_counter));
                pool->busy_thr_num++;
                pthread_mutex_unlock(&(pool->thread_counter));
                (*(task.function))(task.arg);
                /* 任务执行完成 */
                printf("thread 0x%x end working\n", (unsigned int)pthread_self());
                pthread_mutex_lock(&(pool->thread_counter));
                pool->busy_thr_num--;
                pthread_mutex_unlock(&(pool->thread_counter));
        }
        pthread_exit(NULL);

}

void *adjust_thread(void *arg)
{
        threadpool_t *pool = (threadpool_t *)arg;
        int i = 0;
        while(!pool->shutdown) {
                sleep(DEFAULT_WAIT_TIME);
                /* 获取任务数， 存活线程数 */
                pthread_mutex_lock(&(pool->lock));
                int queue_size = pool->queue_size;
                int live_thr_num = pool->live_thr_num;
                pthread_mutex_unlock(&(pool->lock));

                /* 获取忙着的线程数 */
                pthread_mutex_lock(&(pool->thread_counter));
                int busy_thr_num = pool->busy_thr_num;
                pthread_mutex_unlock(&(pool->thread_counter));

                /* 创建线程 */
                if(queue_size >= MIN_WAIT_TASK_NUM && 
                        live_thr_num < pool->max_thr_num) {
                        pthread_mutex_lock(&(pool->lock));
                        int add = 0;

                        /* 一次增加default_thread_vary个线程 */
                        for(i = 0; i < pool->max_thr_num && add < DEFAULT_THREAD_VARY && 
                                pool->live_thr_num < pool->max_thr_num; ++i) {
                                if(pool->threads[i] == 0 || !is_thread_alive(pool->threads[i])) {
                                        pthread_create(&pool->threads[i], NULL, threadpool_thread, (void *)pool);
                                        add++;
                                        pool->live_thr_num++;
                                }
                        }

                        printf("add %d threads\n", DEFAULT_THREAD_VARY);

                        pthread_mutex_unlock(&(pool->lock));
                }

                /* 销毁线程 */
                if((busy_thr_num * 2) < live_thr_num && live_thr_num > pool->min_thr_num) {
                        /* 一次销毁DEFAULT_THREAD_VARY个线程 */
                        pthread_mutex_lock(&(pool->thread_counter));
                        pool->wait_exit_thr_num = DEFAULT_THREAD_VARY;
                        printf("delete %d threads\n", DEFAULT_THREAD_VARY);
                        pthread_mutex_unlock(&(pool->thread_counter));

                        for(i = 0; i < DEFAULT_THREAD_VARY; ++i) {
                                pthread_cond_signal(&(pool->queue_not_empty));
                        }
                }

        }

        return NULL;
}

void threadpool_destory(threadpool_t *pool)
{
        if (pool == NULL) 
                return;
        int i = 0;

        pool->shutdown = true;

        /* 销毁管理者线程 */
        pthread_join(pool->adjust_tid, NULL);

        /* 通知所有存活线程， 自杀 */
        for (i = 0; i < pool->live_thr_num; ++i) {
                pthread_cond_broadcast(&(pool->queue_not_empty));
        }
        /* 回收所有自杀的线程 */
        for (i = 0; i < pool->live_thr_num; ++i) {
                pthread_join(pool->threads[i], NULL);
        }
        threadpool_free(pool);
}

void threadpool_free(threadpool_t *pool) 
{
        if (pool == NULL)
                return ;
        
        if(pool->threads) {
                free(pool->threads);
                pool->threads = NULL;
        }

        if(pool->task_queue) {
                free(pool->task_queue);
                pool->task_queue = NULL;
                pthread_mutex_lock(&(pool->lock));
                pthread_mutex_destroy(&(pool->lock));
                pthread_mutex_lock(&(pool->thread_counter));
                pthread_mutex_destroy(&(pool->thread_counter));
                pthread_cond_destroy(&(pool->queue_not_empty));
                pthread_cond_destroy(&(pool->queue_not_full));
        }

        free(pool);
}

void threadpool_add_task(threadpool_t *pool, void *(*function)(), void *arg)
{
        /* 获得锁 pool->lock */
        pthread_mutex_lock(&(pool->lock));
        
        /* 队列未满 阻塞在queue_not_full条件变量上 */
        while((pool->queue_max_size == pool->queue_size) && (!pool->shutdown)) {
                pthread_cond_wait(&pool->queue_not_full, &pool->lock);
        }

        if(pool->shutdown) {
                pthread_mutex_unlock(&(pool->lock));
        }

        /* 清空参数arg的空间 */
        if(pool->task_queue[pool->queue_rear].arg != NULL) {
                free(pool->task_queue[pool->queue_rear].arg);
                pool->task_queue[pool->queue_rear].arg = NULL;
        }

        /* 任务队列中添加任务, 入队操作 */
        pool->task_queue[pool->queue_rear].function = function;
        pool->task_queue[pool->queue_rear].arg = function;
        pool->queue_rear = (pool->queue_rear + 1) % pool->queue_max_size;
        pool->queue_size++;

        /* 唤醒等待任务的线程 */
        pthread_cond_signal(&(pool->queue_not_empty));
        /* 释放锁 */
        pthread_mutex_unlock(&(pool->lock));
}

int is_thread_alive(pthread_t thrid)
{
        /*pthread_kill的返回值：成功（0） 线程不存在（ESRCH） 信号不合法（EINVAL）*/
        int kill_rc = pthread_kill(thrid, 0);
        if(kill_rc == ESRCH) {
                return false;
        }
        return true;
}