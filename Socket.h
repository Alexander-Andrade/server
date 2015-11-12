#ifndef MYSOCKET_H
#define MYSOCKET_H

#include "Includes.h"

struct InetAddress
{// represents an Internet Protocol (IP) address.
 //port
	std::string port;
	//IP
	std::string IP;

	InetAddress(std::string& IP, unsigned short port)
	{
		this->IP = IP;
		this->port = toString(port);
	}
	InetAddress(std::string& IP, std::string& port)
	{
		this->IP = IP;
		this->port = port;
	}
	InetAddress(sockaddr_in& addr)
	{
		getNameInfo((sockaddr*)&addr, IP, port);
	}
	InetAddress() {}
	bool operator==(InetAddress& inetAddress)
	{//сравнение адресов
		return (IP == inetAddress.IP) && (port == inetAddress.port);
	}
	static bool getNameInfo(sockaddr* pSockAddr, string& hostName, string& port)
	{
		//protocol - independent name resolution from an address to an ANSI host name and from a port number to the ANSI service name.
		//NI_MAXHOST - 1025
		char nodeBuffer[NI_MAXHOST] = "";
		//NI_MAXSERV - 32
		char serviceBuffer[NI_MAXSERV] = "";
		//NI_NUMERICHOST flag returns the numeric form of the host name instead of its name.
		//NI_NUMERICSERV flag returns the port number of the service instead of its name.
		int retVal = getnameinfo(pSockAddr, sizeof(sockaddr), nodeBuffer, NI_MAXHOST, serviceBuffer, NI_MAXSERV, NI_NUMERICHOST | NI_NUMERICSERV);

		hostName = nodeBuffer;
		port = serviceBuffer;

		//On success, getnameinfo returns zero
		return (retVal == 0) ? true : false;
	}
};

class Socket
{
public:
	enum class Selection { ReadCheck, WriteCheck };
protected:
	//socket handle
	SOCKET _handle;
	//The addrinfo structure is used by the getaddrinfo function to hold host address information.
	addrinfo *_result;
	
	//The family, socktype and proto arguments can be optionally
	//specified in order to narrow the list of addresses returned.
	addrinfo _hints;
	//address socket connected to (not localmashine)
	InetAddress _inetAddress;

	u_long _keepAliveTimeOut;
	u_long _keepAliveInterval;

	size_t _messageMaxSize;
	//запрет копирования и присваивания
	Socket(Socket& s);
	Socket& operator=(Socket& s);
public:


	Socket(char* IP, char* port)
	{
		socketSettings(IP, port);
	}

	Socket(SOCKET& handle, InetAddress& inetAddr)
	{//созд сокет из вызова accept
		socketSettings(const_cast<char*>(inetAddr.IP.c_str()), const_cast<char*>(inetAddr.port.c_str()), handle);

	}
	Socket() { socketSettings(); }

	//вернуть адрес ,к которому сокет привязан
	const InetAddress& address() { return _inetAddress; }

	const std::string& IP()const { return _inetAddress.IP; }
	const std::string& port()const { return _inetAddress.port; }

	u_long keepAliveTimeOut()const { return _keepAliveTimeOut; }
	u_long keepAliveInterval()const { return _keepAliveInterval; }

	void resetHande() { _handle = INVALID_SOCKET; }
	//дескриптор сокета
	SOCKET handle() { return _handle; }

	virtual ~Socket()
	{
		freeAddrInfo();
		shutDown();
		closeSocket();
	}

	void closeSocket()
	{
		if (_handle != INVALID_SOCKET)
		{
#if defined(WINDOWS)
			closesocket(_handle);
#elif defined(UNIX)
			close(_handle);
#endif
			_handle = INVALID_SOCKET;
		}
	}
	bool shutDown()
	{
		//then not more send data
		return shutdown(_handle, SD_SEND) != SOCKET_ERROR;
	}
	static bool initializeWinsock_()
	{
		int result = 0;
#if defined(WINDOWS)
		//The WSADATA structure contains information
		//about the Windows Sockets implementation.
		WSADATA wsaData;
		// initialize winsock library process wsock32.dll
		//If successful, the WSAStartup function returns zero. Otherwise,
		//it returns one of the error codes
		result = WSAStartup(SOCKET_V2, //The highest version of Windows Sockets specification that the caller can use
			&wsaData  //data structure that is to receive details of the Windows Sockets implementation.
			);
		if (result != 0)
			throw new runtime_error("WSAStartup failed");
#endif
		return result;
	}

	//---------------------------send data and messages---------------------------------//
	bool sendMessage(string& message)
	{// sending message ends with \r\n
		if (message.empty() || message.back() != '\n')
			message.append("\r\n");
		return send(message.c_str(), message.length()) != SOCKET_ERROR;
	}

	bool sendMessage(const char* message)
	{
		std::string mes(message);
		return sendMessage(mes);
	}

	int send(const char* buffer, int length)
	{
		int flags = 0;
#if defined(UNIX)
		flags = MSG_NOSIGNAL;
#endif
		return ::send(_handle, buffer, length, flags);
	}
	int receive(char* buffer, int length)
	{
		return recv(_handle, buffer, length, 0);
	}

	template<typename T>
	bool send(T& obj)
	{
		int length = sizeof(obj);
		return send((char *)&obj, length) == length;
	}

	template <typename T>
	bool receive(T& obj)
	{
		int length = sizeof(obj);
		return receive((char*)&obj, length) == length;
	}

	string receiveMessage()
	{//nothrows
		string message;
		message.reserve(_messageMaxSize);
		//кол-во принятых байт
		int bytesAccepted = 0;
		char data = 0;
		while (true)
		{
			bytesAccepted = recv(_handle, &data, 1, 0);

			if (bytesAccepted == 0 || bytesAccepted == SOCKET_ERROR)
			{
				//connection has been gracefully closed or обрыв соединения
				return message;
			}

			//запись данных
			message.push_back(data);
			//если это были последние данные, выход
			if (data == '\n')
				break;

		}

		return message;
	}

	string receiveMessage_()
	{//throw runtime_error
		string message;
		message.reserve(_messageMaxSize);
		//кол-во принятых байт
		int bytesAccepted = 0;
		char data = 0;
		while (true)
		{
			bytesAccepted = recv(_handle, &data, 1, 0);

			if (bytesAccepted == 0)
			{
				//connection has been gracefully closed
				throw runtime_error("connection close");
			}
			else if (bytesAccepted == SOCKET_ERROR)
				//disconnection
				socketError("disconnection");
			//data writing
			message.push_back(data);
			//if last data portion->exit
			if (data == '\n')
				break;

		}

		return message;
	}


	bool isValid()
	{//socket check
		return _handle != INVALID_SOCKET;
	}

	bool setBlocking(unsigned int blockMode)
	{//set/reset blocking mode of socket
	 //(nonblockingIO) a nonzero value if the nonblocking mode should be enabled
	 //or zero if the nonblocking mode should be disabled.
		unsigned long  arg = blockMode;
		int retVal = ioctlSocket(_handle,
			FIONBIO,	//set/clear nonblocking i/o
			&arg);
		return retVal != SOCKET_ERROR;
	}

	bool makeUnblocked()
	{//make socket unblocked
		return setBlocking(1);
	}

	bool makeBlocked()
	{//make socket blocked
		return setBlocking(0);
	}

	bool select(Selection selection, int connectionTimeOut = 30)
	{
	 /*
	 The select function returns the total number of socket handles
	 that are ready and contained in the fd_set structures,
	 zero if the time limit expired, or SOCKET_ERROR if an error occurred.
	 */

		timeval timeOut;	
		timeOut.tv_usec = 0;	
		timeOut.tv_sec = connectionTimeOut;   

		fd_set set;	

		FD_ZERO(&set);	
		FD_SET(_handle, &set);	    
		int retVal = 0;
		if (selection == Selection::WriteCheck)
			retVal = ::select(_handle + 1,	//Ignored. The nfds parameter is included only for compatibility with Berkeley sockets.
				&set,//An optional pointer to a set of sockets to be checked for readability.
				NULL,//An optional pointer to a set of sockets to be checked for writability.
				NULL,//An optional pointer to a set of sockets to be checked for errors.
				&timeOut	//The maximum time for select to wait (TIMEVAL structure)
				);
		else if (selection == Selection::ReadCheck)
			retVal = ::select(_handle + 1, NULL, &set, NULL, &timeOut);

		return ((retVal != 0) && FD_ISSET(_handle, &set));
	}



	bool setKeepAliveTimeout(u_long keepAliveTime, u_long keepAliveInterval = 1)
	{
		// default keep-alive timeout = 2hours and keep-alive interval = 1 second
		// in windows the number of keep-alive probes (data retransmissions) is set to 10 and cannot be changed
#if defined(WINDOWS)
		winKeepAlive(1, keepAliveTime, keepAliveInterval);

#elif defined(UNIX)
		unixKeepAlive(keepAliveTime, keepAliveInterval);
#endif
		//проверка через getsockopt
		int optVal = 0;
		bool succeeded = false;
		succeeded = getSockOpt(SOL_SOCKET, SO_KEEPALIVE, optVal);

		if (succeeded && optVal)
		{
			_keepAliveTimeOut = keepAliveTime;
			_keepAliveInterval = keepAliveInterval;
			return true;
		}
		return false;
	}
	int getSendBufferSize()
	{//check buffer size
		int bufferSize = 0;
		getSockOpt(SOL_SOCKET, SO_SNDBUF, bufferSize);
#if defined (UNIX)
		return bufferSize >> 1;
#endif
        return bufferSize;
	}

	bool setSendBufferSize(int bufferSize)
	{
		//the total per-socket buffer space reserved for sends.
		return setSockOpt(SOL_SOCKET, SO_SNDBUF, bufferSize);

	}

	bool setReceiveBufferSize(int bufferSize)
	{
		return setSockOpt(SOL_SOCKET, SO_RCVBUF, bufferSize);

	}

	int getReceiveBufferSize()
	{
		int bufferSize = 0;
		getSockOpt(SOL_SOCKET, SO_RCVBUF, bufferSize);
#if defined (UNIX)
		return bufferSize >> 1;
#endif
        return bufferSize;
	}


	bool setReceiveTimeOut(int timeOutSec)
	{
		//if no data arrives during the period specified in SO_RCVTIMEO,
		//the recv function completes.
		//windows sets the timeout, in milliseconds, for blocking receive calls.
#if defined(WINDOWS)
		return setSockOpt(SOL_SOCKET, SO_RCVTIMEO, timeOutSec * 1000);
#elif defined(UNIX)
		timeval timeout;
		timeout.tv_sec = timeOutSec;
		timeout.tv_usec = 0;
		return setSockOpt(SOL_SOCKET, SO_RCVTIMEO, timeout);
#endif
	}
	bool disableReceiveTimeOut()
	{
#if defined(WINDOWS)
		return setSockOpt(SOL_SOCKET, SO_RCVTIMEO, 0);
#elif defined(UNIX)
		timeval timeout = { 0,0 };
		return setSockOpt(SOL_SOCKET, SO_RCVTIMEO, timeout);
#endif
	}
	bool setSendTimeOut(int timeOutSec)
	{
		//if no data arrives within the period specified in SO_RCVTIMEO,
		//the recv function returns WSAETIMEDOUT, and if data is received, recv returns SUCCESS.
		//The timeout, in milliseconds, for blocking send calls.
#if defined(WINDOWS)
		return setSockOpt(SOL_SOCKET, SO_SNDTIMEO, timeOutSec * 1000);
#elif defined(UNIX)
		timeval timeout;
		timeout.tv_sec = timeOutSec;
		timeout.tv_usec = 0;
		return setSockOpt(SOL_SOCKET, SO_SNDTIMEO, timeout);
#endif
	}

	bool disableSendTimeOut()
	{
#if defined(WINDOWS)
		return setSockOpt(SOL_SOCKET, SO_SNDTIMEO, 0);
#elif defined(UNIX)
		timeval timeout = { 0,0 };
		return setSockOpt(SOL_SOCKET, SO_SNDTIMEO, timeout);
#endif
	}

	bool reuseAddr()
	{
		/*
		!!!!!!!!!!!!!!
		When using bind with the SO_REUSEADDR socket option,
		the socket option must be set prior to executing bind to have any affect.
		*/
		//Allows the socket to be bound to an address that is already in use.
		//For more information, see bind. Not applicable on ATM sockets.
		return setSockOpt(SOL_SOCKET, SO_REUSEADDR, true);
	}

	static void closeWinsock()
	{
#if defined(WINDOWS)
		WSACleanup();
#endif
	}


protected:

	void socketSettings(char* IP = "", char* port = "", SOCKET handle = INVALID_SOCKET, size_t messageMaxSize = 128)
	{//default socket parameters 
		_handle = handle;
		_result = nullptr;
		
		_messageMaxSize = messageMaxSize;

		//IP-portNo
		_inetAddress.IP = IP;
		_inetAddress.port = port;

		//default send and receive socket system buffers size
		void setDefaultSendRecvBufSizes();

	}
	template<typename T>
	bool setSockOpt(int level, int optname, T optval)
	{
		return ::setsockopt(_handle, level, optname, &(*(char*)&optval), sizeof(optval)) == 0;
	}
	template<typename T>
	bool getSockOpt(int level, int optname, T &optval)
	{
		socklen_t optlen = sizeof(optval);
		return ::getsockopt(_handle, level, optname, &(*(char*)&optval), &optlen) == 0;
	}

	bool winKeepAlive(u_long onoff, u_long keepAliveTime, u_long keepAliveInterval)
	{//The WSAIoctl function controls the mode of a socket.
	 /*
	 Argument structure for SIO_KEEPALIVE_VALS
	 struct tcp_keepalive {
	 u_long  onoff;			//determines if TCP keep-alive is enabled or disabled
	 u_long  keepalivetime;	//specifies the timeout, in milliseconds, with no activity until the first keep-alive packet is sent
	 u_long  keepaliveinterval;	//specifies the interval, in milliseconds, between when successive keep-alive packets are sent if no acknowledgement is received.
	 };
	 */
		int retVal = 0;
#if defined (WINDOWS)

		u_long msInSec = 1000;
		u_long bytesReturned = 0;
		tcp_keepalive settings;
		settings.onoff = onoff;
		settings.keepalivetime = keepAliveTime * msInSec;
		settings.keepaliveinterval = keepAliveInterval * msInSec;

		retVal = WSAIoctl(_handle,
			SIO_KEEPALIVE_VALS,	//he control code of operation to perform.
			&settings,		//A pointer to the input buffer.
			sizeof(tcp_keepalive),
			NULL,	//A pointer to the output buffer.
			0,	//The size, in bytes, of the output buffer.
			&bytesReturned,	//A pointer to actual number of bytes of output.
			NULL,	//A pointer to a WSAOVERLAPPED structure (ignored for non-overlapped sockets).
			NULL	// A pointer to the completion routine called when the operation has been completed (ignored for non-overlapped sockets).
			);

#endif
		return retVal == 0;
	}

	bool unixKeepAlive(u_long time, u_long interval, u_long probes = 10)
	{
		/*
		Set TCP keepalive on an open socket.
		It activates after 1 second (after_idle_sec) of idleness,
		then sends a keepalive ping once every 3 seconds (interval_sec),
		and closes the connection after 5 failed ping (max_fails), or 15 seconds

		tcp_keepalive_time
		the interval between the last data packet sent (simple ACKs are not considered data) and the first keepalive probe; after the connection is marked to need keepalive, this counter is not used any further
		tcp_keepalive_intvl
		the interval between subsequential keepalive probes, regardless of what the connection has exchanged in the meantime
		tcp_keepalive_probes
		the number of unacknowledged probes to send before considering the connection dead and notifying the application layer
		*/
#if defined (UNIX)
		//enable
		if (!setSockOpt(SOL_SOCKET, SO_KEEPALIVE, 1)) return false;
		//configure
		if (!setSockOpt(SOL_TCP, TCP_KEEPIDLE, time)) return false;
		if (!setSockOpt(SOL_TCP, TCP_KEEPINTVL, interval)) return false;
		if (!setSockOpt(SOL_TCP, TCP_KEEPCNT, probes)) return false;
#endif
		return true;
	}

	bool socket(addrinfo* pAddrInfo)
	{
		//создание сокета int socket (domain, type, protocol)
		_handle = ::socket(pAddrInfo->ai_family, //используемый для взаимодействия набор протоколов (для стека протоколов TCP/IP)
			pAddrInfo->ai_socktype, // с установлением соединения
			pAddrInfo->ai_protocol //протокол транспортного уровня
								   //(из нескольких возможных в стеке протоколов).
								   //Если этот аргумент задан равным 0,
								   //то будет использован протокол "по умолчанию"
								   //(TCP для SOCK_STREAM и UDP для SOCK_DGRAM
								   //при использовании комплекта протоколов TCP/IP).
			);
		return _handle != INVALID_SOCKET;
	}

	//------------------------ закрытие объекта--------------------------------------//

	void freeAddrInfo()
	{
		//The freeaddrinfo function frees address information that the getaddrinfo
		//function dynamically allocates in addrinfo structures.
		//освобождаем список из структур addrinfo
		if (_result != nullptr)
			freeaddrinfo(_result);
	}


	//-----------------------------обёртки для вызовов функций сокетов-----------------------------//
	bool getAddrInfo(int family, int socktype, int protocol, int flags)
	{
		//создаёт прослушивающий сокет
		memset(&_hints, 0, sizeof(_hints));
		_hints.ai_family = family;
		_hints.ai_socktype = socktype;
		_hints.ai_protocol = protocol;
		_hints.ai_flags = flags;	//The socket address will be used in a call to the bind function.
									/*
									!!!!!!!!!! If the pIP parameter contains an empty string, all registered addresses on the local computer are returned.
									*/
									//The getaddrinfo function provides protocol-independent translation
									//from an ANSI host name to an address.
		int retValue = getaddrinfo(_inetAddress.IP.c_str(),	//a host (node) name or a numeric host address string.
			_inetAddress.port.c_str(), //A pointer to a NULL-terminated ANSI string that contains
									   //either a service name or port number represented as a string.
			&_hints,					//A pointer to an addrinfo structure that provides hints about the type of socket the caller supports.
			&_result					//A pointer to a linked list of one or more addrinfo structures that contains response information about the host.
			);
		//Success returns zero.
		return retValue == 0;
	}

	void getAddrInfo_(int family, int socktype, int protocol, int flags)
	{
		if (!getAddrInfo(family, socktype, protocol, flags))
			socketError("fail to get addrinfo");
	}

	static int errorCode()
	{
#if defined(WINDOWS)
		return GetLastError();
#elif defined(UNIX)
		return errno;
#endif
	}

	static int ioctlSocket(SOCKET handle, long command, unsigned long* pArg)
	{
#if defined(WINDOWS)
		return ioctlsocket(handle, command, pArg);
#elif defined(UNIX)
		return ioctl(handle, command, pArg);
#endif
	}


	//-------------------------обработка ошибок------------------------------------//
	static void socketError(const char* description)
	{

		string err_message = description + toString(errorCode());
		//The WSACleanup function terminates use of the Winsock 2 DLL (Ws2_32.dll).
		throw runtime_error(description);
	}

};

class ServerSocket : public Socket
{
private:
	//запрт присваивания
	ServerSocket(const ServerSocket&);
	ServerSocket& operator=(ServerSocket&);
protected:
	//размер очереди клиентов
	int _nConnections;
public:
	ServerSocket(char* IP, char* port, int nConnections = 5) : Socket(IP, port)
	{
		_nConnections = nConnections;
		getAddrInfo_(AF_INET,//family
			SOCK_STREAM,
			IPPROTO_TCP,
			AI_PASSIVE
			);

		socket_();
		bind_();
		listen_();
	}

	void socket_()
	{
		if (!socket(_result))
			socketError("socket failed with error: ");
	}

	bool bind()
	{//This function associates a local address with a socket.
		int retValue = ::bind(_handle,	//дескриптор сокета
			_result->ai_addr, //структура, содержащуя локальный адрес, приписываемый socket'у
			_result->ai_addrlen);
		return retValue != SOCKET_ERROR;
	}

	void bind_()
	{
		if (!bind())
			socketError("bind failed with error: ");
	}

	bool listen()
	{
		//Now we can start listening (allowing as many connections as possible to
		//be made at the same time using SOMAXCONN). You could specify any
		//integer value equal to or lesser than SOMAXCONN instead for custom
		//purposes). The function will not //return until a connection request is
		//made
		int retValue = ::listen(_handle, _nConnections);	//программы-сервера ожидает запросы к ней от программ-клиентов
		return retValue != SOCKET_ERROR;
	}
	void listen_()
	{
		if (!listen())
			socketError("listen failed with error: ");
	}

	Socket* accept()
	{
		sockaddr_in currentClientAddr;
		socklen_t clientAddrLen = sizeof(currentClientAddr);
		memset(&currentClientAddr, 0, clientAddrLen);
		/*Системный вызов accept извлекает из очереди,
		организованной системным вызовом listen,
		первый запрос на соединение и возвращает дескриптор нового
		(автоматически созданного) socket'а с теми же свойствами, что и socket,
		задаваемый аргументом s. Этот новый дескриптор необходимо использовать
		во всех последующих операциях обмена данными.*/
		SOCKET hClientSocket = ::accept(_handle, //дескриптор сокета,через который программа - сервер
												 // получила запрос на соединение
			(sockaddr*)&currentClientAddr,//содержит адрес socket'а программы-клиента,
										  //сделавшей запрос на соединение
			&clientAddrLen);

		InetAddress addr(currentClientAddr);
		Socket* pClientSocket = new Socket(hClientSocket, addr);
		return pClientSocket;
	}
};



class ClientSocket : public Socket
{
private:
	//запрет присваиваиня
	ClientSocket(ClientSocket&);
	ClientSocket& operator=(ClientSocket&);
public:
	ClientSocket(char* IP, char* port) : Socket(IP, port)
	{

		//заполнение структуры для создания сокета
		getAddrInfo_(AF_UNSPEC,	//the address family is unspecified.
			SOCK_STREAM,//Provides sequenced, reliable, two-way, connection-based byte
						//streams with an OOB data transmission mechanism.
			IPPROTO_TCP, //протокол TCP
			0);	//без флагов

		establishConnection_();
	}


	//-----------------------------обёртки для вызовов функций сокетов-----------------------------//

	bool connect(addrinfo* pAddrInfo)
	{//подключиться к серверу
		int retValue = ::connect(_handle, pAddrInfo->ai_addr, (int)pAddrInfo->ai_addrlen);
		if (retValue == SOCKET_ERROR)
		{
			closeSocket();
			_handle = INVALID_SOCKET;
			return false;
		}
		return true;
	}
	bool establishConnection()
	{

		//подключиться к серверу
		addrinfo* ptr;

		for (ptr = _result; ptr != NULL; ptr = ptr->ai_next)
		{
			// Create a SOCKET for connecting to server
			socket(ptr);
			//если соед установлено,выйти из цикла
			if (connect(ptr))
				break;
		}

		//если не подключились к серверу
		return _handle != INVALID_SOCKET;
	}
	
	void establishConnection_()
	{
		if (!establishConnection())
			socketError("Unable to connect to server");
	}

};


#endif //SOCKET_H
