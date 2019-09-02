#ifndef __HTTPSEVER_HPP_
#define __HTTPSEVER_HPP_

#include<iostream>
#include<pthread.h>
#include"ProtocolUtil.hpp"
#include"ThreadPool.hpp"

class HttpServer{
    private:
        int listen_sock;
        int port;
    public:
        HttpServer(int port_):listen_sock(-1),port(port_)
        {}

        void InitServer()
        {
            listen_sock = SocketApi::Socket();
            SocketApi::Bind(listen_sock, port);
            SocketApi::Listen(listen_sock);
        }
        void *HandlerRequest(void *arg)
        {
            pthread_detach(pthread_self());
            int sock = *(int *)arg;
            
            //for test
            char buff[10240];
            read(sock, buff, sizeof(buff));
            std::cout << buff << std::endl;
            
            close(sock);
            return (void *)0;
        }
        void Start()
        {
            for (;;){
                std::string peer_ip;
                int peer_port;
                int sock = SocketApi::Accept(listen_sock, peer_ip, peer_port);
                if(sock >= 0) {
                    std::cout << peer_ip << " : "<<peer_port << std::endl;
                    Task t(sock, Entry::HandlerRequest);
                    singleton::GetInstance()->PushTask(t);
                    //pthread_t tid;
                    //pthread_create(&tid, NULL, Entry::HandlerRequest, (void *)sockp);
                }
            }
        }
        ~HttpServer()
        {
            if(listen_sock >= 0) {
                close(listen_sock);
            }
        }
};

#endif
