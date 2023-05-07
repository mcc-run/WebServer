#include <iostream>
#include "Server/webserver.h"

using namespace std;

int main()
{
	
	WebServer server;

	server.init();

	server.Sql_pool();

	server.log_write();

	server.threads_pool();

	server.eventListen();

	server.eventLoop();

	

    return 0;
}

