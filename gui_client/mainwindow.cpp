#include "mainwindow.h"
#include "ui_mainwindow.h"
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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    initUI();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::initUI(){
    WSADATA wsaData;
    addr_len = sizeof(struct sockaddr_in);
    //初始化 winsock
    int nRC = WSAStartup(0x0101, &wsaData);
    if (nRC)
    {
        ui->output->append("Client initialize winsock error!");
    }
    if (wsaData.wVersion != 0x0101)
    {
        ui->output->append("Client's winsock version error!");
        WSACleanup();
    }
}
void MainWindow::on_PathChoose_pressed(){
    QDir dir;
    QString PathName = QFileDialog::getOpenFileName(this,tr(""),"",tr("file(*)")); //选择路径
    ui->PathShow->setText(PathName);    //文件名称显示
}
void MainWindow::on_uploadButton_pressed(){
    QByteArray Qfilename = ui->PathShow->text().toLatin1();
    QByteArray QserverIP = ui->uploadServerIP->text().toLatin1();
    QByteArray QclientIP = ui->uploadLocalIP->text().toLatin1();
    char * filePath = Qfilename.data();
    char * serverIP = QserverIP.data();
    char * clientIP = QclientIP.data();
    char buf[512], filename[512];
    int temp = 0;
    for(int i = 0; filePath[i]!='\0'; i++, temp++){
        if(filePath[i] == '/'){
            i++;
            temp = 0;
            filename[temp] = filePath[i];
        }
        else{
            filename[temp] = filePath[i];
        }
    }
    filename[temp] = '\0';
    logFp = fopen("tftp.log", "a");
    if (logFp == NULL) {
        ui->output->append("open log file failed");
    }


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
        sprintf(buf, "Client create socket error!");
        ui->output->append(buf);
        fclose(logFp);
        return;
    }
    sprintf(buf, "Client socket create OK!");
    ui->output->append(buf);
    // socket绑定
    bind(sock,(LPSOCKADDR)&clientAddr, sizeof(clientAddr));

    // 记录时间
    clock_t start, end;
    // 传输字节数
    transByte = 0;
    // 用于保存服务器发送的ip和端口
    sockaddr_in sender;
    // 等待时间,接收包大小
    int time_wait_ack, r_size, choose, resent = 0;
    // 发送包,接收包
    tftpPacket sendPacket, rcv_packet;
    // 写请求数据包
    // 写入操作码
    sendPacket.cmd = htons(CMD_WRQ);
    // 写入文件名以及传输格式
    choose = ui->uploadMode->currentIndex();
    sprintf(buf, "choose=%d", choose);
    ui->output->append(buf);
    if(choose == 0)
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
            sprintf(buf, "Bad packet: r_size=%d", r_size);
            ui->output->append(buf);
        }
        if (r_size >= 4 && rcv_packet.cmd == htons(CMD_ACK) && rcv_packet.block == htons(0)) {
            // 成功收到ACK
            break;
        }
        Sleep(20);
    }
    if (time_wait_ack >= PKT_RCV_TIMEOUT) {
        // 超时,未接受到ACK
        //printf("Could not receive from server.\n");
        sprintf(buf, "Could not receive from server.");
        ui->output->append(buf);
        // 写入日志
        // 获取时间
        time(&rawTime);
        // 转化为当地时间
        info = localtime(&rawTime);
        sprintf(logBuf, "%s ERROR: upload %s, mode: %s, %s\n",
            asctime(info), filename, choose == 0 ? ("netascii") : ("octet"),
            "Could not receive from server.");
        for (int i = 0; i < 512; i++) {
            if (logBuf[i] == '\n') {
                logBuf[i] = ' ';
                break;
            }
        }
        fwrite(logBuf, 1, strlen(logBuf), logFp);
        fclose(logFp);
        return;
    }
    // 打开文件
    FILE* fp = NULL;
    if(choose == 0)
        fp = fopen(filePath, "r");
    else
        fp = fopen(filePath, "rb");
    if (fp == NULL) {
        sprintf(buf, "File not exists!");
        ui->output->append(buf);
        // 写入日志
        // 获取时间
        time(&rawTime);
        // 转化为当地时间
        info = localtime(&rawTime);
        sprintf(logBuf, "%s ERROR: upload %s, mode: %s, %s\n",
            asctime(info), filename, choose == 0 ? ("netascii") : ("octet"),
            "File not exists!");
        for (int i = 0; i < 512; i++) {
            if (logBuf[i] == '\n') {
                logBuf[i] = ' ';
                break;
            }
        }
        fwrite(logBuf, 1, strlen(logBuf), logFp);
        fclose(logFp);

        return;
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
            sprintf(buf, "Send the %d block", block);
            ui->output->append(buf);
            // 等待ACK,最多等待3s,每隔20ms刷新一次
            for (time_wait_ack = 0; time_wait_ack < PKT_RCV_TIMEOUT; time_wait_ack += 20) {
                r_size = recvfrom(sock, (char*)&rcv_packet, sizeof(tftpPacket), 0, (struct sockaddr*) & sender, (int*)&addr_len);
                if (r_size > 0 && r_size < 4) {
                    sprintf(buf, "Bad packet: r_size=%d", r_size);
                    ui->output->append(buf);
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
                    asctime(info), filename, choose == 0 ? ("netascii") : ("octet"),
                    "Can't receive ACK, resent");
                for (int i = 0; i < 512; i++) {
                    if (logBuf[i] == '\n') {
                        logBuf[i] = ' ';
                        break;
                    }
                }
                fwrite(logBuf, 1, strlen(logBuf), logFp);
                resent++;

                continue;
            }
        }
        if (rxmt >= PKT_MAX_RXMT) {
            // 3次重传失败
            sprintf(buf, "Could not receive from server.");
            ui->output->append(buf);
            fclose(fp);


            // 写入日志
            // 获取时间
            time(&rawTime);
            // 转化为当地时间
            info = localtime(&rawTime);
            sprintf(logBuf, "%s ERROR: upload %s, mode: %s, Wait for ACK timeout\n",
                asctime(info), filename, choose == 0 ? ("netascii") : ("octet"));
            for (int i = 0; i < 512; i++) {
                if (logBuf[i] == '\n') {
                    logBuf[i] = ' ';
                    break;
                }
            }
            fwrite(logBuf, 1, strlen(logBuf), logFp);
            fclose(logFp);
            return;
        }
        // 传输下一个数据块
        block++;
    } while (s_size == DATA_SIZE);	// 当数据块未装满时认为时最后一块数据，结束循环
    end = clock();
    sprintf(buf, "Send file end.");
    ui->output->append(buf);

    fclose(fp);
    consumeTime = ((double)(end - start)) / CLK_TCK;
    sprintf(buf, "upload file size: %.1f kB consuming time: %.2f s", transByte/ 1024, consumeTime);
    ui->output->append(buf);
    sprintf(buf, "upload speed: %.1f kB/s", transByte/(1024 * consumeTime));
    ui->output->append(buf);
    sprintf(buf, "resent packet:%d, packet loss probability:%.2f%%", resent, 100 * ((double)resent / (resent + block - 1)));
    ui->output->append(buf);

    // 获取时间
    time(&rawTime);
    // 转化为当地时间
    info = localtime(&rawTime);
    sprintf(logBuf, "%s INFO: upload %s, mode: %s, size: %.1f kB, consuming time: %.2f s\n",
        asctime(info), filename, choose == 0 ? ("netascii") : ("octet"), transByte / 1024, consumeTime);
    for (int i = 0; i < 512; i++) {
        if (logBuf[i] == '\n') {
            logBuf[i] = ' ';
            break;
        }
    }
    fwrite(logBuf, 1, strlen(logBuf), logFp);
    fclose(logFp);
}
void MainWindow::on_downloadButton_pressed(){
    QByteArray QremoteFile = ui->downloadServerFilename->text().toLatin1();
    QByteArray QlocalFile = ui->downloadLocalFilename->text().toLatin1();
    QByteArray QserverIP = ui->downloadServerIP->text().toLatin1();
    QByteArray QclientIP = ui->downloadLocalIP->text().toLatin1();
    char * localFile = QlocalFile.data();
    char * remoteFile = QremoteFile.data();
    char * serverIP = QserverIP.data();
    char * clientIP = QclientIP.data();
    char buf[512];
    ui->output->append(remoteFile);
    ui->output->append(localFile);

    logFp = fopen("tftp.log", "a");
    if (logFp == NULL) {
        ui->output->append("open log file failed");
    }


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
        sprintf(buf, "Client create socket error!");
        ui->output->append(buf);
        fclose(logFp);
        return;
    }
    sprintf(buf, "Client socket create OK!");
    ui->output->append(buf);
    // socket绑定
    bind(sock,(LPSOCKADDR)&clientAddr, sizeof(clientAddr));

    // 记录时间
    clock_t start, end;
    // 传输字节数
    transByte = 0;
    // 用于保存服务器发送的ip和端口
    sockaddr_in sender;
    // 等待时间,接收包大小
    int time_wait_ack, r_size, choose, resent = 0;
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
    choose = ui->downloadMode->currentIndex();
    if(choose == 0)
        sprintf(sendPacket.filename, "%s%c%s%c", remoteFile, 0, "netascii", 0);
    else
        sprintf(sendPacket.filename, "%s%c%s%c", remoteFile, 0, "octet", 0);
    // 发送请求数据包
    sendto(sock, (char *)&sendPacket, sizeof(tftpPacket), 0, (struct sockaddr*) & serverAddr, addr_len);

    // 创建本地写入文件
    FILE* fp = NULL;
    if (choose == 0)
        fp = fopen(localFile, "w");
    else
        fp = fopen(localFile, "wb");
    if (fp == NULL) {
        sprintf(buf, "Create file \"%s\" error.", localFile);
        ui->output->append(buf);

        // 获取时间
        time(&rawTime);
        // 转化为当地时间
        info = localtime(&rawTime);
        sprintf(logBuf, "%s ERROR: download %s as %s, mode: %s, Create file \"%s\" error.\n",
            asctime(info), remoteFile, localFile, choose == 0 ? ("netascii") : ("octet"),
            localFile);
        for (int i = 0; i < 512; i++) {
            if (logBuf[i] == '\n') {
                logBuf[i] = ' ';
                break;
            }
        }
        fwrite(logBuf, 1, strlen(logBuf), logFp);
        fclose(logFp);
        return;
    }
    // 接收数据
    start = clock();
    sendPacket.cmd = htons(CMD_ACK);
    do {
        for (time_wait_data = 0; time_wait_data < PKT_RCV_TIMEOUT * PKT_MAX_RXMT; time_wait_data += 50) {
            // Try receive(Nonblock receive).
            r_size = recvfrom(sock, (char *)&rcv_packet, sizeof(tftpPacket), 0, (struct sockaddr*) & sender,(int *)&addr_len);
            if (time_wait_data == PKT_RCV_TIMEOUT && block == 1) {
                // 未能连接上服务器
                // 获取时间
                sprintf(buf, "Could not receive from server.");
                ui->output->append(buf);

                time(&rawTime);
                // 转化为当地时间
                info = localtime(&rawTime);
                sprintf(logBuf, "%s ERROR: download %s as %s, mode: %s, Could not receive from server.\n",
                    asctime(info), remoteFile, localFile, choose == 0 ? ("netascii") : ("octet"));
                for (int i = 0; i < 512; i++) {
                    if (logBuf[i] == '\n') {
                        logBuf[i] = ' ';
                        break;
                    }
                }
                fwrite(logBuf, 1, strlen(logBuf), logFp);
                fclose(fp);
                fclose(logFp);
                return;
            }
            if (r_size > 0 && r_size < 4) {
                sprintf(buf, "Bad packet: r_size=%d", r_size);
                ui->output->append(buf);
            }
            else if (r_size >= 4 && rcv_packet.cmd == htons(CMD_DATA) && rcv_packet.block == htons(block)) {
                sprintf(buf, "DATA: block=%d, data_size=%d\n", ntohs(rcv_packet.block), r_size - 4);
                ui->output->append(buf);
                // Send ACK.
                sendPacket.block = rcv_packet.block;
                sendto(sock, (char*)&sendPacket, sizeof(tftpPacket), 0, (struct sockaddr*) & sender, addr_len);
                fwrite(rcv_packet.data, 1, r_size - 4, fp);
                break;
            }
            else {
                if (time_wait_data != 0 && time_wait_data % PKT_RCV_TIMEOUT == 0) {
                    // 重发ACK
                    sendto(sock, (char*)&sendPacket, sizeof(tftpPacket), 0, (struct sockaddr*) & sender, addr_len);
                    // 获取时间
                    time(&rawTime);
                    // 转化为当地时间
                    info = localtime(&rawTime);
                    sprintf(logBuf, "%s WARN: download %s as %s, mode: %s, Can't receive DATA #%d, resent\n",
                        asctime(info), remoteFile, localFile, choose == 0 ? ("netascii") : ("octet"),
                        block);
                    for (int i = 0; i < 512; i++) {
                        if (logBuf[i] == '\n') {
                            logBuf[i] = ' ';
                            break;
                        }
                    }
                    fwrite(logBuf, 1, strlen(logBuf), logFp);
                    resent++;
                }
            }
            Sleep(50);
        }
        transByte += (r_size - 4);
        if (time_wait_data >= PKT_RCV_TIMEOUT * PKT_MAX_RXMT) {
            sprintf(buf, "Wait for DATA #%d timeout.\n", block);
            ui->output->append(buf);
            fclose(fp);

            // 获取时间
            time(&rawTime);
            // 转化为当地时间
            info = localtime(&rawTime);
            sprintf(logBuf, "%s ERROR: download %s as %s, mode: %s, Wait for DATA #%d timeout.\n",
                asctime(info), remoteFile, localFile, choose == 0 ? ("netascii") : ("octet"),
                block);
            for (int i = 0; i < 512; i++) {
                if (logBuf[i] == '\n') {
                    logBuf[i] = ' ';
                    break;
                }
            }
            fwrite(logBuf, 1, strlen(logBuf), logFp);
            fclose(logFp);
            return;
        }
        block++;
    } while (r_size == DATA_SIZE + 4);
    end = clock();
    consumeTime = ((double)(end - start)) / CLK_TCK;
    sprintf(buf, "upload file size: %.1f kB consuming time: %.2f s", transByte/ 1024, consumeTime);
    ui->output->append(buf);
    sprintf(buf, "upload speed: %.1f kB/s", transByte/(1024 * consumeTime));
    ui->output->append(buf);
    sprintf(buf, "resent packet:%d, packet loss probability:%.2f%%", resent, 100 * ((double)resent / (resent + block - 1)));
    ui->output->append(buf);
    fclose(fp);
    // 获取时间
    time(&rawTime);
    // 转化为当地时间
    info = localtime(&rawTime);
    sprintf(logBuf, "%s INFO: download %s as %s, mode: %s, size: %.1f kB, consuming time: %.2f s\n",
        asctime(info), remoteFile, localFile, choose == 0 ? ("netascii") : ("octet"), transByte / 1024, consumeTime);
    for (int i = 0; i < 512; i++) {
        if (logBuf[i] == '\n') {
            logBuf[i] = ' ';
            break;
        }
    }
    fwrite(logBuf, 1, strlen(logBuf), logFp);
    fclose(logFp);
}
