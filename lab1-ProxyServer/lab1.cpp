#include <stdio.h>
#include <Windows.h>
#include <process.h>
#include <string.h>
#pragma comment(lib,"Ws2_32.lib")
#define MAXSIZE 65507
#define HTTP_PORT 80
//10.40.109.137
struct HttpHeader
{
    char method[4];
    char url[1024];
    char host[1024];
    char cookie[1024*10];
    HttpHeader()
    {
        ZeroMemory(this,sizeof(HttpHeader));
    }
};
BOOL InitSocket();
void ParseHttpHead(char * buffer,HttpHeader * httpHeader);
BOOL ConnectToServer(SOCKET* serverSocket,char * host);
unsigned int __stdcall ProxyThread(LPVOID lpParameter);
SOCKET ProxyServer;
sockaddr_in ProxyServerAddr;
const int ProxyPort=10240;
const int ProxyThreadMaxNum=1000;
HANDLE ProxyThreadHandle[ProxyThreadMaxNum]={0};
DWORD ProxyThreadDW[ProxyThreadMaxNum]={0};
struct ProxyParam
{
    SOCKET clientSocket;
    SOCKET serverSocket;
};
int main(int argc,_TCHAR* argv[])
{
    printf("Proxy Is Booting.\nInitializing.\n");
    if(!InitSocket())
    {
        printf("Socket Init Fail.\n");
        return -1;
    }
    printf("Proxy Is Running.\nMonitor Port: %d.\n",ProxyPort);
    SOCKET acceptSocket=INVALID_SOCKET;
    ProxyParam *lpProxyParam;
    HANDLE htThread;
    DWORD dwThreadID;
    while(true)
    {
        acceptSocket=accept(ProxyServer,NULL,NULL);
        lpProxyParam=new ProxyParam;
        if(lpProxyParam==NULL)
        {
        	continue;
        }
        lpProxyParam->clientSocket=acceptSocket;
        htThread=(HANDLE)_beginthreadex(NULL,0,&ProxyThread,(LPVOID)lpProxyParam,0,0);
        CloseHandle(htThread);
        //Sleep(200);
    }
    closesocket(ProxyServer);
    WSACleanup();
    return 0;
}
BOOL InitSocket()
{
    WORD wVersionRaquested;
    WSADATA wasData;
    int err;
    wVersionRaquested=MAKEWORD(2,2);
    err=WSAStartup(wVersionRaquested,&wasData);
    if(err!=0)
    {
        printf("Load Winsock Fail, Error Code Is %d.\n",WSAGetLastError());
        return FALSE;
    }
    if(LOBYTE(wasData.wVersion)!=2||HIBYTE(wasData.wVersion)!=2)
    {
    	printf("Cannot Find Correct Winsock Version\n");
    	WSACleanup();
    	return FALSE;
    }
    ProxyServer=socket(AF_INET,SOCK_STREAM,0);
    if(INVALID_SOCKET==ProxyServer)
    {
    	printf("Create Socket Failed, Error Code Is %d.\n",WSAGetLastError() );
    	return FALSE;
    }
    ProxyServerAddr.sin_family=AF_INET;
    ProxyServerAddr.sin_port=htons(ProxyPort);
    ProxyServerAddr.sin_addr.S_un.S_addr=INADDR_ANY;
    if(bind(ProxyServer,(SOCKADDR*)&ProxyServerAddr,sizeof(SOCKADDR))==SOCKET_ERROR)
    {
    	printf("Create Socket Failed.\n");
    	return FALSE;
    }
    if(listen(ProxyServer,SOMAXCONN)==SOCKET_ERROR)
    {
    	printf("Monitoring Port %d Failed.\n",ProxyPort);
    	return FALSE;
    }
}
unsigned int __stdcall ProxyThread(LPVOID lpParameter)
{
	char Buffer[MAXSIZE];
	char *CacheBuffer;
	ZeroMemory(Buffer,MAXSIZE);
	SOCKADDR_IN clientAddr;
	int length=sizeof(SOCKADDR_IN);
	int recvSize;
	int ret;
	recvSize=recv(((ProxyParam*)lpParameter)->clientSocket,Buffer,MAXSIZE,0);
	HttpHeader* httpHeader=new HttpHeader();
	if(recvSize<=0)
	{
		goto error;
	}
	CacheBuffer=new char[recvSize+1];
	ZeroMemory(CacheBuffer,recvSize+1);
	memcpy(CacheBuffer,Buffer,recvSize);
	ParseHttpHead(CacheBuffer,httpHeader);
	delete CacheBuffer;
	if(!ConnectToServer(&((ProxyParam*)lpParameter)->serverSocket,httpHeader->host))
	{
		goto error;
	}
	printf("Proxy Connect To The Main Server.\n");
	ret=send(((ProxyParam*)lpParameter)->serverSocket,Buffer,strlen(Buffer)+1,0);
	recvSize=recv(((ProxyParam*)lpParameter)->serverSocket,Buffer,MAXSIZE,0);
	if(recvSize<=0)
	{
		goto error;
	}
	ret=send(((ProxyParam*)lpParameter)->clientSocket,Buffer,sizeof(Buffer),0);
error:
	printf("Close Socket.\n");
	//Sleep(200);
	closesocket(((ProxyParam*)lpParameter)->clientSocket);
	closesocket(((ProxyParam*)lpParameter)->serverSocket);
	//delete lpParameter;
	_endthreadex(0);
	return 0;
}
void ParseHttpHead(char *buffer,HttpHeader * httpHeader)
{
    int i;
	char * p;
	char * ptr;
	const char * delim="\r\n";
	p=strtok(buffer,delim);
	printf("%s\n",p);
	if(p[0]=='G')
	{
		memcpy(httpHeader->method,"GET",3);
		memcpy(httpHeader->url,&p[4],strlen(p)-13);
	}
	else if(p[0]=='P')
	{
		memcpy(httpHeader->method,"POST",4);
		memcpy(httpHeader->url,&p[5],strlen(p)-14);
	}
	///*
    for(i=0;i<strlen(httpHeader->url);i++)
    {
        if(httpHeader->url[i]=='s')
            if(httpHeader->url[i+1]=='o')
                if(httpHeader->url[i+2]=='g')
                    if(httpHeader->url[i+3]=='o')
                        if(httpHeader->url[i+4]=='u')
                            return;
    }
    //*/
    /*
    if(!strcmp(httpHeader->url,"http://www.sohu.com/"))
        strcpy(httpHeader->url,"http://www.sogou.com/");
    //*/
    printf("%s\n",httpHeader->url);
	p=strtok(NULL,delim);
	while(p)
	{
		switch(p[0])
		{
			case 'H':
				memcpy(httpHeader->host,&p[6],strlen(p)-6);
				break;
			case 'C':
				if(strlen(p)>8)
				{
					char header[8];
					ZeroMemory(header,sizeof(header));
					memcpy(header,p,6);
					if(!strcmp(header,"Cookie"))
					{
						memcpy(httpHeader->cookie,&p[8],strlen(p)-8);
					}
				}
				break;
			default:
				break;
		}
		p=strtok(NULL ,delim);
	}
}
BOOL ConnectToServer(SOCKET * serverSocket,char * host)
{
	sockaddr_in serverAddr;
	serverAddr.sin_family=AF_INET;
	serverAddr.sin_port=htons(HTTP_PORT);
	HOSTENT *hostent=gethostbyname(host);
	if(!hostent)
	{
		return FALSE;
	}
	in_addr Inaddr=*((in_addr*)*hostent->h_addr_list);
	serverAddr.sin_addr.s_addr=inet_addr(inet_ntoa(Inaddr));
	*serverSocket=socket(AF_INET,SOCK_STREAM,0);
	if(*serverSocket==INVALID_SOCKET)
	{
		return FALSE;
	}
	if(connect(*serverSocket,(SOCKADDR*)&serverAddr,sizeof(serverAddr))==SOCKET_ERROR)
	{
		closesocket(*serverSocket);
		return FALSE;
	}
	return TRUE;
}
