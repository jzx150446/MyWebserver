#include <mysql/mysql.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <list>
#include <pthread.h>
#include <iostream>
#include "sql_connection_pool.h"

connection_pool::connection_pool()
{
    m_CurrConn = 0;
    m_FreeConn = 0;
}

connection_pool::~connection_pool()
{
    DestoryPool();
}

void connection_pool::init(string url,string user,string password,string DBname,int MaxConn,int port,int close_log)
{
    m_Url = url;
    m_User = user;
    m_Port = port;
    m_PassWord = password;
    m_DataBaseName = DBname;
    m_close_log = close_log;

    for(int i = 0; i< MaxConn;++i){
        MYSQL *conn = NULL;
        conn = mysql_init(conn);

        if(conn == NULL){
            LOG_ERROR("MYSQL Error : mysql_init");
            exit(1);
        }
        //真正的连接函数
        conn = mysql_real_connect(conn,url.c_str(),user.c_str(),password.c_str(),DBname.c_str(),port,NULL,0);

        if(conn == NULL){
            LOG_ERROR("MYSQL error");
            exit(1);
        }
        connList.push_back(conn);
        ++m_FreeConn;
    }
    reserve = sem(m_FreeConn); //信号量记入共享资源数
    m_MaxConn = m_FreeConn;

}

MYSQL *connection_pool::GetConnection()
{
    MYSQL *conn = NULL;
    if(connList.size() == 0){
        return NULL;
    }
    reserve.wait();
    m_lock.lock();

    conn = connList.front();
    connList.pop_front();

    m_FreeConn--;
    m_CurrConn++;
    m_lock.unlock();

    return conn;

}

bool connection_pool::ReleaseConnection(MYSQL *conn)
{
    if(conn == nullptr){
        return false;
    }
    m_lock.lock();
    connList.push_back(conn);

    m_FreeConn++;
    m_CurrConn--;

    m_lock.unlock();
    reserve.post();
    return true;
}

int connection_pool::GetFreeConn()
{
    return m_FreeConn;
}

void connection_pool::DestoryPool()
{
    m_lock.lock();
    if(connList.size() >0){
        list<MYSQL *>::iterator ite;
        for(ite = connList.begin(); ite != connList.end();++ite){
            MYSQL *temp = *ite;
            mysql_close(temp);
        }
        m_FreeConn = 0;
        m_CurrConn = 0;
        connList.clear();
    }
    m_lock.unlock();

}

connectionRALL::connectionRALL(MYSQL **conn,connection_pool *connPool)
{
    *conn = connPool->GetConnection();
    conRALL = *conn;
    poolRALL = connPool;
}
connectionRALL::~connectionRALL()
{
    poolRALL->ReleaseConnection(conRALL);
}