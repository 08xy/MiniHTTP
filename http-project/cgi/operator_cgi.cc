#include <iostream>
#include "comm.hpp"

int main()
{
    std::string query_string;
    GetQueryString(query_string);
    //x=10&b=20

    std::string str1;
    std::string str2;
    CutString(query_string, "&", str1, str2);

    std::string name1;
    std::string value1;
    CutString(str1, "=", name1, value1);

    std::string name2;
    std::string value2;
    CutString(str2, "=", name2, value2);

    int x = atoi(value1.c_str());
    int y = atoi(value2.c_str());

    //返回
    
    std::cout << "<html>";
    std::cout << "<head><meta charset=\"utf-8\"></head>";
    std::cout << "<body>";
    std::cout << "<h3> " << value1 << " + " << value2 << " = " << x+y << "</h3>";
    std::cout << "<body>";
    std::cout << "</html>";

    return 0;
}
