#include "server.h"
#include<arpa/inet.h>//声明包括socket的更多套接字头文件
#include <cstdio>
#include<iostream>
#include<sys/epoll.h>
#include <vector>
#include <fcntl.h>
#include<cstring>
#include<errno.h>
#include<sys/stat.h>
#include<assert.h>
#include<unistd.h>
#include<sys/sendfile.h>
#include<dirent.h>
using namespace std;
int initListenFD(unsigned short port)
{
	//1.创建用于监听的套接字
	int lfd = socket(AF_INET, SOCK_STREAM, 0);//ipv4协议,流式还是报文
	if (lfd == -1) {
		perror("创建套接字失败");
		return 0;
	}
    //2.设置端口复用
	int opt = 1;//设置端口解绑锁定为1min
	int ret = setsockopt(lfd, SOL_SOCKET,SO_REUSEADDR,&opt,sizeof opt);
	if (ret == -1) {
		perror("设置端口复用失败");
		return 0;
	}
    //3.bind 端口
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;//设置协议为ipv4
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;//设置绑定本地任意一ip地址
	ret = bind(lfd, (struct sockaddr*)&addr, sizeof(addr));//绑定文件描述符，绑定地址，结构体所占内存大小
	if (ret == -1) {
			perror("绑定失败失败");
			return 0;
	}
	//4.设置端口
	ret = listen(lfd, 128);
	if (ret == -1) {
		perror("监听失败");
	}
	//5.返回fd
	return lfd;
}

int epollRun(int lfd)
{
	//1.创建epoll实例
	int epfd = epoll_create(1);
	if (epfd == -1) {
		perror("创建epoll实例失败");
	}
	//1.lfd上树
	struct epoll_event ev;
	ev.data.fd = lfd;
	ev.events = EPOLLIN;
	int ret = epoll_ctl(epfd, EPOLL_CTL_ADD, lfd, &ev);
	if(ret == -1) {
		perror("lfd上树失败");
		return 0;
	}
	//3.检测
	struct epoll_event evs [1024];
	while (true)
	{
		int num=epoll_wait(epfd,evs,sizeof evs/sizeof (struct epoll_event), -1);//指定为-1，表示一直阻塞
		for (int i = 0; i < num; i++) {
			int fd = evs[i].data.fd;
			if (fd == lfd) {//如果是用于监听的文件描述符，就建立连接
                //调用accept函数，程序不会阻塞
				acceptClient(fd, epfd);
			}
			else {
				//主要是处理接收端数据
				recvHttpRequest(fd, epfd);
			}
		}
	}
	return 0;
}

int acceptClient(int lfd, int epfd)
{
	int cfd = accept(lfd, NULL, NULL);
	if (cfd == -1) {
		perror("连接失败");
		return 0;
	}
	//设置非阻塞
	int flag = fcntl(cfd, F_GETFL);//获取属性
	flag |= O_NONBLOCK;//|=按位或设置追加非阻塞属性
	fcntl(cfd,F_SETFL,flag);//设置非阻塞属性
	//添加文件描述符到epfd里面
	struct epoll_event ev;
	ev.data.fd = cfd;
	ev.events = EPOLLIN|EPOLLET;//边沿模式
	int ret = epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &ev);
	if (ret == -1) {
		perror("cfd添加失败");
		return 0;
	}
	return 0;
}

int recvHttpRequest(int cfd, int epfd)
{
	int len=0,total=0;
	char tmp[1024] = { 0 };//避免数据覆盖
	char buf[4096] = { 0 };
	while ((len = recv(cfd, tmp, sizeof tmp, 0)) > 0) {
		if (total + len < sizeof buf)
		{
             memcpy(buf+total, tmp, len);
		}
		total += len;
	}
	//判断是否接收完数据
	if (len == -1 && errno == EAGAIN) {
		cout << "数据接收完毕" <<endl;
		//解析请求行
	}
	else if (len == 0) {
		//客户端断开连接
		cout << "客户端断开连接" << endl;
		//删除树上节点
		epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, NULL);
	}
	else {
		perror("接收错误");
	}
	return 0;
}

int parseRequestLine(const char* line, int cfd)
{
	//解析请求行get /XXX/1.jpg http/1.1
	char method[12];
	char path[1024];
	sscanf(line, "%[^ ] %[^ ]", method, path);
	//处理get请求
	if (strcasecmp(method, "get")!=0) {
		return 0;
	}
	//处理客户端请求的静态资源（文件或目录）
	char* file = NULL;
	if (strcmp(path, "/") == 0) {
		file = "./";
	}
	else {
		file = path + 1;
	}
	//获取文件属性
	struct stat st;
	int ret = stat(file, &st);
	if (ret == -1) {
		cout << "文件不存在" << endl;
		//回复一个404页面
		sendHeadMsg(cfd, 404, "Not Found",getFileType(".hmtl"),-1);
		sendFile("404.html", cfd);
		return 0;
	}
	//判断文件类型
	if (S_ISDIR(st.st_mode)) {//如果是目录返回一，不是返回零
		//发送本地目录内容

	}
	else {
		//把文件内容发送客户端
		sendHeadMsg(cfd, 200, "OK", getFileType(file), st.st_size);
		sendFile(file, cfd);
	}
	return 0;
}

int sendFile(const char* filename, int cfd)
{
	//读一部分发一部分
	//1.打开文件
	int fd = open(filename, O_RDONLY);
#if 0
	assert(fd > 0);//如果大于0直接挂掉程序
	while (1) {
		char buf[1024];
		int len = read(fd, buf, sizeof buf);
		if (len > 0) {
			send(cfd, buf, len, 0);
			usleep(10);//单位是微秒,这里非常重要，不要让数据发送太快
		}
		else if (len==0) {
			break;
		}
		else {
			perror("read");
		}
	}
#else
	int size = lseek(fd, 0, SEEK_END);//文件描述符，偏移量，表示把指针移到最后
	sendfile(cfd, fd, NULL, size);
#endif
	return 0;
}

int sendHeadMsg(int cfd, int status, const char* descr, char* type, int length)
{
	//状态行
	char buf[4096];
	sprintf(buf, "http/1.1 %d %s\r\n", status, descr);
	//响应头
	sprintf(buf + strlen(buf), "content-type: %s\r\n", type);
	sprintf(buf + strlen(buf), "content-length: %d\r\n\r\n", length);
	send(cfd, buf, strlen(buf), 0);
	return 0;
}

char* getFileType( char* name)
{
	struct FileType {
		char* extension;
		char* contentType;
	};

	FileType fileTypes[] = {
		{".html", "text/html"},
		{".htm", "text/html"},
		{".txt", "text/plain"},
		{".jpg", "image/jpeg"},
		{".jpeg", "image/jpeg"},
		{".png", "image/png"},
		{".gif", "image/gif"},
		{".css", "text/css"},
		{".js", "application/javascript"},
		{".json", "application/json"},
		{".xml", "application/xml"},
		{".pdf", "application/pdf"},
		{".zip", "application/zip"},
		{".mp3", "audio/mpeg"},
		{".mp4", "video/mp4"},
		{".avi", "video/x-msvideo"},
		{".mov", "video/quicktime"},
		{".swf", "application/x-shockwave-flash"},
		{".ico", "image/x-icon"},
		{".svg", "image/svg+xml"},
		{".webp", "image/webp"},
		{nullptr, nullptr} // 表示结束
	};

	// 获取文件名的后缀
	const char* dot = strrchr(name, '.');
	if (dot == nullptr) {
		return "application/octet-stream"; // 默认类型
	}

	// 遍历对照表
	for (int i = 0; fileTypes[i].extension != nullptr; ++i) {
		if (strcmp(dot, fileTypes[i].extension) == 0) {
			return fileTypes[i].contentType;
		}
	}

	return "text/plain; charset=utf-8";//返回默认值
}

