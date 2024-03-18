#ifndef _CONNECTION_POOL_
#define _CONNECTION_POOL_


#include<stdio.h>
#include<list>
#include<iostream>
#include<error.h>
#include<string>
#include<string.h>
#include<mysql/mysql.h>
#include "../lock/locker.h"
#include "../log/log.h"

using namespace std;

class connection_pool{
public:
    //单例模式
    static connection_pool *get_instance(){
        static connection_pool instance;
        return &instance;
    }

    MYSQL *GetConnection();                 //获取数据库连接
    bool ReleaseConnection(MYSQL *conn);    //释放连接
    int GetFreeConn();                      //获取空闲连接数
    void DestoryPool();                     //销毁所有连接 

    //初始化数据库连接库
    void init(string url,string user,string password,string DBname,int port,int MaxConn,int close_log);
private:
    connection_pool();
    ~connection_pool();
private:
    int m_MaxConn;          //最大连接数
    int m_FreeConn;         //可用连接数
    int m_CurrConn;         //已用连接数
    list<MYSQL *>connList;   //连接池

    locker m_lock;
    sem reserve;          //信号量记录可用资源

public:
    string m_Url;           //主机地址
    string m_Port;          //数据库端口号
    string m_User;          //数据库用户名
    string m_PassWord;      //数据库密码
    string m_DataBaseName;  //数据库名
    int m_close_log;        //是否开启日志
};

class connectionRALL{
public:
    connectionRALL(MYSQL **conn,connection_pool *connPool);
    ~connectionRALL();
private:
    MYSQL *conRALL;
    connection_pool *poolRALL;

};


#endif
