#include "Includes.h"
#include "server.h"


int main(int argc,char* argv[])
{

	try
	{
		Socket::initializeWinsock_();

		Server server(argv[1],argv[2]);
		server.workWithClients();
		

	}
	catch (exception e)
	{
		cout << e.what() << endl;
		getchar();
	}
	Socket::closeWinsock();
	return 0;
}
