#include"client.h"
// 服务端和客户端的ip地址
sockaddr_in serverAddr, clientAddr;
// 客户端socket
SOCKET sock;
// ip地址长短
unsigned int addr_len;
unsigned long Opt = 1;
double transByte, consumeTime;
FILE* logFp;
char logBuf[512];
time_t rawTime;
tm* info;

void main() {
	// 获取时间
	time(&rawTime);
	// 转化为当地时间
	info = localtime(&rawTime);
	// 打开日志文件
	logFp = fopen("tftp.log", "a");
	if (logFp == NULL) {
		cout << "open log file failed" << endl;
	}
	printf("当前的本地时间和日期：%s", asctime(info));
	char clientIP[20], serverIP[20];
	// 输入命令缓冲区
	char buf[BUFLEN];
	// 用于分割命令
	char* token1, * token2;
	int nRC;
	WSADATA wsaData;
	addr_len = sizeof(struct sockaddr_in);
	//初始化 winsock
	nRC = WSAStartup(0x0101, &wsaData);
	if (nRC)
	{
		printf("Client initialize winsock error!\n");
		return;
	}
	if (wsaData.wVersion != 0x0101)
	{
		printf("Client's winsock version error!\n");
		WSACleanup();
		return;
	}
	printf("Client's winsock initialized !\n");
	cout << "input client IP:" << endl;
#ifdef DEBUG
	strcpy(clientIP, "192.168.65.1");
#else
	cin >> clientIP;
#endif // DEBUG

	cout << "input server IP:" << endl;
#ifdef DEBUG
	strcpy(serverIP, "192.168.65.132");
#else
	cin >> serverIP;
#endif // DEBUG
	//strcpy(argv[1], "192.168.65.1");
	// 设置服务器 地址族(ipv4) 端口 ip
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(69);
	serverAddr.sin_addr.S_un.S_addr = inet_addr(serverIP);
	// 设置客户端 地址族(ipv4) 端口 ip
	clientAddr.sin_family = AF_INET;
	clientAddr.sin_port = htons(0);
	clientAddr.sin_addr.S_un.S_addr = inet_addr(clientIP);
	// 创建客户端socket
	sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	ioctlsocket(sock, FIONBIO, &Opt);
	if (sock == INVALID_SOCKET)
	{
		// 创建失败
		cout << "Client create socket error!" << endl;
		WSACleanup();
		return;
	}
	printf("Client socket create OK!\n");
	// socket绑定
	nRC = bind(sock,(LPSOCKADDR)&clientAddr, sizeof(clientAddr));
	if (nRC == SOCKET_ERROR)
	{
		// 绑定失败
		printf("Client socket bind error!\n");
		closesocket(sock);
		WSACleanup();
		return;
	}
	printf("Client socket bind OK!\n");

	while (1) {
		// 输入命令
		gets_s(buf, sizeof(buf));
		token1 = strtok(buf, " \t");
		if (token1 == NULL)
			continue;
		if (strcmp(token1, "upload") == 0) {
			// 上传文件
			token1 = strtok(NULL, " \t");
			if (token1 != NULL) {
				cout << "****upload****" << endl;
				upload(token1);
			}
			else
				cout << "please input: upload + filename" << endl;
		}
		else if (strcmp(token1, "download") == 0) {
			token1 = strtok(NULL, " \t");
			token2 = strtok(NULL, " \t");
			if (token1 != NULL && token2 != NULL) {
				download(token1, token2);
			}
			else {
				cout << "please input: download + remote filename + local filename" << endl;
			}
			cout << "****download****" << endl;
		}
		else if (strcmp(token1, "exit") == 0) {
			cout << "****bye****" << endl;
			break;
		}
	}
	fclose(logFp);
}