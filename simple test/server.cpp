#include "server.h"
#include<arpa/inet.h>//��������socket�ĸ����׽���ͷ�ļ�
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
	//1.�������ڼ������׽���
	int lfd = socket(AF_INET, SOCK_STREAM, 0);//ipv4Э��,��ʽ���Ǳ���
	if (lfd == -1) {
		perror("�����׽���ʧ��");
		return 0;
	}
    //2.���ö˿ڸ���
	int opt = 1;//���ö˿ڽ������Ϊ1min
	int ret = setsockopt(lfd, SOL_SOCKET,SO_REUSEADDR,&opt,sizeof opt);
	if (ret == -1) {
		perror("���ö˿ڸ���ʧ��");
		return 0;
	}
    //3.bind �˿�
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;//����Э��Ϊipv4
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;//���ð󶨱�������һip��ַ
	ret = bind(lfd, (struct sockaddr*)&addr, sizeof(addr));//���ļ����������󶨵�ַ���ṹ����ռ�ڴ��С
	if (ret == -1) {
			perror("��ʧ��ʧ��");
			return 0;
	}
	//4.���ö˿�
	ret = listen(lfd, 128);
	if (ret == -1) {
		perror("����ʧ��");
	}
	//5.����fd
	return lfd;
}

int epollRun(int lfd)
{
	//1.����epollʵ��
	int epfd = epoll_create(1);
	if (epfd == -1) {
		perror("����epollʵ��ʧ��");
	}
	//1.lfd����
	struct epoll_event ev;
	ev.data.fd = lfd;
	ev.events = EPOLLIN;
	int ret = epoll_ctl(epfd, EPOLL_CTL_ADD, lfd, &ev);
	if(ret == -1) {
		perror("lfd����ʧ��");
		return 0;
	}
	//3.���
	struct epoll_event evs [1024];
	while (true)
	{
		int num=epoll_wait(epfd,evs,sizeof evs/sizeof (struct epoll_event), -1);//ָ��Ϊ-1����ʾһֱ����
		for (int i = 0; i < num; i++) {
			int fd = evs[i].data.fd;
			if (fd == lfd) {//��������ڼ������ļ����������ͽ�������
                //����accept���������򲻻�����
				acceptClient(fd, epfd);
			}
			else {
				//��Ҫ�Ǵ�����ն�����
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
		perror("����ʧ��");
		return 0;
	}
	//���÷�����
	int flag = fcntl(cfd, F_GETFL);//��ȡ����
	flag |= O_NONBLOCK;//|=��λ������׷�ӷ���������
	fcntl(cfd,F_SETFL,flag);//���÷���������
	//����ļ���������epfd����
	struct epoll_event ev;
	ev.data.fd = cfd;
	ev.events = EPOLLIN|EPOLLET;//����ģʽ
	int ret = epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &ev);
	if (ret == -1) {
		perror("cfd���ʧ��");
		return 0;
	}
	return 0;
}

int recvHttpRequest(int cfd, int epfd)
{
	int len=0,total=0;
	char tmp[1024] = { 0 };//�������ݸ���
	char buf[4096] = { 0 };
	while ((len = recv(cfd, tmp, sizeof tmp, 0)) > 0) {
		if (total + len < sizeof buf)
		{
             memcpy(buf+total, tmp, len);
		}
		total += len;
	}
	//�ж��Ƿ����������
	if (len == -1 && errno == EAGAIN) {
		cout << "���ݽ������" <<endl;
		//����������
	}
	else if (len == 0) {
		//�ͻ��˶Ͽ�����
		cout << "�ͻ��˶Ͽ�����" << endl;
		//ɾ�����Ͻڵ�
		epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, NULL);
	}
	else {
		perror("���մ���");
	}
	return 0;
}

int parseRequestLine(const char* line, int cfd)
{
	//����������get /XXX/1.jpg http/1.1
	char method[12];
	char path[1024];
	sscanf(line, "%[^ ] %[^ ]", method, path);
	//����get����
	if (strcasecmp(method, "get")!=0) {
		return 0;
	}
	//����ͻ�������ľ�̬��Դ���ļ���Ŀ¼��
	char* file = NULL;
	if (strcmp(path, "/") == 0) {
		file = "./";
	}
	else {
		file = path + 1;
	}
	//��ȡ�ļ�����
	struct stat st;
	int ret = stat(file, &st);
	if (ret == -1) {
		cout << "�ļ�������" << endl;
		//�ظ�һ��404ҳ��
		sendHeadMsg(cfd, 404, "Not Found",getFileType(".hmtl"),-1);
		sendFile("404.html", cfd);
		return 0;
	}
	//�ж��ļ�����
	if (S_ISDIR(st.st_mode)) {//�����Ŀ¼����һ�����Ƿ�����
		//���ͱ���Ŀ¼����

	}
	else {
		//���ļ����ݷ��Ϳͻ���
		sendHeadMsg(cfd, 200, "OK", getFileType(file), st.st_size);
		sendFile(file, cfd);
	}
	return 0;
}

int sendFile(const char* filename, int cfd)
{
	//��һ���ַ�һ����
	//1.���ļ�
	int fd = open(filename, O_RDONLY);
#if 0
	assert(fd > 0);//�������0ֱ�ӹҵ�����
	while (1) {
		char buf[1024];
		int len = read(fd, buf, sizeof buf);
		if (len > 0) {
			send(cfd, buf, len, 0);
			usleep(10);//��λ��΢��,����ǳ���Ҫ����Ҫ�����ݷ���̫��
		}
		else if (len==0) {
			break;
		}
		else {
			perror("read");
		}
	}
#else
	int size = lseek(fd, 0, SEEK_END);//�ļ���������ƫ��������ʾ��ָ���Ƶ����
	sendfile(cfd, fd, NULL, size);
#endif
	return 0;
}

int sendHeadMsg(int cfd, int status, const char* descr, char* type, int length)
{
	//״̬��
	char buf[4096];
	sprintf(buf, "http/1.1 %d %s\r\n", status, descr);
	//��Ӧͷ
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
		{nullptr, nullptr} // ��ʾ����
	};

	// ��ȡ�ļ����ĺ�׺
	const char* dot = strrchr(name, '.');
	if (dot == nullptr) {
		return "application/octet-stream"; // Ĭ������
	}

	// �������ձ�
	for (int i = 0; fileTypes[i].extension != nullptr; ++i) {
		if (strcmp(dot, fileTypes[i].extension) == 0) {
			return fileTypes[i].contentType;
		}
	}

	return "text/plain; charset=utf-8";//����Ĭ��ֵ
}

