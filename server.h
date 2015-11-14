#ifndef SERVER_H
#define SERVER_H

#include "Connection.h"

class Server : public Connection
{
private:
	unique_ptr<ServerSocket> _serverSocket;
	unique_ptr<Socket> _contactSocket;
	    
	std::queue<int> _clients;
public:
	Server(char* nodeName, char* serviceName, int nConnections = 5, int sendBufLen = 3000, int timeOut = 30) : Connection(sendBufLen,timeOut)
	{//ethernet frame = 1460 bytes
		_serverSocket.reset(new ServerSocket(nodeName,serviceName, nConnections));
		_contactSocket = nullptr;
		
		fillCommandMap();
		
	}
   
	void workWithClients()
	{   
		while (true)
		{
			//try to accept()
			if (acceptNewClient())
				//interact with new client
				clientCommandsHandling();
		}
	}

protected:

	virtual void clientCommandsHandling()
	{
		while (true)
		{
			string message = _contactSocket->receiveMessage();
			if (message.empty()) break;
	
			if (!checkStringFormat(message, "( )*[A-Za-z0-9]+(( )+(.)+)?(\r\n|\n)"))
			{  
                std::string errorMessage = string("invalid command format \"") + message;
				_contactSocket->sendMessage(errorMessage);
			}

			if (!catchCommand(message))
			{
				_contactSocket->sendMessage("unknown command");
				continue;
			}
			
			if (std::regex_search(message, std::regex("quit|exit|close")))
				break;

		}
	}

	//---------------------------------  ----------------------------------------//

	bool sendFile(string& message)
	{
		bool retVal = Connection::sendFile(_contactSocket.get(), message, std::bind(&Server::tryToReconnect, this, std::placeholders::_1));
	  
		_contactSocket->receiveAck();
	       
		retVal ? _contactSocket->sendMessage("file downloaded\n") : _contactSocket->sendMessage("fail to download the file\n");
		return retVal;
	}
	bool receiveFile(string& message)
	{
		bool retVal = Connection::receiveFile(_contactSocket.get(), message, std::bind(&Server::tryToReconnect, this, std::placeholders::_1));
		retVal ? _contactSocket->sendMessage("file uploaded\n") : _contactSocket->sendMessage("fail to upload the file\n");
		return retVal;
	}
	void registerNewClient(int clientId)
	{
		if (_clients.size() == 2)
			_clients.pop();
		_clients.push(clientId);
	}
	bool acceptNewClient()
	{
		_contactSocket.reset(_serverSocket->accept());
	  
		bool result = _contactSocket->handle() != INVALID_SOCKET;
		if (result)
		{
			int clientId;
			_contactSocket->receive(clientId);
			registerNewClient(clientId);
		}
		return result;
	}

	Socket* tryToReconnect(int timeOut)
	{    
	 
		if (!_serverSocket->makeUnblocked())
			return nullptr;
		//fcntl(_serverSocket->handle(),F_SETFL,O_NONBLOCK);
		if (!_serverSocket->select(Socket::Selection::WriteCheck ,timeOut))
		{	
			_serverSocket->makeBlocked();
			return nullptr ;
		}

		if (!_serverSocket->makeBlocked())
			return nullptr;

		acceptNewClient();
	
		if (_clients.front() == _clients.back())
			return _contactSocket.get();
	
		return nullptr;
	}

	//-----------------------------------(),  ------------------------------//

	
	bool echo(string& message)
	{
		cutSuitableSubstring(message, "( )+");
		return _contactSocket->sendMessage(message);
	}
	
	bool quit(string& message)
	{
		 //bool result = _contactSocket->shutDown();
		//_contactSocket->closeSocket();
		_contactSocket.reset();
		return true;
	}
	bool time(string& message)
	{
		time_t curTime;
		curTime = std::time(NULL);
		return _contactSocket->sendMessage(std::ctime(&curTime));
	}

	void fillCommandMap() override
	{
		
		_commandMap[string("echo")] = std::bind(&Server::echo, this, std::placeholders::_1);
		_commandMap[string("time")] = std::bind(&Server::time, this, std::placeholders::_1);
		_commandMap[string("quit")] = std::bind(&Server::quit, this, std::placeholders::_1);
		
		_commandMap[string("download")] = std::bind(&Server::sendFile, this, std::placeholders::_1);
		_commandMap[string("upload")] = std::bind(&Server::receiveFile, this, std::placeholders::_1);
	}



};


#endif //SERVER_H
