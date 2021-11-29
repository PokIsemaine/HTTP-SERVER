//code by zsl
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cerrno>
#include <fcntl.h>
#include <sys/epoll.h>
#include <csignal>
#include "locker.h"
#include "threadpool.h"
#include "http_conn.h"

#define MAX_FD 65535 //最大的文件描述符个数
#define MAX_EVENT_NUMBER 10000 //同时最大监听事件数量

int http_conn::m_epollfd = -1; //所有的socket上的事件都被注册到同一个epoll对象上
int http_conn::m_user_count = 0; //统计用户的数量
//添加信号捕捉
void addsig(int sig,void(handler)(int)){
    struct sigaction sa;
    memset(&sa,'\0',sizeof(sa));
    sa.sa_handler = handler;
    sigfillset(&sa.sa_mask);
    sigaction(sig,&sa,nullptr);
}

//添加文件描述符到epoll当中 http_conn.cpp里实现
extern void addfd(int epollfd,int fd,bool one_shot);
//从epoll中删除文件描述符
extern void removefd(int epollfd,int fd);
//修改文件描述符，重置socket EPOLLONESHOT和EPOLLRDHUP事件，确保下一次可读时EPOLLIN时间被触发
extern void modfd(int epollfd,int fd,int ev);

int main(int argc,char* argv[]){

    if(argc <= 1){
        printf("按照如下格式运行：%s port_num\n",basename(argv[0]));
        exit(-1);
    }

    //获取端口号
    int port = atoi(argv[1]);

    //对SIGPIE信号做处理,SIG_IGN忽略信号
    addsig(SIGPIPE,SIG_IGN);

    //创建线程池，初始化线程池
    threadpool<http_conn>* pool = nullptr;
    try{
        pool = new threadpool<http_conn>;
    }catch(...){
        exit(-1);
    }

    //创建一个数组用于保存所有的用户客户端信息
    http_conn * users = new http_conn[ MAX_FD ];
    
    int listenfd = socket(PF_INET,SOCK_STREAM,0);

    //设置端口复用,绑定之前设置
    int reuse = 1;
    setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));

    //绑定
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    bind(listenfd,(struct sockaddr*)&address,sizeof(address));

    //监听
    listen(listenfd,5);

    //创建epoll对象，事件数组
    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);//参数会被忽略，>0即可

    //将监听的文件描述符到epoll对象中
    addfd(epollfd,listenfd,false);
    http_conn::m_epollfd = epollfd;

    while(true){
        //如果成功，返回请求的I/O准备就绪的文件描述符的数目
        int num = epoll_wait(epollfd,events,MAX_EVENT_NUMBER,-1);
        if((num<0)&&(errno!=EINTR)){
            printf("epoll failure\n");
            break;
        }

        //循环遍历事件数组
        for(int i=0;i<num;i++){
            int sockfd = events[i].data.fd;//data类型为一个union
            if(sockfd == listenfd){
                //有客户端连接进来
                struct sockaddr_in client_address;
                socklen_t client_addrlen = sizeof(client_address);
                int connfd = accept(listenfd,(struct sockaddr*)&client_address,&client_addrlen);
            
                if(http_conn::m_user_count>=MAX_FD){
                    //目前的n接数满了
                    //这里可以告诉客户端服务器内部正忙
                    close(connfd);
                    continue;
                }
                // 将新的客户的数据初始化放到数组当中
                users[connfd].init(connfd,client_address);
            }else if(events[i].events & (EPOLLRDHUP|EPOLLHUP|EPOLLERR)){
                //对方异常断开，关闭链接
                users[sockfd].close_conn();
            }else if(events[i].events & EPOLLIN){
                //有读事件发生
                if(users[sockfd].read()){
                    //一次性把数据全部读完
                    pool->append(users+sockfd);
                }else{
                    //没读到数据或者关闭了
                    users[sockfd].close_conn();
                }
            }else if(events[i].events & EPOLLOUT){
                //有写事件发生
                if(!users[sockfd].write()){
                    //写失败了
                    users[sockfd].close_conn();
                }
            }
        }
    }

    close(epollfd);
    close(listenfd);
    delete [] users;
    delete pool;

    return 0;
}
