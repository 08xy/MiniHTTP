#pragma once

#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <cstdlib>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/sendfile.h>
#include <fcntl.h>
#include "Log.hpp"
#include "Util.hpp"

#define SEP ": "
#define WEB_ROOT "wwwroot"
#define HOME_PAGE "index.html"
#define HTTP_VERSION "HTTP/1.0"
#define LINE_END "\r\n"
#define PAGE_404 "404.html"
#define PAGE_400 "400.html"
#define PAGE_500 "500.html"

#define OK 200
#define BAD_REQUEST 400
#define NOT_FOUND 404
#define SERVER_ERROR 500


static std::string CodeDesc(int code)
{
    std::string desc;
    switch(code)
    {
        case 200:
            desc = "OK";
            break;
        case 404:
            desc = "NOT_FOUND";
            break;
        default:
            break;
    }
    return desc;
}

static std::string SuffixDesc(const std::string &suffix)
{
    static std::unordered_map<std::string, std::string> suffix_desc = {
        {".html", "text/html"},
        {".css", "text/css"},
        {".js", "application/javascript"},
        {".xml", "application/xml"},
        {".jpg", "application/x-jpg"}
    };

    auto iter = suffix_desc.find(suffix);
    if(iter != suffix_desc.end())
    {
        return iter->second;
    }
    //若没找到映射, 默认为text/html
    return "text/html"; 
}

class HttpRequest
{
public:
    std::string request_line;
    std::vector<std::string> request_header;
    std::string blank;
    std::string request_body;

    //解析完毕后的结果
    std::string method;
    std::string uri;
    std::string version;

    std::unordered_map<std::string, std::string> header_kv;
    int content_length;
    std::string path;          //所请求文件的路径
    std::string suffix;        //所请求文件的后缀
    std::string query_string;  //uri的参数

    bool cgi;
    int size;

public:
    HttpRequest(): content_length(0), cgi(false){}
    ~HttpRequest() {}
};

class HttpResponse
{
public:
    std::string status_line;
    std::vector<std::string> response_header;
    std::string blank;
    std::string response_body;

    int status_code;
    int fd;

public:
    HttpResponse() : blank(LINE_END), status_code(OK), fd(-1) {}
    ~HttpResponse() {}
};

//读取请求, 分析请求, 构建响应
class EndPoint
{
private:
    int sock;
    bool stop;
    HttpRequest http_request;
    HttpResponse http_response;
private:
    bool RecvHttpRequestLine()
    {
       auto& line = http_request.request_line;
       if(Util::ReadLine(sock, line) > 0)
       {
            line.resize(line.size() - 1);  // 去除 \n
            LOG(INFO, http_request.request_line);
       }
       else
       {
            // 读入出错
            stop = true;
       }
    }
    bool RecvHttpRequestHeader()
    {
        std::string line;
        while(true)
        {
            line.clear();
            if(Util::ReadLine(sock, line) <= 0)
            {
                // 读入出错
                stop = true;
                break;
            }
            if(line == "\n")
            {
                http_request.blank = "\n";
                break;
            }
            line.resize(line.size() - 1); // 去除  \n
            http_request.request_header.push_back(line);
            //LOG(INFO, line);
        }
    }
    void ParseHttpRequestLine()
    {
        auto& line = http_request.request_line;
        std::stringstream ss(line);
        ss >> http_request.method >> http_request.uri >> http_request.version;
        auto& method = http_request.method;
        std::transform(method.begin(), method.end(), method.begin(), ::toupper);
        LOG(INFO, line);
    }
    void ParseHttpRequestHeader()
    {
        std::string key;
        std::string value;
        for(auto& iter : http_request.request_header)
        {
            if(Util::CutString(iter, key, value, SEP))
            {
                http_request.header_kv.insert({key, value});
            }
        }
    }
    bool IsNeedRecvHttpRequestBody()
    {
        auto& method = http_request.method;
        if(method == "POST")
        {
            auto& header_kv = http_request.header_kv;
            auto iter = header_kv.find("Content-Length");
            if(iter != header_kv.end())
            {
                LOG(INFO, "POST Length"+iter->second);
                http_request.content_length = atoi(iter->second.c_str());
                return true;
            }
        }
        return false;
    }
    bool RecvHttpRequestBody()
    {
        if(IsNeedRecvHttpRequestBody())
        {
            int content_length = http_request.content_length;
            auto& body = http_request.request_body;

            char ch = 0;
            while(content_length)
            {
                ssize_t s = recv(sock, &ch, 1, 0);
                if(s > 0)
                {
                    body.push_back(ch);
                    content_length--;
                }
                else
                {
                    stop = true; // 读取出错
                    break;
                }
            }
        }

        return stop;
    }
    void HandlerError(std::string page)
    {
        std::cout << "debug: " << page << std::endl;
        http_request.cgi = false; // 部分出错处理走到这, cgi == true

        // 返回对应的错误页面
        http_response.fd = open(page.c_str(), O_RDONLY);
        if(http_response.fd > 0)
        {
            struct stat st;
            stat(page.c_str(), &st);
            http_request.size = st.st_size;

            std::string line = "Content-Type: text/html";
            line += LINE_END;
            http_response.response_header.push_back(line);

            line = "Content-Length: ";
            line += std::to_string(http_request.size); // st.st_size
            line += LINE_END;
            http_response.response_header.push_back(line);
        }
    }
    void BuildOKResponse()
    {
        // 构建响应报头
        std::string line = "Content-Type: ";
        line += SuffixDesc(http_request.suffix);
        line += LINE_END;
        http_response.response_header.push_back(line);

        line = "Content-Length: ";
        if(http_request.cgi)
        {
            line += std::to_string(http_response.response_body.size());
        }
        else
        {
            line += std::to_string(http_request.size); // GET
        }
        line += LINE_END;
        http_response.response_header.push_back(line);
    }
    void BuildHttpResponseHelper()
    {
        auto &code = http_response.status_code;

        // 构建状态行
        auto &status_line = http_response.status_line;
        // HTTP/1.0 200 OK
        status_line += HTTP_VERSION;
        status_line += " ";
        status_line += std::to_string(code);
        status_line += " ";
        status_line += CodeDesc(code);
        status_line += LINE_END;

        // 构建响应正文,可能包含响应报头
        std::string path = WEB_ROOT;
        path += "/";
        switch(code)
        {
            case OK:
            BuildOKResponse();
            break;
            case NOT_FOUND:
            path += PAGE_404;
            HandlerError(path);
            break;
            case BAD_REQUEST:
            path += PAGE_404;
            HandlerError(path);
            break;
            case SERVER_ERROR:
            path += PAGE_404;
            HandlerError(path);
            break;
            default:
            break;
        }
    }
    int ProcessNonCgi()
    {
        http_response.fd = open(http_request.path.c_str(), O_RDONLY);
        if(http_response.fd >= 0)
        {
            LOG(INFO, http_request.path + "open success");
            return OK;
        }
        return NOT_FOUND;
    }
    int ProcessCgi()
    {
        std::cout << "debug: " << "Use CGI Modle" << std::endl;

        int code = OK;
        auto &method = http_request.method;
        auto &query_string = http_request.query_string; // GET
        auto &body_text = http_request.request_body;    // POST
        auto &bin = http_request.path;  // 走到这,子进程执行的目标程序一定存在
        int content_length = http_request.content_length;
        auto &response_body = http_response.response_body;

        std::string method_env;
        std::string query_string_env;
        std::string content_length_env;

        // 站在父进程角度,建立父子进程的两个通信管道
        int input[2];
        int output[2];
        if(pipe(input) < 0)
        {
            LOG(ERROR, "pipe input error");
            code = 404;
            return code;
        }
        if(pipe(output) < 0)
        {
            LOG(ERROR, "pipe output error");
            code = 404;
            return code;
        }

        //这是一个新线程, 但是目前只有一个进程httpserver, 所以不能直接进行进程替换
        pid_t pid = fork();
        if(0 == pid)    //child
        {
            close(input[0]);
            close(output[1]);

            method_env = "METHOD=";
            method_env += method;
            putenv((char*)method_env.c_str());

            if("GET" == method)
            {
                query_string_env = "QUERY_STRING=";
                query_string_env += query_string;
                putenv((char*)query_string_env.c_str());
                LOG(INFO, "GET Method, Add Query_string_Env");
            }
            else if("POST" == method)
            {
                content_length_env = "CONTENT_LENGTH";
                content_length_env += std::to_string(content_length);
                putenv((char*)content_length_env.c_str());
                LOG(INFO, "POST Method, Add Content_length_env");
            }
            else{}

            std::cout << "bin: " << bin <<std::endl;

            //站在子进程角度
            //  input[]: 写出 -> 1 -> input[1]
            // output[]: 读入 <- 0 <- output[0]
            dup2(output[0], 0);
            dup2(input[1], 1);

            //替换成功后, 目标子进程不需要知道pipefd, 知道读0.写1即可

            execl(bin.c_str(), bin.c_str(), nullptr);
            exit(1);
        }
        else if(pid < 0)
        {
            LOG(ERROR, "fork error");
            return NOT_FOUND;
        }
        else            //parent
        {
            close(input[1]);
            close(output[0]);

            if("POST" == method)
            {
                const char *start = body_text.c_str();
                int total = 0;
                int size = 0;
                while(total < content_length &&
                      (size = write(output[10], start+total, body_text.size()-total)) > 0)
                {
                    total += size;
                }
            }

            // 从管道中读取子进程处理完的数据
            char ch = 0;
            while(read(input[0], &ch, 1) > 0)
            {
                response_body.push_back(ch);
            }

            int status = 0;
            pid_t ret = waitpid(pid, &status, 0);
            if(ret == pid)
            {
                if(WIFEXITED(status)) //进程正常退出
                {
                    if(WEXITSTATUS(status) == 0) //退出码为0
                    {
                        code = OK;
                    }
                    else
                    {
                        code = BAD_REQUEST;
                    }
                }
                else
                {
                    code = SERVER_ERROR;
                }
            }

            //记得回收文件描述符
            close(input[0]);
            close(output[1]);
        }

        return code;
    }
    
public:
    EndPoint(int _sock) :sock(_sock), stop(false) {}

    bool IsStop()
    {
        return stop;
    }

    void RecvHttpRequest()
    {
        if( (!RecvHttpRequestLine()) && (!RecvHttpRequestHeader()) )
        {
            // 读请求行不停止(成功)   &&     读请求报头不停止(成功)
            // 即读取成功
            ParseHttpRequestLine();
            ParseHttpRequestHeader();
            RecvHttpRequestBody();
        }
    }
    void BulidHttpResponse()
    {
        std::string _path;
        struct stat st;
        std::size_t found = 0;
        auto& code = http_response.status_code;

        if(http_request.method != "GET" && http_request.method != "POST")
        {
            // 非法请求: 既不是GET,也不是POST
            std::cout << "method: " << http_request.method << std::endl;
            code = BAD_REQUEST;
            goto END;
        }

        if(http_request.method == "GET")
        {
            size_t pos = http_request.uri.find('?');
            if(pos != std::string::npos)    // GET方法带参请求
            {
                Util::CutString(http_request.uri, http_request.path, http_request.query_string, "?");
                http_request.cgi = true;
            }
            else                            // GET方法不带参请求
            {
                http_request.path = http_request.uri;
            }
        }
        else if(http_request.method == "POST")   // POST方法请求
        {
            http_request.cgi = true;
            http_request.path = http_request.uri;
        }
        else      // 请求方法扩展
        {}

        //到了这一步, path缺少web根目录的前缀
        _path = http_request.path;
        http_request.path = WEB_ROOT;
        http_request.path += _path;

        // 处理请求资源为 / 也就是首页的情况
        if(http_request.path[http_request.path.size()-1] == '/')
        {
            http_request.path += HOME_PAGE;
        }

        if(stat(http_request.path.c_str(), &st) == 0)
        {   
            // 说明资源存在
            if(S_ISDIR(st.st_mode))
            {
                //说明请求的资源是一个目录, 返回当前目录的首页
                //虽然是一个目录,但不以 / 结尾
                http_request.path += "/";
                http_request.path += HOME_PAGE;
                stat(http_request.path.c_str(), &st);
            }
            if((st.st_mode&S_IXUSR) || (st.st_mode&S_IXGRP) || (st.st_mode&S_IXOTH))
            {
                //说明当前所请求的资源是一个可执行文件 需要特殊处理
                http_request.cgi = true;
            }
            http_request.size = st.st_size;
        }
        else
        {
            //说明资源不存在
            std::string info = http_request.path;
            info += " NOT_FOUND";
            LOG(WARNING, info);
            code = NOT_FOUND;
            goto END;
        }

        //path
        found = http_request.path.rfind(".");
        if(found == std::string::npos)
        {
            http_request.suffix = ".html"; //没找到后缀, 默认为.html
        }
        else
        {
            http_request.suffix = http_request.path.substr(found);
        }

        if(http_request.cgi)
        {
            code = ProcessCgi();
        }
        else
        {
            code = ProcessNonCgi();  //返回静态网页
        }

END:    
        BuildHttpResponseHelper(); // 状态行填充了, 响应报头也有了, 空行有了, 正文也有了
    }
    void SendHttpResponse()
    {
        send(sock, http_response.status_line.c_str(), http_response.status_line.size(), 0);
        for(auto iter : http_response.response_header)
        {
            send(sock, iter.c_str(), iter.size(), 0);
        }
        send(sock, http_response.blank.c_str(), http_response.blank.size(), 0);
        
        // 正文: NonCgi->fd  Cgi->response_body
        if(http_request.cgi)
        {
            auto &response_body = http_response.response_body;
            size_t size = 0;
            size_t total = 0;
            const char* start = response_body.c_str();

            std::cerr << " debug: cgi_out " << response_body << std::endl << std::endl;
            while(total < response_body.size() && (size = send(sock, start+total, response_body.size() - total, 0)) > 0)
            {
                std::cout << "debug: size = " << size << std::endl;
                total += size;
            }
            std::cerr << " debug: cgi_out " << response_body << std::endl << std::endl;
        }
        else
        {
            std::cout << "..........fd..........." << http_response.fd << std::endl;
            std::cout << ".........size.........." << http_request.size << std::endl;
            sendfile(sock, http_response.fd, nullptr, http_request.size);
            close(http_response.fd); 
        }
    }
    ~EndPoint()
    {
        close(sock);
    }
};

class CallBack
{
public:
    CallBack() {}

    void operator()(int sock)
    {
        HandlerRequest(sock);
    }

    void HandlerRequest(int sock)
    {
        LOG(INFO, "Handler Request Begin");
        
        EndPoint* ep = new EndPoint(sock);
        ep->RecvHttpRequest();
        if(!ep->IsStop())
        {
            LOG(INFO, "Recv Not Error, Begin Build and Send");
            ep->BulidHttpResponse();
            ep->SendHttpResponse();
        }
        else
        {
            LOG(WARNING, "Recv Error, Stop Build and Error");
        }
        delete ep;

        LOG(INFO, "Handler Request End");
        //close(sock);
    }
    ~CallBack() {}
};
