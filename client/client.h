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
// TFTP的操作码
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

// TFTP数据包结构体
struct tftpPacket {
	// 前两个字节表示操作码
	unsigned short cmd;
	// 中间区字段
	union {
		unsigned short code;
		unsigned short block;
		// filename只有两个字节，但是可以往后继续写入，覆盖数据区
		char filename[2];
	};
	// 最后是数据区字段
	char data[DATA_SIZE];
};

using namespace std;
// 上传文件函数声明
bool upload(char* filename);
bool download(char* remoteFile, char* localFile);