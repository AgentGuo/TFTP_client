#include"client.h"
// client.cpp中定义的全局变量
extern sockaddr_in serverAddr, clientAddr;
extern SOCKET sock;
extern unsigned int addr_len;
extern unsigned long Opt;
extern double transByte, consumeTime;
extern FILE* logFp;
extern char logBuf[512];
extern time_t rawTime;
extern tm* info;
/*
* 上传文件
* 参数说明：filename:上传文件名(路径)
*/
bool upload(char* filename) {
	// 记录时间
	clock_t start, end;
	// 传输字节数
	transByte = 0;
	// 用于保存服务器发送的ip和端口
	sockaddr_in sender;
	// 等待时间,接收包大小
	int time_wait_ack, r_size, choose;
	// 发送包,接收包
	tftpPacket sendPacket, rcv_packet;
	// 写请求数据包
	// 写入操作码
	sendPacket.cmd = htons(CMD_WRQ);
	// 写入文件名以及传输格式
	cout << "please choose the file format(1.netascii  2.octet)" << endl;
	cin >> choose;
	if(choose == 1)
		sprintf(sendPacket.filename, "%s%c%s%c", filename, 0, "netascii", 0);
	else
		sprintf(sendPacket.filename, "%s%c%s%c", filename, 0, "octet", 0);
	// 发送请求数据包
	sendto(sock, (char*)&sendPacket, sizeof(tftpPacket), 0, (struct sockaddr*) & serverAddr, addr_len);
	// 等待接收回应,最多等待3s,每隔20ms刷新一次
	for (time_wait_ack = 0; time_wait_ack < PKT_RCV_TIMEOUT; time_wait_ack += 20) {
		// 非阻塞模式
		//ioctlsocket(sock, FIONBIO, &Opt);
		// 尝试接收
		r_size = recvfrom(sock, (char*)&rcv_packet, sizeof(tftpPacket), 0, (struct sockaddr*) & sender, (int*)&addr_len);
		if (r_size > 0 && r_size < 4) {
			// 接收包异常
			printf("Bad packet: r_size=%d\n", r_size);
		}
		if (r_size >= 4 && rcv_packet.cmd == htons(CMD_ACK) && rcv_packet.block == htons(0)) {
			// 成功收到ACK
			break;
		}
		Sleep(20);
	}
	if (time_wait_ack >= PKT_RCV_TIMEOUT) {
		// 超时,未接受到ACK
		printf("Could not receive from server.\n");
		// 写入日志
		// 获取时间
		time(&rawTime);
		// 转化为当地时间
		info = localtime(&rawTime);
		sprintf(logBuf, "%s ERROR: upload %s, mode: %s, %s\n",
			asctime(info), filename, choose == 1 ? ("netascii") : ("octet"),
			"Could not receive from server.");
		for (int i = 0; i < 512; i++) {
			if (logBuf[i] == '\n') {
				logBuf[i] = ' ';
				break;
			}
		}
		fwrite(logBuf, 1, strlen(logBuf), logFp);

		return false;
	}
	// 打开文件
	FILE* fp = NULL;
	if(choose == 1)
		fp = fopen(filename, "r");
	else
		fp = fopen(filename, "rb");
	if (fp == NULL) {
		printf("File not exists!\n");
		// 写入日志
		// 获取时间
		time(&rawTime);
		// 转化为当地时间
		info = localtime(&rawTime);
		sprintf(logBuf, "%s ERROR: upload %s, mode: %s, %s\n",
			asctime(info), filename, choose == 1 ? ("netascii") : ("octet"),
			"File not exists!");
		for (int i = 0; i < 512; i++) {
			if (logBuf[i] == '\n') {
				logBuf[i] = ' ';
				break;
			}
		}
		fwrite(logBuf, 1, strlen(logBuf), logFp);

		return false;
	}
	int s_size = 0;
	int rxmt;
	unsigned short block = 1;
	// 数据包操作码
	sendPacket.cmd = htons(CMD_DATA);
	// Send data.
	start = clock();
	do {
		// 清空数据
		memset(sendPacket.data, 0, sizeof(sendPacket.data));
		// 写入块编号
		sendPacket.block = htons(block);
		// 读入数据
		s_size = fread(sendPacket.data, 1, DATA_SIZE, fp);
		transByte += s_size;
		// 最多重传3次
		for (rxmt = 0; rxmt < PKT_MAX_RXMT; rxmt++) {
			// 发送数据包
			sendto(sock, (char*)&sendPacket, s_size + 4, 0, (struct sockaddr*) & sender, addr_len);
			printf("Send the %d block\n", block);
			// 等待ACK,最多等待3s,每隔20ms刷新一次
			for (time_wait_ack = 0; time_wait_ack < PKT_RCV_TIMEOUT; time_wait_ack += 20) {
				r_size = recvfrom(sock, (char*)&rcv_packet, sizeof(tftpPacket), 0, (struct sockaddr*) & sender, (int*)&addr_len);
				if (r_size > 0 && r_size < 4) {
					printf("Bad packet: r_size=%d\n", r_size);
				}
				if (r_size >= 4 && rcv_packet.cmd == htons(CMD_ACK) && rcv_packet.block == htons(block)) {
					break;
				}
				Sleep(20);
			}
			if (time_wait_ack < PKT_RCV_TIMEOUT) {
				// 发送成功
				break;
			}
			else {
				// 未收到ACK，重传
				// 写入日志
				// 获取时间
				time(&rawTime);
				// 转化为当地时间
				info = localtime(&rawTime);
				sprintf(logBuf, "%s WARN: upload %s, mode: %s, %s\n",
					asctime(info), filename, choose == 1 ? ("netascii") : ("octet"),
					"Can't receive ACK, Retransmission");
				for (int i = 0; i < 512; i++) {
					if (logBuf[i] == '\n') {
						logBuf[i] = ' ';
						break;
					}
				}
				fwrite(logBuf, 1, strlen(logBuf), logFp);

				continue;
			}
		}
		if (rxmt >= PKT_MAX_RXMT) {
			// 3次重传失败
			printf("Could not receive from server.\n");
			fclose(fp);

			// 写入日志
			// 获取时间
			time(&rawTime);
			// 转化为当地时间
			info = localtime(&rawTime);
			sprintf(logBuf, "%s ERROR: upload %s, mode: %s, %s\n",
				asctime(info), filename, choose == 1 ? ("netascii") : ("octet"),
				"Could not receive from server.");
			for (int i = 0; i < 512; i++) {
				if (logBuf[i] == '\n') {
					logBuf[i] = ' ';
					break;
				}
			}
			fwrite(logBuf, 1, strlen(logBuf), logFp);

			return false;
		}
		// 传输下一个数据块
		block++;
	} while (s_size == DATA_SIZE);	// 当数据块未装满时认为时最后一块数据，结束循环
	end = clock();
	printf("Send file end.\n");

	fclose(fp);
	consumeTime = ((double)(end - start)) / CLK_TCK;
	printf("upload file size: %.1f kB consuming time: %.2f s\n", transByte/ 1024, consumeTime);
	printf("upload speed: %.1f kB/s\n", transByte/(1024 * consumeTime));

	// 获取时间
	time(&rawTime);
	// 转化为当地时间
	info = localtime(&rawTime);
	sprintf(logBuf, "%s INFO: upload %s, mode: %s, size: %.1f kB, consuming time: %.2f s\n",
		asctime(info), filename, choose == 1 ? ("netascii") : ("octet"), transByte / 1024, consumeTime);
	for (int i = 0; i < 512; i++) {
		if (logBuf[i] == '\n') {
			logBuf[i] = ' ';
			break;
		}
	}
	fwrite(logBuf, 1, strlen(logBuf), logFp);
	return true;
}
/*
* 下载文件
* 参数说明：remoteFile:远程文件名(路径) localFile:本地文件名(路径)
*/
bool download(char* remoteFile, char * localFile) {
	// 记录时间
	clock_t start, end;
	// 传输字节数
	transByte = 0;
	// 用于保存服务器发送的ip和端口
	sockaddr_in sender;
	// 等待时间,接收包大小
	int time_wait_ack, r_size, choose;
	// 发送包,接收包
	tftpPacket sendPacket, rcv_packet, ack;
	
	int next_block = 1;
	int recv_n;
	int total_bytes = 0;

	int time_wait_data;
	unsigned short block = 1;

	// 读请求数据包
	// 读取操作码
	sendPacket.cmd = htons(CMD_RRQ);
	// 写入文件名以及传输格式
	cout << "please choose the file format(1.netascii  2.octet)" << endl;
	cin >> choose;
	if (choose == 1)
		sprintf(sendPacket.filename, "%s%c%s%c", remoteFile, 0, "netascii", 0);
	else
		sprintf(sendPacket.filename, "%s%c%s%c", remoteFile, 0, "octet", 0);
	// 发送请求数据包
	sendto(sock, (char *)&sendPacket, sizeof(tftpPacket), 0, (struct sockaddr*) & serverAddr, addr_len);

	// 创建本地写入文件
	FILE* fp = NULL;
	if (choose == 1)
		fp = fopen(localFile, "w");
	else
		fp = fopen(localFile, "wb");
	if (fp == NULL) {
		printf("Create file \"%s\" error.\n", localFile);

		// 获取时间
		time(&rawTime);
		// 转化为当地时间
		info = localtime(&rawTime);
		sprintf(logBuf, "%s ERROR: download %s as %s, mode: %s, Create file \"%s\" error.\n",
			asctime(info), remoteFile, localFile, choose == 1 ? ("netascii") : ("octet"),
			localFile);
		for (int i = 0; i < 512; i++) {
			if (logBuf[i] == '\n') {
				logBuf[i] = ' ';
				break;
			}
		}
		fwrite(logBuf, 1, strlen(logBuf), logFp);

		return false;
	}

	// 接收数据
	start = clock();
	sendPacket.cmd = htons(CMD_ACK);
	do {
		for (time_wait_data = 0; time_wait_data < PKT_RCV_TIMEOUT * PKT_MAX_RXMT; time_wait_data += 10) {
			// Try receive(Nonblock receive).
			r_size = recvfrom(sock, (char *)&rcv_packet, sizeof(tftpPacket), 0, (struct sockaddr*) & sender,(int *)&addr_len);
			if (r_size > 0 && r_size < 4) {
				printf("Bad packet: r_size=%d\n", r_size);
			}
			if (r_size >= 4 && rcv_packet.cmd == htons(CMD_DATA) && rcv_packet.block == htons(block)) {
				printf("DATA: block=%d, data_size=%d\n", ntohs(rcv_packet.block), r_size - 4);
				// Send ACK.
				sendPacket.block = rcv_packet.block;
				sendto(sock, (char*)&sendPacket, sizeof(tftpPacket), 0, (struct sockaddr*) & sender, addr_len);
				fwrite(rcv_packet.data, 1, r_size - 4, fp);
				break;
			}
			Sleep(10);
		}
		transByte += (r_size - 4);
		if (time_wait_data >= PKT_RCV_TIMEOUT * PKT_MAX_RXMT) {
			printf("Wait for DATA #%d timeout.\n", block);
			fclose(fp);

			// 获取时间
			time(&rawTime);
			// 转化为当地时间
			info = localtime(&rawTime);
			sprintf(logBuf, "%s ERROR: download %s as %s, mode: %s, Wait for DATA #%d timeout.\n",
				asctime(info), remoteFile, localFile, choose == 1 ? ("netascii") : ("octet"),
				block);
			for (int i = 0; i < 512; i++) {
				if (logBuf[i] == '\n') {
					logBuf[i] = ' ';
					break;
				}
			}
			fwrite(logBuf, 1, strlen(logBuf), logFp);

			return false;
		}
		block++;
	} while (r_size == DATA_SIZE + 4);
	end = clock();
	consumeTime = ((double)(end - start)) / CLK_TCK;
	printf("download file size: %.1f kB consuming time: %.2f s\n", transByte / 1024, consumeTime);
	printf("download speed: %.1f kB/s\n", transByte / (1024 * consumeTime));
	fclose(fp);
	// 获取时间
	time(&rawTime);
	// 转化为当地时间
	info = localtime(&rawTime);
	sprintf(logBuf, "%s INFO: download %s as %s, mode: %s, size: %.1f kB, consuming time: %.2f s\n",
		asctime(info), remoteFile, localFile, choose == 1 ? ("netascii") : ("octet"), transByte / 1024, consumeTime);
	for (int i = 0; i < 512; i++) {
		if (logBuf[i] == '\n') {
			logBuf[i] = ' ';
			break;
		}
	}
	fwrite(logBuf, 1, strlen(logBuf), logFp);
	return true;
}