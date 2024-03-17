#include "log.h"
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <sys/time.h>
#include <pthread.h>
using namespace std;
Log::Log()
{
    m_count = 0;
    m_is_async = false;
}
Log::~Log()
{
    if(m_fp != NULL){
        fclose(m_fp);
    }
}

//异步需要设置阻塞队列的长度，同步不需要设置
bool Log::init(const char *file_name, int close_log, int log_buf_size, int split_lines , int max_queue_size)
{
    if(max_queue_size >= 1){
        m_is_async = true;        //异步写入
        m_log_queue = new block_queue<string>(max_queue_size);
        pthread_t tid;            //创建一个线程，执行异步写入文件
        pthread_create(&tid,NULL,flush_log_thread,NULL);
        
    }
    //初始化各值
    m_close_log = close_log;
    m_split_lines = split_lines;
    m_log_buf_size = log_buf_size;
    m_buf = new char[m_log_buf_size];
    memset(m_buf,'\0',m_log_buf_size);

    //创建struct tm变量接受当下时间
    time_t t = time(NULL);
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;


    //在file_name中找到/，未找到返回nullptr，找到返回最后一个/的位置
    const char * p = strrchr(file_name,'/');
    char log_full_name[256] = {0} ;    //创建一个局部缓冲区对文件命名

    //下面时命名规则，日志文件命名为年_月_日_文件名
    if(p == nullptr){
        snprintf(log_full_name,255,"%d_%02d_%02d_%s",my_tm.tm_year + 1900,my_tm.tm_mon +1,my_tm.tm_mday,file_name);
    }else{
        strcpy(log_name,p+1);
        strncpy(dir_name,file_name,p-file_name+1);
        snprintf(log_full_name,255,"%s%d_%02d_%02d_%s",dir_name,my_tm.tm_year + 1900,my_tm.tm_mon +1,my_tm.tm_mday,log_name);
    }

    m_today = my_tm.tm_mday;
    m_fp = fopen(log_full_name,"a");
    if(m_fp == nullptr){
        return false;
    }
    return true;
}

void Log::write_log(int level, const char *format, ...)
{
    struct timeval now = {0,0};
    gettimeofday(&now,NULL);
    time_t t = now.tv_sec;
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;

    char s[16] = {0};
    switch(level)
    {
        case 0:
            strcpy(s,"[debug]:");
            break;
        case 1:
            strcpy(s,"[info]:");
            break;
        case 2:
            strcpy(s,"[warn]:");
            break;
        case 3:
            strcpy(s,"[erro]:");
            break;
        default:
            strcpy(s,"[info]:");
            break;
    }
    //开始写入
    m_mutex.lock();
    m_count++;
    //如果时新的一天或日志行数达到上限则创建新日志
    if(m_today != my_tm.tm_mday || m_count % m_split_lines == 0){
        char new_log[256] = {0};
        flush();
        fclose(m_fp);
        char tail[16] = {0};
        snprintf(tail,16,"%d_%02d_%02d",my_tm.tm_year + 1900,my_tm.tm_mon +1,my_tm.tm_mday);

        if(m_today != my_tm.tm_mday){
            snprintf(new_log,255,"%s%s%s",dir_name,tail,log_name);
            m_today = my_tm.tm_mday;
            m_count = 0;
        }else{
            snprintf(new_log,255,"%s%s%s.%lld",dir_name, tail, log_name, m_count / m_split_lines);

        }
        m_fp = fopen(new_log,"a");
    }
    m_mutex.unlock();

    //可变参数定义初始化，在vsprintf作用，输入具体的日志内容
    va_list valst;
    va_start(valst,format);

    string log_str;
    m_mutex.lock();
    //每一行的开头格式
    int n = snprintf(m_buf,48,"%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
                    my_tm.tm_year+1900,my_tm.tm_mon+1,my_tm.tm_mday,
                    my_tm.tm_hour,my_tm.tm_min,my_tm.tm_sec,now.tv_usec,s);
    int m = vsnprintf(m_buf+n,m_log_buf_size -n -1,format,valst);

    //加入空格和换行
    m_buf[ n +m]  = '\n';
    m_buf[n +m + 1] =  '\0';
    log_str = m_buf;
    m_mutex.unlock();

    //决定时异步还是同步
    if(m_is_async && !m_log_queue->full()){
        m_log_queue->push(log_str);
    }else{
        m_mutex.lock();
        fputs(log_str.c_str(),m_fp);
        m_mutex.unlock();
    }
    va_end(valst);


}
void Log::flush(void){
    m_mutex.lock();
    fflush(m_fp);
    m_mutex.unlock();
}