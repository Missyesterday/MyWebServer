#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include "locker.h"
#include "threadpool.h"
#include "http_conn.h"

//最大文件描述符个数
#define MAX_FD 65535

//一次最多监听的事件数量
#define MAX_EVENT_NUMBER  10000



//添加信号捕捉
void addsig(int sig, void(handler)(int))
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    sigfillset(&sa.sa_mask);
    sigaction(sig, &sa, nullptr);

}

//添加文件描述符到epoll中
extern void addfd(int epollfd, int fd, bool one_shot);
//从epoll中删除文件描述符
extern void removefd(int epollfd, int fd);
//从epoll中修改文件描述符
extern void modfd(int epollfd, int fd, int ev);

int main(int argc, char* argv[])
{
    if(argc <= 1 )
    {
        printf("按照如下格式运行: %s port_num\n", basename(argv[0]));
        exit(-1);
    }

    //获取端口号
    int port = atoi(argv[1]);

    //对SIGPIE信号进行处理
    addsig(SIGPIPE, SIG_IGN);

    //创建线程池, 初始化线程池
    threadpool<http_conn>* pool = nullptr;
    try
    {
        pool = new threadpool<http_conn>;
    }
    catch (...)
    {
        exit(-1);
    }


    //创建一个数组用于保存所有的客户端信息
    http_conn* users = new http_conn[ MAX_FD ];

    int listenfd = socket(PF_INET, SOCK_STREAM, 0);

    //设置端口复用
    int reuse = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

   // 绑定
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    bind(listenfd, (struct sockaddr*)&address, sizeof(address));

    //监听
    listen(listenfd, 5);

    //创建epoll对象, 事件数组, 添加
    epoll_event events[ MAX_EVENT_NUMBER ];

    int epollfd = epoll_create(5);

    //将监听的文件描述符添加到epoll对象中
    addfd(epollfd, listenfd, false);

    http_conn::m_epollfd = epollfd;

    while(true)
    {
        int num = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if( (num < 0) && (errno != EINTR))
        {
            printf("epoll failure\n");
            break;
        }

        //循环遍历事件数组
        for(int i = 0; i < num; ++i)
        {
            int sockfd = events[i].data.fd;
            if(sockfd == listenfd)
            {
                //表示有客户端连接进来

                struct sockaddr_in client_addr{};
                socklen_t client_addrlen = sizeof(client_addr);

                int connfd = accept(listenfd, (struct sockaddr*)&client_addr, &client_addrlen);

                if(http_conn::m_user_count >= MAX_FD)
                {
                    //目前的连接数满了,
                    //给客户端显示一个 服务器内部正忙

                    close(connfd);
                    continue;
                }

                //将新的客户端的数据初始化, 放到数组里面
                users[connfd].init(connfd, client_addr);
            }

            else if(events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                //对方异常断开或者错误的事件
                //此时需要关闭连接
                users[sockfd].close_conn();

            }
            else if(events[i].events & EPOLLIN)
            {
                //一次性读出所有的数据
                if(users[sockfd].read())
                {
                    pool->append(users + sockfd);
                }
            }
            else if(events[i].events & EPOLLOUT)
            {
                //一次性写完所有的数据
                if(!users[sockfd].write())
                {
                    users[sockfd].close_conn();
                }
            }

        }

    }

    close(epollfd);
    close(listenfd);
    delete[] users;
    delete pool;
    return 0;
}
