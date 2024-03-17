#ifndef LST_TIMER_H
#define LST_TIMER_H

#include<unistd.h>
#include<signal.h>
#include<sys/epoll.h>
#include<sys/types.h>
#include<fcntl.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<assert.h>
#include<sys/stat.h>
#include<string.h>
#include<pthread.h>
#include<stdio.h>
#include<sys/mman.h>
#include<stdlib.h>
#include<stdarg.h>
#include<errno.h>
#include<sys/wait.h>
#include<sys/uio.h>

#include<time.h>
#include "../log/log.h"

class util_timer;


//用户数据结构体
struct client_data{
    sockaddr_in address;
    int sockfd;
    util_timer *timer;
};

class util_timer{
public:
    util_timer():prev(NULL),next(NULL){}
public:

    util_timer *prev;
    util_timer *next;

    time_t expire;          //绝对时间
    void (*cb_fun)(client_data *);   //回调函数
    client_data *user_date;       //连接资源
};

class sort_util_timer{
public:
    sort_util_timer();
    ~sort_util_timer();

    void add_timer(util_timer *timer);           //增加定时器
    void del_timer(util_timer *timer);           //删除定时器
    void adjust_timer(util_timer *timer);        //调整定时器
    void tick();                //定时任务处理函数



private:
    void add_timer(util_timer *timer,util_timer* lst_head);
    util_timer *head;
    util_timer *tail;
};

class utils{
public:
    utils(){}
    ~utils(){}
    void init(int timeslot);

    int setnonblocking(int fd);                                         //对文件描述符设置非阻塞模式
    void addfd(int epollfd,int fd, bool one_shut,int TRIGNODE);         //将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
    static void sig_handler(int sig);                                   //信号处理函数
    void addsig(int sig,void(handler)(int),bool restart = true);        //设置信号处理函数   这里第二个参数void(handler)(int)等价于void(*handler)(int),再作函数参数时，后者的*可以省略
    void timer_handler();                                               //定时处理任务， 重新定时以不触发SIGALRM信号*/
    void show_error(int connfd,const char *info);


public:
    static int *u_pipefd;
    sort_util_timer m_timer_lst;
    static int u_epollfd;
    int m_TIMESOLT;
};





#endif