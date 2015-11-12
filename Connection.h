#ifndef CONNECTION_H
#define CONNECTION_H

#include "Socket.h"

class Connection
{
	//contains mutual data end algorithms for server and client
protected:
	//file r/w buffer
	vector<char> _buffer;

	int _timeOut;
	int _sendBufLen;
	//comands and their functions map(dictionary)
	//реализуемые этими командами (const char* -> std::function<void(string)>)
	CommandMap _commandMap;

	//id of the client or server
	int _id;

	//service packages confirm/refuse previous operation
	static const string _confirmMessage;
	static const string _refuseMessage;

	virtual void fillCommandMap() = 0;

public:
	Connection(int sendBufLen, int timeOut) : _sendBufLen(sendBufLen),
		_timeOut(timeOut)
	{
		_id = generateId<int>(0, std::numeric_limits<int>::max());
	}

	virtual ~Connection() {}

protected:

	bool sendConfirm(Socket& s)
	{//previos op confirmation
		return s.sendMessage(const_cast<string&>(_confirmMessage));
	}
	bool sendRefuse(Socket& s)
	{//previos op refutation
		return s.sendMessage(const_cast<string&>(_refuseMessage));
	}
	bool getAck(Socket& s)
	{//get confirm or refuse ->return true,false 
		size_t pos = s.receiveMessage().find(_confirmMessage);
		return  pos != string::npos ? true : false;
	}


	bool catchCommand(string request)
	{
		//identifies command from request
		string command = cutSuitableSubstring(request, "[A-Za-z0-9]+");
		//check command
		if (checkCommandExistance(command))
		{	
			//command execution
			_commandMap[command](request);		
			return true;
		}
		//there is no such command
		return false;
	}

	static bool checkStringFormat(const string& message, const string& pattern)
	{//regexp match
		std::regex regExp(pattern);
		return std::regex_match(message, regExp);
	}

	bool checkCommandExistance(const string& command)
	{
		//command existance check
		auto it = _commandMap.find(command);
		return it != _commandMap.end();
	}
	//--------------------------find substring-----------------------------//
	static std::string cutSuitableSubstring(string &message, const string& pattern)
	{//cut first matching substring from message
		std::regex regExp(pattern);
		std::smatch matches;
		std::regex_search(message, matches, regExp);
	
		string result = matches.empty() ? string("") : matches[0];
		message = matches.suffix().str();

		return result;
	}

	static std::string getFirstPatternedSubstring(const string &message, const string& pattern)
	{//get first substring mathing to pattern 
		std::regex regExp(pattern);
		std::smatch matches;
		std::regex_search(message, matches, regExp);
		
		return matches.empty() ? string("") : matches[0];
	}

	//---------------------------------работа с файлами----------------------------------------//
	int getFileLength(std::ifstream& file)
	{

		//cursor to the end of file
		file.seekg(0, ios::end);
		//get it position
		int fileEndPos = file.tellg();
		//cursor to the beginning
		file.seekg(0, ios::beg);
		//file length
		return fileEndPos;
	}


	bool prepareSocketForDataTransmission(Socket* socket)
	{
		//set send timeout
		if (!socket->setSendTimeOut(_timeOut >> 2)) return false;	//(/4)

		if(!socket->setSendBufferSize(_sendBufLen)) return  false;

	}

	bool prepareClientForDataTransmission(Socket* socket,int bufferSize,int fileLength)
	{
		//send  sendBufferSize
		if(!socket->send(bufferSize)) return false;
		if(!socket->send(_timeOut)) return  false;
		//send total file length
		return socket->send(fileLength);
	}

	bool tryToRestoreConnectionFromTransmittingSide(Socket*& socket,std::ifstream& file,std::function<Socket*(int)>& tryToReconnect,int timeOut)
	{
		int clientBytesReceived = 0;

		if ((socket = tryToReconnect(timeOut)) == nullptr)
		{	   
			file.close();
			socket->disableSendTimeOut();
			return false;
		}

		prepareSocketForDataTransmission(socket);
		//get bytes number that client managed to get
		socket->receive(clientBytesReceived);
	
		file.seekg(clientBytesReceived, ios::beg);

		         
		if (file.eof())
			file.clear();
		return true;
	}

	bool sendFile(Socket* socket, string& message, std::function<Socket*(int)> tryToReconnect)
	{    
		string fileName = getFirstPatternedSubstring(message, "[A-Za-z0-9]+.[A-Za-z0-9]+");  
		std::ifstream file;
		file.open(fileName, ios::in | ios::binary);

		if (!file.is_open())
		{  
			sendRefuse(*socket);
			return false;
		}
		else  
			sendConfirm(*socket);

		prepareSocketForDataTransmission(socket);
		int bufLen = socket->getSendBufferSize();

		int fileLength = getFileLength(file);

		prepareClientForDataTransmission(socket,bufLen,fileLength);
  
		if (_buffer.size() < _sendBufLen)
			_buffer.resize(_sendBufLen);
		int fileByteRead = 0;
		int bytesWrite = 0;
		//position read from, when connection was failed
		int clientBytesReceived = 0;
     
		while (true)
		{
			try { 
				file.read(_buffer.data(), bufLen);
				fileByteRead = file.gcount();
				if (!file.eof() && bufLen != fileByteRead)
					return false;

				bytesWrite = socket->send(_buffer.data(), fileByteRead);

				if (bytesWrite == SOCKET_ERROR)
					throw runtime_error("connection is lost");

				if (file.eof())
				{
					socket->setSendTimeOut(_timeOut >> 1);	//( timeout / 2 )
					//check bytes that client has received
					socket->receive(clientBytesReceived);
					if (clientBytesReceived == fileLength)
						break;
					else
					{
						socket->setSendTimeOut(_timeOut);
						throw runtime_error("connection is lost");
					}
				}
			}
			catch (exception e)
			{
				if(!tryToRestoreConnectionFromTransmittingSide(socket,file,tryToReconnect,_timeOut << 1)) break;
			}
		}

		file.close();
		socket->disableSendTimeOut();

		return true;
	}


	bool tryToRestoreConnectionFromReceivingSide(Socket*& socket, std::ofstream& file, std::function<Socket*(int)>& tryToReconnect, int timeOut, int bytesReceived)
	{
		if ((socket = tryToReconnect(_timeOut)) == nullptr)
		{
			file.close();
			socket->disableReceiveTimeOut();
			return false;
		}

		socket->send(bytesReceived);
		return socket->setReceiveTimeOut(_timeOut);
	}

	bool receiveFile(Socket* socket, string& message, std::function<Socket*(int)> tryToReconnect)
	{
		//waiting for acknowledge
		if (!getAck(*socket))
		{//there is no such file
			cout << "there is no such file" << endl;
			return false;
		}
		//get file name
		string fileName = getFirstPatternedSubstring(message, "[A-Za-z0-9]+.[A-Za-z0-9]+");
		std::ofstream file;
		file.open(fileName, ios::out | ios::trunc | ios::binary);

		if (!file.is_open())
			//can't create file
			return false;

		//size of data portion
		int bufLen;
		if (!socket->receive(bufLen)) return false;
		int timeOut;
		if (!socket->receive(timeOut)) return false;
		//get file length
		int fileLength;
		if (!socket->receive(fileLength)) return false;

		//set receive timeout
		if (!socket->setReceiveTimeOut(timeOut)) return false;

		if (_buffer.size() < bufLen)
			_buffer.resize(bufLen);
		int bytesRead = 0;

		//realy received file length
		int bytesReceived = 0;
		//file writing
		while (true)
		{
			try
			{

				//port reading
				bytesRead = socket->receive(_buffer.data(), bufLen);

				if (bytesRead == SOCKET_ERROR)
					throw runtime_error("connection is lost");
				else if (bytesRead == 0)
					//connection close
					break;
				//file writing
				file.write(_buffer.data(), bytesRead);
				bytesReceived += bytesRead;

				//end of transmittion check
				if (bytesReceived == fileLength)
				{//file uploaded
				 //transmit to server bytes number that has received
					socket->send(bytesReceived);
					break;
				}
			}
			catch (exception e)
			{
				if (!tryToRestoreConnectionFromReceivingSide(socket, file, tryToReconnect, timeOut << 1, bytesReceived)) break;
			}
		}

		file.close();
		socket->disableReceiveTimeOut();

		return fileLength == bytesReceived;
	}

	template<typename T>
	T generateId(int lowerBound = 0, int upperBound = 255)
	{
		std::default_random_engine generator;
		std::uniform_int_distribution<T> distribution(lowerBound, upperBound);
		generator.seed(time(NULL));
		return distribution(generator);
	}
};

//инициализация статических переменных
const string Connection::_confirmMessage = "confirm";
const string Connection::_refuseMessage = "refuse";

#endif //CONNECTION_H