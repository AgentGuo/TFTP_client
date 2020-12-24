#include"client.h"
// client.cpp�ж����ȫ�ֱ���
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
* �ϴ��ļ�
* ����˵����filename:�ϴ��ļ���(·��)
*/
bool upload(char* filename) {
	// ��¼ʱ��
	clock_t start, end;
	// �����ֽ���
	transByte = 0;
	// ���ڱ�����������͵�ip�Ͷ˿�
	sockaddr_in sender;
	// �ȴ�ʱ��,���հ���С
	int time_wait_ack, r_size, choose;
	// ���Ͱ�,���հ�
	tftpPacket sendPacket, rcv_packet;
	// д�������ݰ�
	// д�������
	sendPacket.cmd = htons(CMD_WRQ);
	// д���ļ����Լ������ʽ
	cout << "please choose the file format(1.netascii  2.octet)" << endl;
	cin >> choose;
	if(choose == 1)
		sprintf(sendPacket.filename, "%s%c%s%c", filename, 0, "netascii", 0);
	else
		sprintf(sendPacket.filename, "%s%c%s%c", filename, 0, "octet", 0);
	// �����������ݰ�
	sendto(sock, (char*)&sendPacket, sizeof(tftpPacket), 0, (struct sockaddr*) & serverAddr, addr_len);
	// �ȴ����ջ�Ӧ,���ȴ�3s,ÿ��20msˢ��һ��
	for (time_wait_ack = 0; time_wait_ack < PKT_RCV_TIMEOUT; time_wait_ack += 20) {
		// ������ģʽ
		//ioctlsocket(sock, FIONBIO, &Opt);
		// ���Խ���
		r_size = recvfrom(sock, (char*)&rcv_packet, sizeof(tftpPacket), 0, (struct sockaddr*) & sender, (int*)&addr_len);
		if (r_size > 0 && r_size < 4) {
			// ���հ��쳣
			printf("Bad packet: r_size=%d\n", r_size);
		}
		if (r_size >= 4 && rcv_packet.cmd == htons(CMD_ACK) && rcv_packet.block == htons(0)) {
			// �ɹ��յ�ACK
			break;
		}
		Sleep(20);
	}
	if (time_wait_ack >= PKT_RCV_TIMEOUT) {
		// ��ʱ,δ���ܵ�ACK
		printf("Could not receive from server.\n");
		// д����־
		// ��ȡʱ��
		time(&rawTime);
		// ת��Ϊ����ʱ��
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
	// ���ļ�
	FILE* fp = NULL;
	if(choose == 1)
		fp = fopen(filename, "r");
	else
		fp = fopen(filename, "rb");
	if (fp == NULL) {
		printf("File not exists!\n");
		// д����־
		// ��ȡʱ��
		time(&rawTime);
		// ת��Ϊ����ʱ��
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
	// ���ݰ�������
	sendPacket.cmd = htons(CMD_DATA);
	// Send data.
	start = clock();
	do {
		// �������
		memset(sendPacket.data, 0, sizeof(sendPacket.data));
		// д�����
		sendPacket.block = htons(block);
		// ��������
		s_size = fread(sendPacket.data, 1, DATA_SIZE, fp);
		transByte += s_size;
		// ����ش�3��
		for (rxmt = 0; rxmt < PKT_MAX_RXMT; rxmt++) {
			// �������ݰ�
			sendto(sock, (char*)&sendPacket, s_size + 4, 0, (struct sockaddr*) & sender, addr_len);
			printf("Send the %d block\n", block);
			// �ȴ�ACK,���ȴ�3s,ÿ��20msˢ��һ��
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
				// ���ͳɹ�
				break;
			}
			else {
				// δ�յ�ACK���ش�
				// д����־
				// ��ȡʱ��
				time(&rawTime);
				// ת��Ϊ����ʱ��
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
			// 3���ش�ʧ��
			printf("Could not receive from server.\n");
			fclose(fp);

			// д����־
			// ��ȡʱ��
			time(&rawTime);
			// ת��Ϊ����ʱ��
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
		// ������һ�����ݿ�
		block++;
	} while (s_size == DATA_SIZE);	// �����ݿ�δװ��ʱ��Ϊʱ���һ�����ݣ�����ѭ��
	end = clock();
	printf("Send file end.\n");

	fclose(fp);
	consumeTime = ((double)(end - start)) / CLK_TCK;
	printf("upload file size: %.1f kB consuming time: %.2f s\n", transByte/ 1024, consumeTime);
	printf("upload speed: %.1f kB/s\n", transByte/(1024 * consumeTime));

	// ��ȡʱ��
	time(&rawTime);
	// ת��Ϊ����ʱ��
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
* �����ļ�
* ����˵����remoteFile:Զ���ļ���(·��) localFile:�����ļ���(·��)
*/
bool download(char* remoteFile, char * localFile) {
	// ��¼ʱ��
	clock_t start, end;
	// �����ֽ���
	transByte = 0;
	// ���ڱ�����������͵�ip�Ͷ˿�
	sockaddr_in sender;
	// �ȴ�ʱ��,���հ���С
	int time_wait_ack, r_size, choose;
	// ���Ͱ�,���հ�
	tftpPacket sendPacket, rcv_packet, ack;
	
	int next_block = 1;
	int recv_n;
	int total_bytes = 0;

	int time_wait_data;
	unsigned short block = 1;

	// ���������ݰ�
	// ��ȡ������
	sendPacket.cmd = htons(CMD_RRQ);
	// д���ļ����Լ������ʽ
	cout << "please choose the file format(1.netascii  2.octet)" << endl;
	cin >> choose;
	if (choose == 1)
		sprintf(sendPacket.filename, "%s%c%s%c", remoteFile, 0, "netascii", 0);
	else
		sprintf(sendPacket.filename, "%s%c%s%c", remoteFile, 0, "octet", 0);
	// �����������ݰ�
	sendto(sock, (char *)&sendPacket, sizeof(tftpPacket), 0, (struct sockaddr*) & serverAddr, addr_len);

	// ��������д���ļ�
	FILE* fp = NULL;
	if (choose == 1)
		fp = fopen(localFile, "w");
	else
		fp = fopen(localFile, "wb");
	if (fp == NULL) {
		printf("Create file \"%s\" error.\n", localFile);

		// ��ȡʱ��
		time(&rawTime);
		// ת��Ϊ����ʱ��
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

	// ��������
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

			// ��ȡʱ��
			time(&rawTime);
			// ת��Ϊ����ʱ��
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
	// ��ȡʱ��
	time(&rawTime);
	// ת��Ϊ����ʱ��
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