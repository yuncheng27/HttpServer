#ifndef __PROTOCOLUTIL_HPP__
#define __PROTOCOLUTIL_HPP__

#include<iostream>
#include<sys/types.h>
#include<string.h>
#include<sstream>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<stdlib.h>
#include<unistd.h>
#include<algorithm>
#include<vector>
#include<sys/stat.h>
#include<unordered_map>
#include<sys/sendfile.h>
#include<fcntl.h>
#include<sys/wait.h>

#define BACKLOG 5

#define NORMAL 0
#define WARNING 1
#define ERROR 2
#define BUFF_NUM 1024
#define WEBROOT "wwwroot"
#define HOMEPAGE "index.html"

const char *ErrLevel[]={   //或者使用std::cerr,是ISO C++标准错误输出流，对应于ISO C标准库的stderr
    "Normal",
    "Warning",
    "Error"
};

// _FILE_ _LINE_
void log(std::string msg, int level, std::string file, int line)
{
    std::cout << "[ "<< file << ":" <<line<<" ]"<<msg << "[ "<< ErrLevel[level]<< " ]"<<std::endl;
}

#define LOG(msg, level) log(msg, level, __FILE__, __LINE__);  //日志功能

class Util{
    public:
        // XXXX: YYYY {k, v}
        static void MakeKV(std::string s, std::string &k, std::string &v)
        {
            std::size_t pos = s.find(": ");
            k = s.substr(0, pos);
            v = s.substr(pos + 2);
        }

        static std::string IntToString(int &x)
        {
            std::stringstream ss;
            ss << x;
            return ss.str();
        }
        static std::string CodeToDesc(int code)
        {
            switch(code)
            {
                case 200:
                    return "OK";
                case 404:
                    return "Not Found";
                default:
                    break;
            }
            return "Unkown";
        }

        static std::string SuffixToContent(std::string &suffix)
        {
            if(suffix == ".css")
            {
                return "text/css";
            }

            if(suffix == ".js")
            {
                return "application/x-javascript";
            }

            if(suffix == ".html" || suffix == ".htm")
            {
                return "text/html";
            }

            if(suffix == ".jpg")
            {
                return "application/x-jpg";
            }
            return "text/html";
        }
};

class SocketApi{
    public:
        static int Socket()
        {
            int sock = socket(AF_INET, SOCK_STREAM, 0);
            if (sock < 0){
                LOG("socket error!", ERROR);
                exit(2);
            }
            int opt = 1;
            setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
            return sock;
        }
        static void Bind(int sock, int port)
        {
            struct sockaddr_in local;
            bzero(&local, sizeof(local));
            local.sin_family = AF_INET;
            local.sin_port = htons(port);
            local.sin_addr.s_addr = htonl(INADDR_ANY); //动态绑定  //此处为赋值，所以必须把结构体写透   
            //vim /usr/include/linux/in.h 第221行和第80行 
            if(bind(sock, (struct sockaddr*)&local, sizeof(local)) < 0){
                LOG("Bind error!", ERROR);
                exit(3);
            }
        }
        static void Listen(int sock)
        {
            if(listen(sock, BACKLOG) < 0){
                LOG("listen error", ERROR);
                exit(4);
            }
        }
        static int Accept(int listen_sock, std::string &ip, int &port){
            struct sockaddr_in peer;
            socklen_t len = sizeof(peer);
            int sock = accept(listen_sock, (struct sockaddr *)&peer, &len);
            if (sock < 0) {
                LOG("accept error", WARNING);
                return -1;
            }
            port = ntohs(peer.sin_port);
            ip = inet_ntoa(peer.sin_addr);//调用函数形参实例化的时候实际上是以初始化的方式调用函数
            return sock;
        }
};

class Http_Response{
    public:
        //基本协议字段
        std::string status_line;
        std::vector<std::string> response_header;
        std::string blank;
        std::string response_text;
    private:
        int code;
        std::string path;
        int resource_size;

    public:
        Http_Response():blank("\r\n"), code(200),resource_size(0)
    {}

        int &Code()
        {
            return code;
        }

        void SetPath(std::string &path_)
        {
            path = path_;
        }

        std::string &Path()
        {
            return path;
        }

        void SetResourseSize(int rs)
        {
            resource_size = rs;
        }

        int ResourceSize()
        {
            return resource_size;
        }

        void MakeStatusLine()
        {
            status_line = "HTTP/1.0";
            status_line += " ";
            status_line += Util::IntToString(code);
            status_line += " ";
            status_line += Util::CodeToDesc(code); 
            status_line += "\r\n";
            LOG("Make Status Line Done!", NORMAL);
        }

        void MakeResponseHeader()
        {
            std::string line;
            std::string suffix;
            //Content_Type
            line = "Content-Length: ";
            std::size_t pos = path.rfind('.');
            if(std::string::npos != pos)
            { 
                suffix = path.substr(pos);
                transform(suffix.begin(), suffix.end(), suffix.begin(), ::tolower); //把大写后缀转换为小写
            }
            line += Util::SuffixToContent(suffix);
            line += "\r\n";
            response_header.push_back(line);

            //Content_Length
            line = "Content-Length: ";
            line += Util::IntToString(resource_size);
            line += "\r\n";
            response_header.push_back(line);

            line += "\r\n";
            response_header.push_back(line);

            LOG("Make Response Header Done!", NORMAL);
        }
        ~Http_Response()
        {}
};

class Http_Request{
    public:
        //基本协议字段
        std::string request_line;
        std::vector<std::string> request_header;
        std::string blank;
        std::string request_text;
    private:   
        //解析字段
        std::string method;
        std::string uri;   //path?arg
        std::string version;
        std::string path;
        //int resource_size;
        std::string query_string;
        std::unordered_map<std::string, std::string> header_kv;  //kv映射
        bool cgi;

    public:
        Http_Request():path(WEBROOT),cgi(false),blank("\r\n")
    {}
        void RequestLineParse()
        {
            std::stringstream ss(request_line);
            ss >> method >> uri >> version;  
            transform(method.begin(), method.end(), method.begin(), ::toupper);
        }

        void UriParse()
        {
            if(method == "GET"){
                std::size_t pos = uri.find('?');
                if(pos != std::string::npos) {
                    cgi = true;
                    path = uri.substr(0, pos);
                    query_string = uri.substr(pos + 1);
                }
                else{
                    path += uri;    // wwwroot/
                }
            }
            else{
                cgi = true;
                path += uri;
            }
            if(path[path.size() - 1] == '/')
            {
                path += HOMEPAGE;  // wwwroot/index
            }
        }

        void HeaderParse()
        {
            std::string k, v;
            for(auto it = request_header.begin(); it !=  request_header.end(); it++)
            {
                Util::MakeKV(*it, k, v);
                header_kv.insert({k, v});
            }
        }

        bool IsMethodLegal()
        {
            if(method != "GET" && method != "POST"){
                return false;
            }
            return true;
        }

        int IsPathLegal(Http_Response *rsp)  // wwwroot/a/b/c/d.html
        {
            int rs = 0;
            struct stat st;  // man stat

            if(stat(path.c_str(), &st) < 0) 
            {
                return 404;
            }
            else
            {
                rs= st.st_size;

                if(S_ISDIR(st.st_mode))
                {
                    path += "/";
                    path += HOMEPAGE;
                    stat(path.c_str(), &st);
                    rs = st.st_size;
                }
                else if((st.st_mode &S_IXUSR) || \
                        (st.st_mode &S_IXGRP) || \
                        (st.st_mode &S_IXOTH))
                {
                    cgi = true;
                }
                else
                {
                    //TODO
                }
            }
            rsp->SetPath(path);
            rsp->SetResourseSize(rs);

            LOG("Path is OK!", NORMAL);
            return 0;
        }

        bool IsNeedRecv()
        {
            return method == "POST" ? true : false;
        }

        bool IsCgi()
        {
            return cgi;
        }

        int ContentLength()
        {
            int content_length = -1;
            std::string cl = header_kv["Content-Length"].c_str();
            std::stringstream ss(cl);
            ss >> content_length;
            return content_length;
        }

        std::string GetParam()
        {
            if(method == "GET")
            {
                return query_string;
            }
            else{
                return request_text;
            }
        }

        ~Http_Request()
        {}       
};

class Connect{
    private:
        int sock;
    public:
        Connect(int sock_):sock(sock_)
    {}
        int RecvOneLine(std::string &line_)
        {
            char buff[BUFF_NUM];
            int i = 0;
            char c = 'X';
            while(c != '\n' && i < BUFF_NUM - 1){
                ssize_t s = recv(sock, &c, 1, 0);
                if(s > 0) {
                    if(c == '\r')
                    {
                        recv(sock, &c, 1, MSG_PEEK);
                        if(c == '\n')
                        {
                            recv(sock, &c, 1, 0);
                        }
                        else{
                            c = '\n';
                        }
                    }
                    // \r \n \r\n 
                    buff[i++] = c;
                }
                else{
                    break;
                }
            }
            buff[i] = 0;
            line_ = buff;
            return i;
        }

        void RecvRequestHeader(std::vector<std::string> &v)
        {
            std::string line = "X";
            while (line != "\n") {
                RecvOneLine(line);
                if(line != "\n"){
                    v.push_back(line);
                }
            }
            LOG("Header Recv OK!", NORMAL);
        }
        void RecvText(std::string &text, int content_length)
        {
            char c;
            for(auto i = 0; i < content_length; i++){
                recv(sock, &c, 1, 0);
                text.push_back(c);
            }
        }

        void SendStatusLine(Http_Response *rsp)
        {
            std::string &sl = rsp->status_line;
            send(sock, sl.c_str(), sl.size(), 0);
        }

        void SendHeader(Http_Response *rsp)
        {
            std::vector<std::string> &v = rsp->response_header;
            for(auto it = v.begin(); it != v.end(); it++)
            {
                send(sock, it->c_str(), it->size(), 0);
            }
        }

        void SendText(Http_Response *rsp, bool cgi_)
        {
            if(!cgi_){
                std::string &path = rsp->Path();
                int fd = open(path.c_str(), O_RDONLY);
                if(fd < 0)
                {
                    LOG("open file error!", WARNING);
                    return;
                }
                sendfile(sock, fd, NULL, rsp->ResourceSize());
                close(fd);
            }
            else{
                std::string &rsp_text = rsp->response_text;
                send(sock, rsp_text.c_str(), rsp_text.size(), 0);
            }
        }
        ~Connect()
        {
            close(sock);
        }
};

class Entry{
    public:
        static int ProcessCgi(Connect *conn, Http_Request *rq, Http_Response *rsp)
        {
            int input[2];
            int output[2];
            pipe(input);  //child
            pipe(output);

            std::string bin = rsp->Path();  //wwwroot/a/b/binx;
            std::string param = rq->GetParam();
            int size = param.size();
            std::string param_size = "CONTENT-LENGTH=";
            param_size += Util::IntToString(size);

            std::string &response_text = rsp->response_text;

            pid_t id = fork();
            if(id < 0)
            {
                LOG("fork error!", WARNING);
                return 503;
            }
            else if(id == 0) //child
            {
                close(input[1]);
                close(output[0]);
                putenv((char *)param_size.c_str());
                dup2(input[0], 0);  //重定向到标准输入
                dup2(output[1], 1); //重定向到标准输出

                execl(bin.c_str(), bin.c_str(), NULL);
                exit(1);
            }
            else //father
            {
                close(input[0]);
                close(output[1]);
                char c;
                for(auto i = 0; i < size; i++)
                {
                    c = param[i];
                    write(input[1], &c, 1);
                }
                waitpid(id, NULL, 0);
                while(read(output[0], &c, 1) > 0)
                {
                    response_text.push_back(c); 
                }

                rsp->MakeStatusLine();
                rsp->SetResourseSize(response_text.size());
                rsp->MakeResponseHeader();

                conn->SendStatusLine(rsp);
                conn->SendHeader(rsp);  //add \n
                conn->SendText(rsp, true);
            }
            return 200;
        }
        static int ProcessNonCgi(Connect *conn, Http_Request *rq, Http_Response *rsp)
        {
            rsp->MakeStatusLine();
            rsp->MakeResponseHeader();

            conn->SendStatusLine(rsp);
            conn->SendHeader(rsp);  //add \n
            conn->SendText(rsp, false);
            LOG("Send Response Done!", NORMAL);
        }

        static int ProcessResponse(Connect *conn, Http_Request *rq, Http_Response *rsp)
        {
            if(rq->IsCgi()){
                LOG("MakeResponse Use Cgi", NORMAL);
                ProcessCgi(conn, rq, rsp); 
            }
            else{
                LOG("MakeResponse Use Non Cgi", NORMAL);
                ProcessNonCgi(conn, rq, rsp);
            }
        }

        static void HandlerRequest(int sock)
        {
            pthread_detach(pthread_self());

#ifdef _DEBUG_
            //for test
            char buff[10240];
            read(sock, buff, sizeof(buff));
            std::cout << "###################################" << std::endl;
            std::cout << buff << std::endl;
            std::cout << "###################################" << std::endl;            
#else
            Connect *conn = new Connect(sock);
            Http_Request *rq = new Http_Request();
            Http_Response *rsp = new Http_Response();
            int &code = rsp->Code();

            conn->RecvOneLine(rq->request_line);
            rq->RequestLineParse();

            if(!rq->IsMethodLegal())
            {
                code = 404;
                LOG("Request Method Is not Legal", WARNING);
                goto end;
            }

            rq->UriParse();

            if(rq->IsPathLegal(rsp) != 0){
                code = 404;
                LOG("file is not exist!", WARNING);
                goto end;
            }
            //request line done!
            //header
            conn->RecvRequestHeader(rq->request_header);
            rq->HeaderParse();
            //header done!
            if(rq->IsNeedRecv()) //是否需要继续读取请求
            {
                LOG("POST Method, Need Recv Begin!", NORMAL);
                conn->RecvText(rq->request_text, rq->ContentLength());
            }
            LOG("Http Request Recv Done, OK!", NORMAL);
            ProcessResponse(conn, rq, rsp);
end:
            delete conn;
            delete rq;
            delete rsp;
#endif    
            //std::cout<< line << std::endl;
        }
};

#endif
