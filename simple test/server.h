#pragma once
//初始化用于监听的套接字
int initListenFD(unsigned short port);
//启动epoll
int epollRun(int lfd);
//和客户端建立连接
int acceptClient(int lfd,int epfd);
//接收http请求
int recvHttpRequest(int cfd,int epfd);
//解析请求行
int parseRequestLine(const char* line, int cfd);
//发送文件给客户端
int sendFile(const char* filename, int cfd);
//发送响应头（状态加响应头）
int sendHeadMsg(int cfd, int status, const char* descr,char *type,int length);
//content-type 对照函数
char* getFileType(char* name);
//发送目录
int sendDir(const char* dirName, int cfd);
