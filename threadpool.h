/*  
 *  Description : 
 *  Created by 旋风冲锋龙卷风 on 2023/02/22 13:56
 *  个人博客 : http://letsgofun.cn/
 */
//

#ifndef WEBSERVER_THREADPOOL_H
#define WEBSERVER_THREADPOOL_H


#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "locker.h"



//线程池类, 定义成模版类是为了代码的复用
template<typename T>
class threadpool
{
public:

    threadpool(int thread_number = 8, int max_request = 10000);
    ~threadpool();
    bool append(T * request);

private:
    //在C++中必须是一个静态函数,因为成员函数会传入this指针
    // 而worker只有一个参数
    //这是pthread_create的要求
    static void* worker(void* arg);
    void run();
private:
    //线程的数量
    int m_thread_number;

    //线程池数组, 大小为m_thread_number
    pthread_t* m_threads;

    //请求队列中最多允许的等待处理的请求数量
    int m_max_requests;

    //请求队列
    std::list<T*> m_workqueue;

    // 互斥锁
    locker m_queuelocker;

    //信号量,用来判断是否有任务需要处理
    sem m_queuestat;

    //是否结束线程
    bool m_stop;

};

template<typename T>
threadpool<T>::threadpool(int thread_number, int max_request) :
    m_thread_number(thread_number), m_max_requests(max_request), m_stop(false), m_threads(nullptr)
{
    if((thread_number <= 0) || (max_request <= 0))
    {
        throw std::exception();
    }

    m_threads = new pthread_t[m_thread_number];
    if(!m_threads)
    {
        throw std::exception();
    }

    //创建thread_number个线程,并设置为线程脱离

    for(int i = 0; i < thread_number; ++i)
    {
        printf("正在创建第%d个线程\n", i);
        if (pthread_create(m_threads + i, nullptr, worker, this)  != 0)
        {
            delete[] m_threads;
            throw std::exception();
        }

        //如果分离出错
        if (pthread_detach(m_threads[i]))
        {
            delete[] m_threads;
            throw std::exception();
        }
    }
}
template<typename T>
threadpool<T>::~threadpool()
{
    delete[] m_threads;
    m_stop = true;
}

template<typename T>
bool threadpool<T>::append(T *request)
{
    m_queuelocker.lock();
    if(m_workqueue.size() > m_max_requests)
    {
        //解锁
        m_queuelocker.unlock();
        return false;
    }

    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;

}

template<typename T>
void* threadpool<T>::worker(void *arg)
{
    threadpool* pool = (threadpool*) arg;
    pool->run();
    return pool;
}

template<typename T>
void threadpool<T>::run()
{
    while(!m_stop)
    {
        m_queuestat.wait();
        m_queuelocker.lock();
        if(m_workqueue.empty())
        {
            m_queuelocker.unlock();
            continue;
        }

        T* request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();

        if(!request)
        {
            continue;
        }

        request->process();
    }
}
#endif //WEBSERVER_THREADPOOL_H
