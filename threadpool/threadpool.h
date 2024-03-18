#ifndef THREADPOOL_H
#define THREADPOOL_H


#include<list>
#include<cstdio>
#include<pthread.h>
#include<exception>
#include "../lock/locker.h"
#include "../GGImysql/sql_connection_pool.h"

template<class T>
class threadpool{
public:
    //thraed_num是线程池中线程数，max_requests是请求队列中最大请求数
    threadpool(int actor_modal,connection_pool *connPool,int thread_num = 8,int max_requests = 10000);
    ~threadpool();
    //一添加任务
    bool append(T *request,int state);
    bool append_p(T *request);

private:
    //工作线程运行的函数
    static void *worker(void *argc);
    void run();

private:
    int m_thread_num;
    pthread_t *m_threads;
    int m_max_requests;
    std::list<T*>m_worker;  //请求队列
    locker m_queuelocker;
    sem m_queuestaat;   //是否有任务处理
    connection_pool *m_conn_pool;
    int m_actor_modal;      //模式切换


};


template<class T>
threadpool<T>::threadpool(int actor_modal,connection_pool *connPool,int thread_num = 8,int max_requests = 10000)
{
    cout<<"线程池初始化开始";
    if(thread_num <= 0||max_requests <=0)
    {
        throw std::exception();
    }
    m_threads = new pthread_t[thread_num];
    if(!m_threads)
    {
        throw std::exception();
    }
    for(int i = 0; i<thread_num;++i)
    {
        if(pthread_create(m_threads +i,NULL,worker,this) != 0)
        {
            delete[] m_threads;
            throw std::exception();
        }
        if(pthread_detach(m_threads[i])){
            delete[] m_threads;
            throw std::exception();
        }
    }
    cout<<"线程池初始化完成";

}

template<class T>
threadpool<T>::~threadpool()
{
    delete [] m_threads;

}

template<class T>
bool threadpool<T>::append(T *request,int state)
{
    m_queuelocker.lock();
    if(m_worker.size() >= m_max_requests)
    {
        m_queuelocker.unlock();
        return false;
    }
    /*在本项目中，request通常是http数据类型，而m_state则表示该request是读事件还是写事件*/
    request->m_state = state;
    m_worker.push_back(request);
    m_queuelocker.unlock();
    m_queuestaat.post();
    return true;
}
template<class T>
bool threadpool<T>::append_p(T *request)
{
    m_queuelocker.lock();
    if (m_workqueue.size() >= m_max_requests) {
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}
template<typename T>
void *threadpool<T>::worker(void *argc)
{
    threadpool *pool = (threadpool *)argc;
    pool->run();
    return pool;
}

template<typename T>
void threadpool<T>::run()
{
    while(true)
    {
        m_queuestaat.wait();
        m_queuelocker.lock();
        if (m_workqueue.empty()) 
        {
            m_queuelocker.unlock();
            continue;
        }
        T *request = m_worker.front();
        m_worker.pop_front();

        m_queuelocker.unlock();
        if (!request) {
            continue;
        }
        if( 1==m_actor_modal){

            if(0 == request->m_state){
                if (request->read_once())
                {
                    request->improv = 1;
                    connectionRAII mysqlcon(&request->mysql, m_connPool);
                    request->process();
                }
                else
                {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }else{
                if (request->write())
                {
                    request->improv = 1;
                }
                else
                {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
        }else{
            connectionRAII mysqlcon(&request->mysql, m_connPool);
            request->process();
        }

    }
}
#endif