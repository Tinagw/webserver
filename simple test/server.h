#pragma once
//��ʼ�����ڼ������׽���
int initListenFD(unsigned short port);
//����epoll
int epollRun(int lfd);
//�Ϳͻ��˽�������
int acceptClient(int lfd,int epfd);
//����http����
int recvHttpRequest(int cfd,int epfd);
//����������
int parseRequestLine(const char* line, int cfd);
//�����ļ����ͻ���
int sendfile(const char* filename, int cfd);