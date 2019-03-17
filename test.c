#include "threadpool.h"

void *process(void *arg)
{
        sleep(5);
        return NULL;
}
int main(int argc, char **argv)
{
        threadpool_t *pool = threadpool_create(3, 100, 100);

        int num[200] = { 0 };
        for(int i = 0; i < 20; ++i) {
                num[i] = i;
                threadpool_add_task(pool, process, (void *)i);
        }
        
        getchar();
        threadpool_destory(pool);
        return 0;
}