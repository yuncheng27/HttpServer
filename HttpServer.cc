#include<iostream>
#include"HttpServer.hpp"


static void Usage(std::string proc)
{
    std::cout << "Usage: " << proc << " port" << std::endl;
}

// ./HttpServer 8080
int main(int argc, char *argv[])
{
    if(argc != 2){
        Usage(argv[0]);
        exit(1);
    }
    HttpServer *ser = new HttpServer(atoi(argv[1]));
    ser->InitServer();
    ser->Start();

}
