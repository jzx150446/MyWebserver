#include "WebServer.h"

WebServer::WebServer()
{   
    /*new个http_conn对象*/
    users = new http_conn[MAX_FD];

    char server_path[200];
    getcwd(server_path,200);  //获取当前的 工作目录
    char root[6] = "/root";
    m_root = (char *)malloc(strlen(server_path)+strlen(root) +1 );
    strcpy(m_root, server_path);
    strcat(m_root, root);

    //初始化定时器
    users_timer = new client_data[MAX_FD];

}

WebServer::~WebServer()
{
    /*关闭epoll文件描述符，监听文件描述符，管道，
      delete http对象，定时器，线程池*/
      close(m_epollfd);
      close(m_listenfd);
      close(m_pipefd[1]);
      close(m_pipefd[0]);
      delete []users;
      delete []users_timer;
      delete m_pool;
}

void WebServer::init(int port,string user,string password,string databaseName,
              int log_write,int opt_linger,int trigmode,int sql_num,
              int thread_num,int close_log,int actor_model)
{
    m_port = port;
    m_user = user;
    m_password = password;
    m_databaseName = databaseName;
    m_log_write = log_write;
    m_OPT_LINGER = opt_linger;
    m_TRIGMode = trigmode;
    m_sql_num = sql_num;
    m_thread_num = thread_num;
    m_close_log = close_log;
    m_actormodel = actor_model;
}

void WebServer::trig_mode()
{
    //LT + LT
    cout<<"model : ";
    if(m_TRIGMode == 0){
        cout<<"LT+LT"<<endl;
        m_LISTENTrigmode = 0;
        m_CONNTrigmode = 0;
    }
    /*LT + ET*/
    else if (m_TRIGMode == 1) {
        cout<<"LT + ET"<<endl;
        m_LISTENTrigmode = 0;
        m_CONNTrigmode = 1;
    }
    /*ET + LT*/
    else if (m_TRIGMode == 2) {
        cout<<"ET + LT"<<endl;
        m_LISTENTrigmode = 1;
        m_CONNTrigmode = 0;
    }
    /*ET + ET*/
    else if (m_TRIGMode == 3) {
        cout<<"ET + ET"<<endl;
        m_LISTENTrigmode = 1;
        m_CONNTrigmode = 1;
    }
}

//m_close_log参数决定是否开启并初始化日志系统,m_log_write的参数值选择是异步写还是同步写
void WebServer::log_write()
{
    cout<<"log_write()";
    if(m_log_write == 0){
        cout<<"进入日志成功";
        /*初始化日志,m_log_write为1代表异步写*/
        if(m_log_write == 1)
        {
            cout<<"-日志异步写";
            Log::get_instance()->init("./ServerLog",m_close_log,2000,800000,800);
        }
        else
        {
            cout<<"-日志同步写";
            Log::get_instance()->init("./ServerLog",m_close_log,2000,800000,0);
        }
    }
    cout<<endl;
}

void WebServer::sql_pool()
{
    cout<<"sql_pool()--";
    m_connPool = connection_pool::get_instance();
    m_connPool->init("localhost",m_user,m_password,m_databaseName,3306,m_sql_num,m_close_log);
    //初始化数据库读取表
    users->initmysql_result(m_connPool);
}

void WebServer::thread_pool()
{
    cout<<"thread_pool()--";
    m_pool = new threadpool<http_conn>(m_actormodel,m_connPool,m_thread_num);
}


void WebServer::eventListen()
{
    m_listenfd = socket(PF_INET,SOCK_STREAM,0);
    assert(m_listenfd >= 0);

    //优雅关闭连接
    if(0 == m_OPT_LINGER)
    {
        struct linger tmp{0,1};
        setsockopt(m_listenfd,SOL_SOCKET,SO_LINGER,&tmp,sizeof(tmp));
    }else if (1 == m_OPT_LINGER){
        struct linger tmp = {1, 1};
        setsockopt(m_listenfd,SOL_SOCKET,SO_LINGER,&tmp,sizeof(tmp));
    }

    int ret = 0;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(m_port);

    int flag = 1;
    setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    ret = bind(m_listenfd, (struct sockaddr *)&address, sizeof(address));
    assert(ret >= 0);
    ret = listen(m_listenfd, 5);
    assert(ret >= 0);

    m_utils.init(TIMESLOT);

    //epoll创建内核事件表
    epoll_event events[MAX_EVENT_NUMBER];
    m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);

    m_utils.addfd(m_epollfd, m_listenfd, false, m_LISTENTrigmode);
    http_conn::m_epollfd = m_epollfd;

    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_pipefd);
    assert(ret != -1);
    m_utils.setnonblocking(m_pipefd[1]);
    m_utils.addfd(m_epollfd, m_pipefd[0], false, 0);

    m_utils.addsig(SIGPIPE, SIG_IGN);
    m_utils.addsig(SIGALRM, m_utils.sig_handler, false);
    m_utils.addsig(SIGTERM, m_utils.sig_handler, false);

    alarm(TIMESLOT);

    //工具类,信号和描述符基础操作
    utils::u_pipefd = m_pipefd;
    utils::u_epollfd = m_epollfd;
}

void WebServer::timer(int connfd, sockaddr_in client_address)
{
    users[connfd].init(connfd,client_address,m_root,m_CONNTrigmode,m_close_log,m_user,m_password,m_databaseName);

    //初始化client_data数据
    //创建定时器，设置回调函数和超时时间，绑定用户数据，将定时器添加到链表中
    users_timer[connfd].address = client_address;
    users_timer[connfd].sockfd = connfd;
    util_timer *timer= new util_timer;
    timer->user_date = &users_timer[connfd];
    timer->cb_fun = cb_func ;
    time_t cur = time(NULL);
    timer->expire = cur + 3*TIMESLOT;
    users_timer[connfd].timer = timer;
    m_utils.m_timer_lst.add_timer(timer);
}

//若有数据传输，则将定时器往后延迟3个单位
//并对新的定时器在链表上的位置进行调整
void WebServer::adjust_timer(util_timer *timer)
{
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;
    
    m_utils.m_timer_lst.add_timer(timer);
    LOG_INFO("%s", "adjust timer once");
}

void WebServer::deal_timer(util_timer *timer, int sockfd)
{
   timer->cb_fun(&users_timer[sockfd]);
   if(timer){
    m_utils.m_timer_lst.del_timer(timer);
   }
   LOG_INFO("close fd %d", users_timer[sockfd].sockfd);
}

bool WebServer::dealclientdata()
{
    struct sockaddr_in client_address;
    socklen_t client_addrlength = sizeof(client_address);

    if(0 == m_LISTENTrigmode)
    {
        int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addrlength);
        if (connfd < 0)
        {
            LOG_ERROR("%s:errno is:%d", "accept error", errno);
            return false;
        }
        if(http_conn::m_user_count >= MAX_FD)
        {
            m_utils.show_error(connfd, "Internal server busy");
            LOG_ERROR("%s", "Internal server busy");
            return false;
        }
        timer(connfd, client_address);
    }else{
        while(1)
        {
            int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addrlength);
            if (connfd < 0)
            {
                LOG_ERROR("%s:errno is:%d", "accept error", errno);
                break;
            }
            if (http_conn::m_user_count >= MAX_FD)
            {
                m_utils.show_error(connfd, "Internal server busy");
                LOG_ERROR("%s", "Internal server busy");
                break;
            }
            timer(connfd, client_address);
        }
        return false;
    }
    return true;

}

bool WebServer::dealwithsignal(bool &timeout,bool &stop_server)
{
    int ret = 0;
    int sig;
    char signals[1024];
    ret = recv(m_pipefd[0],signals,sizeof(signals),0);
    if (ret == -1)
    {
        return false;
    }
    else if (ret == 0)
    {
        return false;
    }
    else
    {
        for (int i = 0; i < ret; ++i)
        {
            switch (signals[i])
            {
            case SIGALRM:
            {
                timeout = true;
                break;
            }
            case SIGTERM:
            {
                stop_server = true;
                break;
            }
            }
        }
    }
    return true;
}

void WebServer::dealwithread(int sockfd)
{
    util_timer *timer = users_timer[sockfd].timer;

    //reactor
    if (1 == m_actormodel)
    {
        if (timer)
        {
            adjust_timer(timer);
        }

        //若监测到读事件，将该事件放入请求队列
        m_pool->append(users + sockfd, 0);

        while (true)
        {
            if (1 == users[sockfd].improv)
            {
                if (1 == users[sockfd].timer_flag)
                {
                    deal_timer(timer, sockfd);
                    users[sockfd].timer_flag = 0;
                }
                users[sockfd].improv = 0;
                break;
            }
        }
    }
    else
    {
        //proactor
        if (users[sockfd].read_once())
        {
            LOG_INFO("deal with the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));

            //若监测到读事件，将该事件放入请求队列
            m_pool->append_p(users + sockfd);

            if (timer)
            {
                adjust_timer(timer);
            }
        }
        else
        {
            deal_timer(timer, sockfd);
        }
    }
}

void WebServer::dealwithwrite(int sockfd)
{
    util_timer *timer = users_timer[sockfd].timer;
    //reactor
    if (1 == m_actormodel)
    {
        if (timer)
        {
            adjust_timer(timer);
        }

        m_pool->append(users + sockfd, 1);

        while (true)
        {
            if (1 == users[sockfd].improv)
            {
                if (1 == users[sockfd].timer_flag)
                {
                    deal_timer(timer, sockfd);
                    users[sockfd].timer_flag = 0;
                }
                users[sockfd].improv = 0;
                break;
            }
        }
    }
    else
    {
        //proactor
        if (users[sockfd].write())
        {
            LOG_INFO("send data to the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));

            if (timer)
            {
                adjust_timer(timer);
            }
        }
        else
        {
            deal_timer(timer, sockfd);
        }
    }
}

void WebServer::eventLoop()
{
    bool timeout = false;
    bool stop_server = false;

    while(!stop_server)
    {
        int number = epoll_wait(m_epollfd,events,MAX_EVENT_NUMBER,-1);
        if(number < 0 && errno != EINTR)
        {
            LOG_ERROR("%s", "epoll failure");
            break;
        }
        for(int i =0; i<number ;i++)
        {
            int sockfd = events[i].data.fd;
            //处理新到的客户连接
            if (sockfd == m_listenfd)
            {
                bool flag = dealclientdata();
                if (false == flag)
                    continue;
            }
            else if(events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                //服务器端关闭连接，移除对应的定时器
                util_timer *timer = users_timer[sockfd].timer;
                deal_timer(timer, sockfd);
            }
            else if((sockfd == m_pipefd[0]) && (events[i].events & EPOLLIN))  //处理信号
            {
                bool flag = dealwithsignal(timeout, stop_server);
                if (false == flag)
                    LOG_ERROR("%s", "dealclientdata failure");
            }
            //处理客户连接上接收到的数据
            else if(events[i].events & EPOLLIN)
            {
                dealwithread(sockfd);
            }
            else if (events[i].events & EPOLLOUT)
            {
                dealwithwrite(sockfd);
            }
        }
        if(timeout)
        {
            m_utils.timer_handler();

            LOG_INFO("%s", "timer tick");

            timeout = false;
        }
    }
}