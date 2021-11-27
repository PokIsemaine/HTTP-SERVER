#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H

#include <sys/epoll.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <cerrno>
#include <sys/uio.h>
#include <cstring>
#include "locker.h"


class http_conn{
public:

    static int m_epollfd; //所有的socket上的事件都被注册到同一个epoll对象上.
    static int m_user_count; //统计用户的数量

    static const int READ_BUFFER_SIZE = 4048;
    static const int WRITE_BUFFER_SIZE = 4048;
    static const int FILENAME_LEN = 200;

    //HTTP请求方法，但我们只支持GET
    enum METHOD {GET = 0,POST,HEAD,PUT,DELETE,TRACE,OPTIONS,CONNECT};

    /* 解析客户端请求时主状态机状态
     * CHECK_STATE_REQUEST_LINE：当前正在分析请求行
     * CHECK_STATE_HEADER：当前正在分析头部字段
     * CHECK_STATE_CONTENT：当前正在分析请求体
    */
    enum CHECK_STATE{CHECK_STATE_REQUEST_LINE = 0,CHECK_STATE_HEADER,CHECK_STATE_CONTENT};

    /*
     * 从状态机的三种状态，即每行的读取状态
     * LINE_OK      读取到一个完整的行
     * LINE_BAD     行出错
     * LINE_OPEN    行数据不完整
     */
    enum LINE_STATUS{LINE_OK=0,LINE_BAD,LINE_OPEN};

    /*
     * 服务器处理HTTP请求的可能结果，报文解析结果
     * NO_REQUEST           请求不完整，需要继续读取客户端数据
     * GET_REQUEST          表示获得了一个完整的客户请求
     * BAD_REQUEST          表示客户请求语法错误
     * NO_RESOURCE          表示服务器没有资源
     * FORBIDDEN_REQUEST    表示客户对资源没有足够的访问权限
     * FILE_REQUEST         文件请求，获取文件成功
     * INTERNAL_ERROR       表示服务器内部数据
     * CLOSED_CONNECTION    表示客户端已经断开连接了
     */
    enum HTTP_CODE{NO_REQUEST = 0,GET_REQUEST,BAD_REQUEST,NO_RESOURCE,FORBIDDEN_REQUEST,FILE_REQUEST,INTERNAL_ERROR,CLOSED_CONNECTION};



    http_conn(){};
    ~http_conn(){};

    void process(); //处理客户端的请求
    void init(int sockfd,const sockaddr_in &addr);//初始化新接收的连接
    void close_conn(); //关闭连接
    bool read(); //非阻塞的读
    bool write(); //非阻塞的写



private:
    int m_sockfd;                                   //该HTTP连接的socket
    sockaddr_in m_address;                          //通信的socket地址

    char m_read_buf[READ_BUFFER_SIZE];              //读缓冲区
    int m_read_idx;                                 //标识读缓冲区中以及读入的客户端数据的最后一个字节的下一个位置
    int m_checked_index;                            //当前正在分析的字符在读缓冲区的位置
    int m_start_line;                               //当前正在解析行的起始位置

    char m_write_buf[WRITE_BUFFER_SIZE];            //写缓冲区
    int m_write_idx;                                //写缓冲区中待发送的字节数
    struct stat m_file_stat;    //目标文件的状态，可以用来看文件是否存在，是否可读，是否有访问权限，是否为目录，以及文件大小等相关信息
    char* m_file_address;   //客户请求的目标文件被mmap到内存中的起始位置
    struct iovec m_iv[2];                           // 我们将采用writev来执行写操作，所以定义下面两个成员，其中m_iv_count表示被写内存块的数量。
    int m_iv_count;                                 //被写的内存块的数量，m_write_buf + m_file_address

    char * m_url;   //请求目标文件名
    char * m_version;    //协议版本只支持HTTP1.1
    METHOD m_method;    //请求方法
    char * m_host;      //主机名
    bool m_linger;      //HTTP请求是否要保持连接
    int m_content_length;   //HTTP请求的消息总长度
    char m_real_file[FILENAME_LEN]; //客户请求目标文件的完整路径 doc_root + m_url


    CHECK_STATE m_check_state;                      //主状态机当前所处的状态

    void init();                                    //初始化连接其余的数据
    HTTP_CODE process_read();                       //解析HTTP请求
    HTTP_CODE parse_request_line(char * text);      //解析HTTP请求首行
    HTTP_CODE parse_request_header(char * text);    //解析HTTP请求头
    HTTP_CODE parse_request_content(char * text);   //解析HTTP请求体

    LINE_STATUS parse_line();                        //解析行

    char * get_line(){return m_read_buf+m_start_line;}
    HTTP_CODE do_request();     //具体处理
    void unmap();   //对内存映射区进行munmap操作

    bool process_write(HTTP_CODE ret);
    bool add_response(const char* format,...);  //往写缓冲区中写入待发送的数据
    bool add_content(const char* content);
    bool add_content_type();
    bool add_status_line(int status,const char* titile);
    bool add_headers(int content_length);
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();


};

#endif
