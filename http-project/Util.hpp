#pragma once

#include <iostream>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>


// 工具类
class Util
{
public:
    static int ReadLine(int sock, std::string& out)
    {
        char ch = 'S';
        while(ch != '\n')
        {
            ssize_t s = recv(sock, &ch, 1, 0);
            if(s > 0)
            {
                if(ch == '\r')
                {
                    recv(sock, &ch, 1, MSG_PEEK); //数据窥探
                    if(ch == '\n')
                    {
                        // \r\n -> \n
                        recv(sock, &ch, 1, 0);
                    }
                    else
                    {
                        // \r -> \n
                        ch = '\n';
                    }
                }
                // 1. 普通字符
                // 2. \n
                out.push_back(ch);
            }
            else if(0 == s)
            {
                return 0; //链接关闭了
            }
            else
            {
                return -1; //err
            }
        }

        return out.size();
    }

    static bool CutString(const std::string& target, std::string& sub1_out, std::string& sub2_out, std::string sep)
    {
        size_t pos = target.find(sep);
        if(pos != std::string::npos)
        {
            sub1_out = target.substr(0, pos);
            sub2_out = target.substr(pos + sep.size());
            return true;
        }
        return false;
    }
};