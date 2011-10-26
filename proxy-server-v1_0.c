#include <stdio.h>
#include <winsock.h>
#include <string.h>
#include <stdlib.h>
#include <process.h>

// listen on port...
#define PORT 70
// handle listen in same time for ... connections 
#define CONNECTIONS -1 // -1 is unlimited

/* the proxy server get socket that connect (with SroAccept function)
 * and run in a new thread proxy server (with SroProxy function)
 * we need to transfer the socket from the main process to the new thread
 * we do it, with this public variable
 */
SOCKET SockNew = 0;

/* -------------------------- function: error
 * the function get message that contain discription of error
 * and if already is there a socket, the function get the socket for close
 * the function printf error message, [and close socket], and close thread.
 */
void error(char msg[], SOCKET Socket = -1)
	{
	fprintf(stderr, "ERROR! %d: %s\n", WSAGetLastError(), msg);
	if (Socket > 0)
		closesocket(Socket);
	_endthread();
	}

void SroWSAStartup(void)
	{
	WSADATA WsaDat;
	if (WSAStartup(MAKEWORD(1, 1), &WsaDat) != 0)
		error("SroWSAStartup - WSA Initialization failed.");
	}

int SroSocket(int domain = AF_INET, int type = SOCK_STREAM, int protocol = 0)
	{
	SOCKET Socket = socket(domain, type, protocol);
	if (Socket == INVALID_SOCKET)
		error("SroSocket - Socket creation failed.");
	return Socket;
	}

SOCKADDR_IN SroConnect(SOCKET Socket, int addr = INADDR_ANY, int port = 80, int family = AF_INET)
	{
	SOCKADDR_IN SockAddr;
	SockAddr.sin_family = family;
	SockAddr.sin_port = htons(port);
	SockAddr.sin_addr.s_addr = addr;
	if (connect(Socket, (sockaddr *)&SockAddr, sizeof(SOCKADDR)))
		error("SroConnect - connection failde.", Socket);
	return SockAddr;
	}

SOCKADDR_IN SroBind(SOCKET Socket, int addr = INADDR_ANY, int port = 80, int family = AF_INET)
	{
	SOCKADDR_IN SockAddr;
	SockAddr.sin_family = family;
	SockAddr.sin_port = htons(port);
	SockAddr.sin_addr.s_addr = addr;
	if (bind(Socket, (SOCKADDR *)(&SockAddr), sizeof(SockAddr)) == SOCKET_ERROR)
		error("SroBind - Attempt to bind failed.", Socket);		
	return SockAddr;
	}

void SroListen(SOCKET Socket, int connections = -1)
	{
	if (listen(Socket, connections) == SOCKET_ERROR)
		error("SroListen - Attempt to listen failed.", Socket);
	}

SOCKET SroAccept(SOCKET Socket, SOCKADDR_IN *SockAddr)
	{
	SOCKET TempSock = SOCKET_ERROR;
	int len = sizeof(SOCKADDR);
	while (TempSock == SOCKET_ERROR)
		TempSock = accept(Socket, (SOCKADDR *)&SockAddr, &len);
	return TempSock;
	}

// pull out the host-name from the request
char * SroPullouthostname(char *request, SOCKET Socket)
	{
	// we must to copy request to host,
	// because if not, strchr change the destination of the pointer request!
	// and we need it later on main function.
	char *host = (char *)malloc(sizeof(char) * strlen(request));
	// we need to to remember the plase of host, for free the memory!
	char *hostForFree = host;
	// the requset contain the host in the secondery line, after the word "Host: " (6 chars)
	host = strchr(strcpy(host, request), '\n') + 7; // cut until after the word "Host: "
	if (host == NULL)
		error("SroPullouthostname - The request is invalid, the request is only one line.", Socket);

	// put \0 [end string] after the host name. '\r' and '\n' is new line, ':' using to port
	int i = 0;
	while (host[i] != '\n' && host[i] != '\r' && host[i] != ':')
		if (i == strlen(host))
			{
			free(hostForFree);
			error("SroPullouthostname - The host name don't have finish.", Socket);
			}
		else
			i++;
	host[i] = '\0';
	free(hostForFree);
	return host;
	}

char * SroGethostbyname(char *host, SOCKET Socket)
	{
	struct hostent *h = gethostbyname(host);
	if (h == NULL)
		error("SroGethostbyname - gethostbyname failed.", Socket);
	return inet_ntoa(*((struct in_addr *)h->h_addr));
	}

void SroProxy(void * Pid)
	{
	// copy SockNew, and free him for the next thread
	SOCKET Socket = SockNew;
	SockNew = 0;

	// read the request from socket
	char request[2048];
	int requestLen = recv(Socket, request, sizeof(request) - 1, 0);

	// for debugging
	printf("\r\n\r\n\r\n--- new request\r\n");
	// put \0 after request, because otherwise it's printf also the old request
	// but in socket isn't there this problem, because we set how many long is the string
	request[requestLen] = '\0';
	printf("socket <%d> long of request: %d\r\n\r\n%s", Socket, requestLen, request);

	// pull out host name from requset
	char *host = SroPullouthostname(request, Socket);
	// get host by name (DNS request)
	host = SroGethostbyname(host, Socket);

	// create new Socket for read page
	SOCKET SockRead = SroSocket();
	// connect to host
	SroConnect(SockRead, inet_addr(host));
	// send the request are got
	send(SockRead, request, requestLen, 0);

	// for debugging
	printf("read from: %s\r\n", host);
	printf("size of package:");

	while (requestLen > 0)
		{
		// receive package, and immediately send
		requestLen = recv(SockRead, request, sizeof(request) - 1, 0);
		send(Socket, request, requestLen, 0);

		// for debugging
		printf(" %d |", requestLen, sizeof(request));

		// check from error
		if (requestLen == SOCKET_ERROR || requestLen == WSAECONNRESET || requestLen == WSAECONNABORTED)
			error("Connection closed at other end.");
		}

	closesocket(SockRead);
	closesocket(Socket);
	}

int main(int argc, char *argv[])
	{
	// this while is unending, its open the program with additional parameter at "child-process"
	// when the "child-process" (it's not realy child-process) abort, it's start additional "child-process".
	// dont forget, DOS is OS that run only one program concurrently.
	// therefore, the "main-process" wait until the "child-process" abort
	// and only then, continue in the while and start the "child-process" again.
	char * filename = strrchr(argv[0], '\\') + 1;
	if (argc == 1) // the first argument is the path & filename self
		while (TRUE)
			system(strcat(filename, " child-process"));

	printf("\r\n\r\n");
	printf("\t\t+---------------------------------------+\r\n");
	printf("\t\t|                                       |\r\n");
	printf("\t\t|        P R O X Y - S E R V E R        |\r\n");
	printf("\t\t|      ~~~~~~~~~~~~~~~~~~~~~~~~~~~      |\r\n");
	printf("\t\t|                                       |\r\n");
	printf("\t\t| Written by: Sro [http://sroblog.tk]   |\r\n");
	printf("\t\t|                                       |\r\n");
	printf("\t\t| Date: 29 May 2010                     |\r\n");
	printf("\t\t|                                       |\r\n");
	printf("\t\t| Version: 1.0                          |\r\n");
	printf("\t\t|                                       |\r\n");
	printf("\t\t+---------------------------------------+\r\n\r\n");

	// create socket for listen and start listen
	// WSAStartup
	SroWSAStartup();
	// create Socket
	SOCKET SockListen = SroSocket();
	// Bind with port
		// INADDR_ANY is for all IP addresses
		// for specific IP address, use the function inet_addr
		// example: inet_addr("127.0.0.1")
	SOCKADDR_IN SockAddr = SroBind(SockListen, INADDR_ANY, PORT);
	// start Listen
	SroListen(SockListen, CONNECTIONS);

	// main loop
	while(TRUE)
		{
		// wait until the last new thread catch the sock from public variable
		// after the thread (SroProxy function) catch, he change the variable to 0
		while (SockNew != 0) {}
		// Accept incoming connections
		SockNew = SroAccept(SockListen, &SockAddr);
		// perform proxy server in a new thread :)
		_beginthread(SroProxy, 0, NULL);
		}
		
	closesocket(SockListen);
	printf("\r\nDONE");
	return (EXIT_SUCCESS);
	}
