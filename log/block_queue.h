/*
循环数组实现阻塞队列，m_back = （back + 1）%m_max_size
线程安全。每个操作前要加锁，操作完在解锁
*/
#ifndef BLOCK_QUEUE_H
#define BLOCK_QUEUE_H

#include<iostream>
#include<stdio.h>
#include<pthread.h>
#include<sys/time.h>
#include "../lock/locker.h"

using namespace std;

template<class T>
class block_queue{
public:
    //初始化阻塞队列
    block_queue(int max_size = 1000){
        if(max_size <=0){
            exit(-1);
        }
        m_max_size = max_size;
        m_array = new T[max_size];
        m_size =0;
        m_front = -1;
        m_back = -1;
    }
    //删除new出来的数组
    ~block_queue(){
        m_mutex.lock();
        if(m_array != NULL){
            delete []m_array;
        }
        m_mutex.unlock();
    }
    //清空队列
    void clear(){
        m_mutex.lock();
        m_size = 0;
        m_front = -1;
        m_back = -1;
        m_mutex.unlock();
    }
    //判断队列满
    bool full(){
        m_mutex.lock();
        if(m_size >= m_max_size){
            m_mutex.unlock();
            return true;
        }
        m_mutex.unlock();
        return false;
    }
    //判断队列为空
    bool empty(){
        m_mutex.lock();
        if(m_size == 0){
            m_mutex.unlock();
            return true;
        }
        m_mutex.unlock();
        return false;
    }
    //获得队首元素
    bool front(T& value){
        m_mutex.lock();
        if(m_size == 0){
            m_mutex.unlock();
            return false;
        }
        value = m_array[(m_front+1) % m_max_size];
        m_mutex.unlock();
        return true;
    }
    //获得队尾元素
    bool back(T& value){
        m_mutex.lock();
        if(m_size == 0){
            m_mutex.unlock();
            return false;
        }
        value = m_array[m_back];
        m_mutex.unlock();
        return true;
    }

    int size(){
        int temp = 0;
        m_mutex.lock();
        temp = m_size;
        m_mutex.unlock();
        return temp;
    }

    int max_size(){
        int temp = 0;
        m_mutex.lock();
        temp = m_max_size;
        m_mutex.unlock();
        return temp;
    }

    //往队列添加元素前需要先将所有使用队列的线程唤醒
    //阻塞队列封装了生产者消费者模型，调用push的是生成者，也就是工作线程
    bool push(T& item){
        m_mutex.lock();
        if(m_size >= m_max_size){
            m_cond.broadcast();
            m_mutex.unlock();
            return false;
        }
        m_back = (m_back +1) % m_max_size;
        m_array[m_back] = item;
        m_size++;

        m_cond.broadcast();
        m_mutex.unlock();
        return true;

    }
    //pop是消费者，负责将生产者的内容写入文件
    //pop时，如果队列没有元素，则会等待条件变量
    bool pop(T& item){
        m_mutex.lock();
        while(m_size <=0){
            if(!m_cond.wait(m_mutex.get())){
                m_mutex.unlock();
                return false;
            }
        }
        m_front = (m_front +1 )% m_max_size;
        item =  m_array[m_front] ;
        m_size--;

        m_mutex.unlock();
        return true;
    }

    //增加超时判断
    bool pop(T& item,int ms_timeout){
        struct timespec t = {0,0};
        struct timeval now = {0,0};
        gettimeofday(&now,nullptr);
        m_mutex.lock();
        if(m_size <= 0){
            t.tv_sec = now.tv_sec + ms_timeout/1000;
            t.tv_nsec = (ms_timeout % 1000) *1000;
            if(!m_cond.timewait(m_mutex.get(),t)){
                m_mutex.unlock();
                return false;
            }
        }
        if(m_size <=0){
            m_mutex.unlock();
            return false;
        }
        m_front = (m_front +1 )% m_max_size;
        item =  m_array[m_front] ;
        m_size--;

        m_mutex.unlock();
        return true;
    }
private:
    locker m_mutex;
    cond m_cond;

    T *m_array;
    int m_size;
    int m_max_size;
    int m_front;
    int m_back;
};

#endif