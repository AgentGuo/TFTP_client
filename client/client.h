#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <iostream>
#include <winsock2.h>
#include <windows.h>
#pragma comment(lib,"ws2_32.lib")

#define DEBUG
// TFTP�Ĳ�����
#define CMD_RRQ (short)1
#define CMD_WRQ (short)2
#define CMD_DATA (short)3
#define CMD_ACK (short)4
#define CMD_ERROR (short)5
#define CMD_LIST (short)6
#define CMD_HEAD (short)7
#define BUFLEN 255
#define DATA_SIZE 512
#define PKT_MAX_RXMT 3
#define PKT_RCV_TIMEOUT 3*1000

// TFTP���ݰ��ṹ��
struct tftpPacket {
	// ǰ�����ֽڱ�ʾ������
	unsigned short cmd;
	// �м����ֶ�
	union {
		unsigned short code;
		unsigned short block;
		// filenameֻ�������ֽڣ����ǿ����������д�룬����������
		char filename[2];
	};
	// ������������ֶ�
	char data[DATA_SIZE];
};

using namespace std;
// �ϴ��ļ���������
bool upload(char* filename);
bool download(char* remoteFile, char* localFile);