#include "../timer/lst_timer.h"

sort_util_timer::sort_util_timer()
{
    head = NULL;
    tail = NULL;
}
sort_util_timer::~sort_util_timer()
{
    util_timer *tmp = head;
    while(tmp){
        head = head->next;
        delete tmp;
        tmp = head;
    }
}

void sort_util_timer::add_timer(util_timer *timer)
{
    if(!timer){
        return;
    }
    if(!head){
        head = tail = timer;
        return;
    }
    if(timer->expire <head->expire){
        timer->next = head;
        head->prev = timer;
        head = timer;
    }
    add_timer(timer,head);

}

void sort_util_timer::del_timer(util_timer *timer)
{
    if(!timer){
        return;
    }
    if(timer == head && timer == tail)
    {
        delete timer;
        head = NULL;
        tail = NULL;
    }
    if(timer == head){
        head = head->next;
        head->prev = NULL;
        delete timer;
        return;
    }
    if(timer == tail){
        tail = tail->prev;
        tail->next = NULL;
        delete timer;
        return;
    }
    timer->prev->next = timer->next;
    timer->next->prev = timer->prev;
    delete timer;
}

void sort_util_timer::adjust_timer(util_timer *timer)
{
    if(!timer){
        return;
    }
    util_timer *temp = timer->next;
    if(!temp || timer->expire < temp->expire)
    {
        return;
    }
    if(timer == head)
    {
        head = head->next;
        head->prev = NULL;
        timer->next = NULL;
        add_timer(timer,head);
    }else{
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        add_timer(timer,head);
    }
}
void sort_util_timer::add_timer(util_timer *timer,util_timer *head)
{
    util_timer *pre = head;
    util_timer *temp = head->next;
    while(temp){
        if(timer->expire < temp->expire){
            pre ->next = timer;
            timer->next = temp;
            temp->prev = timer;
            timer->prev = pre;
        }
        pre = temp;
        temp = temp->next;
    }
    if(!temp)
    {
        pre->next = timer;
        timer->prev = pre;
        timer->next = NULL;
        tail = timer;
    }
}

void sort_util_timer::tick()
{
    if(!head){
        return;
    }
    time_t cur_time = time(NULL);
    util_timer *temp = head;
    while(!temp)
    {
        if(cur_time < temp->expire)
        {
            break;
        }
        temp->cb_fun(temp->user_date);
        head = temp->next;
        while(head)
        {
            head->prev = NULL;
        }
        delete temp;
        temp = head;

    }
}
void utils::init(int timeslot)
{
    m_TIMESOLT = timeslot;
}

int utils::setnonblocking(int fd)
{
    int old_option = fcntl(fd,F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd,F_SETFL,new_option);
    return old_option;
}

void utils::addfd(int epollfd,int fd, bool one_shut,int TRIGNODE)
{
    epoll_event event;
    event.data.fd = fd;
    //开启边缘触发模式
    if(TRIGNODE == 1)
    {
        event.events = EPOLLIN |EPOLLET |EPOLLRDHUP;
    }else{
        event.events = EPOLLIN |EPOLLRDHUP;
    }
    if(one_shut){
        event.events  |=  EPOLLONESHOT;
    }
    epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);
    setnonblocking(fd);
}

void utils::sig_handler(int sig)
{
    int save_errno = errno;
    int msg = sig;
    send(u_pipefd[1],(char*)&msg,1,0);
    errno = save_errno;

}

void utils::addsig(int sig,void(handler)(int),bool restart = true)
{
    struct sigaction sa;
    memset(&sa,'\0',sizeof(sa));

    sa.sa_handler = handler;
    if(restart){
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig,&sa,NULL) != 0);
}

void utils::timer_handler()
{
    m_timer_lst.tick();
    alarm(m_TIMESOLT);
}
void utils::show_error(int connfd,const char *info)
{
    send(connfd,info,strlen(info),0);
    close(connfd);
}

int *utils::u_pipefd = 0;
int utils::u_epollfd = 0;

class utils;
void cb_fun(client_data *user_data)
{
    epoll_ctl(utils::u_epollfd,EPOLL_CTL_DEL,user_data->sockfd,0);
    assert(user_data);
    close(user_data->sockfd);
    http_conn::m_user_conn--;
}