/*
#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#define DEBUG_NEW new(_NORMAL_BLOCK, __FILE__, __LINE__)
#define new DEBUG_NEW
#endif
*/

#include "Server.h"
#include "Options.h"

void main(int argc, char *argv[]) {	
	//_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

	Options::initialize(argc, argv);

	Server server(Options::portNumber, INADDR_ANY, Options::nodelay);
	server.run();
}