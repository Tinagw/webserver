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
int sendFile(const char* filename, int cfd);
//������Ӧͷ��״̬����Ӧͷ��
int sendHeadMsg(int cfd, int status, const char* descr,char *type,int length);
//content-type ���պ���
char* getFileType(char* name);
//����Ŀ¼
int sendDir(const char* dirName, int cfd);
