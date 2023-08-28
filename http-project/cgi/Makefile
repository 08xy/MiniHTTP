.PHONY:all
all:operator_cgi mysql_cgi

operator_cgi:operator_cgi.cc
	g++ -o $@ $^ -std=c++11
mysql_cgi:mysql.cc
	g++ -o $@ $^ -std=c++11 -lmysqlclient -lpthread -ldl -static

.PHONY:clean
clean:
	rm -f mysql_cgi operator_cgi
