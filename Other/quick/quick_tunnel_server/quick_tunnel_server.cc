#include "worker.h"
#include <map>
#include "../log/LogWrapper.h"

LogWrapper g_log;
int main(int argc, char* argv[]) {
	LogWrapper::Load();
	g_log.Open("tunnel_server");

	Worker worker;
	worker.Exec();
	return 0;
}