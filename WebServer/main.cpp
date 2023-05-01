#include <iostream>
#include "log/log.h"

using namespace std;

int main()
{

	sql_pool::getinstance()->init();

	log::getinstance()->init();

    return 0;
}
