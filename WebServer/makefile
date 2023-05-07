WebServer: main.cpp sql_pool/sql_pool.cpp log/log.cpp http_conn/http_conn.cpp timer/lst_timer.cpp Server/webserver.cpp
	g++ -o $@ $^ -I./lock -I./sql_pool -I./thread_pool -I./log -I./http_conn -I./timer -I./Server -L/usr/lib64/mysql -lmysqlclient -pthread


