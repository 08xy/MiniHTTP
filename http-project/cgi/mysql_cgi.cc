#include <iostream>
#include <string>
#include <unordered_map>
#include "comm.hpp"
#include  "mysql/mysql.h"


const std::string host = "127.0.0.1";
const std::string user = "root";
const std::string password = "root";
const std::string database = "http_user";
const unsigned int port = 3306;

class Mysql
{
private:
    MYSQL *conn;

public:
    Mysql():conn(nullptr)
    {
        // 创建mysql句柄
        conn = mysql_init(nullptr);
        // 设置编码类型
        mysql_set_character_set(conn, "utf8");
        Connect();
    }
    bool Connect()
    {
        if(nullptr == mysql_real_connect(conn, host.c_str(), user.c_str(), password.c_str(), database.c_str(), port, nullptr, 0))
        {
            std::cerr << "connect error" << std::endl;
            return false;
        }
        std::cerr << "connect success" << std::endl;
        return true;
    }

    bool Insert(std::string sql)
    {
        std::cerr << "query: " << sql << std::endl;
        int ret = mysql_query(conn, sql.c_str());
        std::cerr << "ret: " << ret << std::endl;
        return true;
    }
};

    static std::unordered_map<std::string, int> sql_type;

    static void InitSqlType()
    {
        sql_type.insert({"insert", 1});
        sql_type.insert({"update", 2});
        sql_type.insert({"delete", 3});
        sql_type.insert({"select", 4});
    }
    std::string& MakeSql(std::string sqltype, std::string _name, std::string _passwd)
    {
        std::string sql;
        int type = sql_type[sqltype];
        switch(type)
        {
            case 1:
            sql = "insert into user(name, passwd) values (\'";
            sql += _name;
            sql += "\', \'";
            sql += _passwd;
            sql +="\')";
            break;
            case 2:
            break;
            case 3:
            break;
            case 4:
            break;
            default:
            std::cerr << "你小子越界了!" << std::endl;
            break;
        }

        return sql;
    }

int main()
{
    Mysql myself;
    InitSqlType();

    // 1.获得数据
    std::string query_string;
    if (GetQueryString(query_string))
    {
        std::cerr << query_string << std::endl;

        // 2.数据分离提取
        std::string name;
        std::string passwd;

        CutString(query_string, "&", name, passwd);

        std::string name_;
        std::string _name;
        CutString(name, "=", name_, _name);

        std::string passwd_;
        std::string _passwd;
        CutString(passwd, "=", passwd_, _passwd);

        std::string sql = MakeSql("insert", _name, _passwd);
        myself.Insert(sql);
    }

    return 0;
}
