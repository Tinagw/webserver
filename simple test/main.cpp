#include <cstdio>
#include<iostream>
#include<unistd.h>
#include<cstdlib>
#include"server.h"
int main(int argc,char* argv[])
{
    if (argc < 3)
    {
        printf("错误");
        return -1;
    }
    unsigned short port = atoi(argv[1]);
    //切换服务器工作目录
    chdir(argv[2]);
    //初始化用于监听的套接字
    int lfd = initListenFD(port);//0-65535
    //启动服务
    
    return 0;
}