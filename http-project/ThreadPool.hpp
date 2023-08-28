#pragma once 

#include <iostream>
#include <queue>
#include <pthread.h>
#include "Log.hpp"
#include "Task.hpp"

#define NUM 6

class ThreadPool
{
private:
    int num;
    bool stop;
    std::queue<Task> task_queue;
    static ThreadPool *single_instance;
    pthread_mutex_t lock;
    pthread_cond_t cond;

    ThreadPool(int _num = NUM):num(_num), stop(false)
    {
        pthread_mutex_init(&lock, nullptr);
        pthread_cond_init(&cond, nullptr);
    }

    ThreadPool(const ThreadPool &tp) {}

public:
    static ThreadPool* getinstance()
    {
        static pthread_mutex_t _mutex = PTHREAD_MUTEX_INITIALIZER;
        if(nullptr == single_instance)
        {
            pthread_mutex_lock(&_mutex);
            if(nullptr == single_instance)
            {
                single_instance = new ThreadPool();
                single_instance->InitThreadPool();
            }
            pthread_mutex_unlock(&_mutex);
        }

        return single_instance;
    }

    bool TaskQueueIsEmpty()
    {
        return (task_queue.size() == 0) ? true : false;
    }
    void Lock()
    {
        pthread_mutex_lock(&lock);
    }
    void Unlock()
    {
        pthread_mutex_unlock(&lock);
    }
    void ThreadWait()
    {
        pthread_cond_wait(&cond, &lock); //释放锁, 挂起
    }
    void ThreadWakeup()
    {
        pthread_cond_signal(&cond); // 争抢锁, 唤醒
    }

    static void *ThreadRoutine(void *args)
    {
        ThreadPool *tp = (ThreadPool*)args;

        while(true)
        {
            Task t;
            tp->Lock();
            while(tp->TaskQueueIsEmpty()) // while 防止假判定
            {
                tp->ThreadWait();
            }
            tp->PopTask(t);
            tp->Unlock();
            t.ProcessOn();
        }
    }

    bool InitThreadPool()
    {
        pthread_t tid;
        for(int i = 0; i < NUM; i++)
        {
            if(pthread_create(&tid, nullptr, ThreadRoutine, this) != 0)
            {
                LOG(FATAL, "create thread pool error!");
                return false;
            }
        }
        LOG(INFO, "create pthread pool success");
        return true;
    }

    void PushTask(const Task &task)
    {
        Lock();
        task_queue.push(task);
        Unlock();
        ThreadWakeup();
    }
    void PopTask(Task &task)
    {
        task = task_queue.front();
        task_queue.pop();
    }

    ~ThreadPool()
    {
        pthread_mutex_destroy(&lock);
        pthread_cond_destroy(&cond);
    }
};
ThreadPool* ThreadPool::single_instance = nullptr;