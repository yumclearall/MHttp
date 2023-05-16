#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>

//网络通信需要包含的头文件，需要加载的库文件
#include <WinSock2.h>
#pragma comment(lib, "WS2_32.lib")

#define PRINTF(str) printf("[%s - %d]"#str"=%s\n", __func__, __LINE__, str);
//void PRINTF(const char* str) {
//	printf("[%s - %d]%s", __func__, __LINE__, str);
//}

void error_die(const char* str) {	//报错信息
	perror(str);
	exit(1);
}


int startup(unsigned short* port) {	//传入指针，动态连接端口
							//向tinyhttpd服务器致敬。。。
	//实现网络初始化
	//返回值是一个套接字（服务器端套接字），端口

	//1.网络通信初始化
	WSADATA data;
	int ret = WSAStartup(
		MAKEWORD(1,1),	//1.1版本的协议
		&data
		);
	if (ret != 0) {
		error_die("WSAStsrtup");
	}

	//2.创建套接字
	int server_socket = socket(	PF_INET, //套接字的类型(网络套接字，文件套接字)
		SOCK_STREAM	,	//数据流和数据报
		IPPROTO_TCP	//协议
		);			//数据流，建立专用通道后传输，
					//数据报，决定路线后进行传输
	if (server_socket == -1) {
		//应该做的事情，打印错误提示并结束
		error_die("套接字");
	}

	//3.设置套接字属性，端口可复用
	int opt = 1;
	ret = setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
	if (ret == -1) {
		error_die("setsockopt");
	}

	//配置服务器端的网络地址
	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(server_addr));	//清零
	server_addr.sin_family = AF_INET;	//网络地址类型
	server_addr.sin_port = htons(*port);	//本地转网络short
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);	//都可以访问

	//绑定套接字和网络地址
	ret = bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr));
	if (ret < 0) {
		error_die("bind");
	}

	//如果port为0，动态分配一个端口
	if (*port == 0) {
		int namelen = sizeof(server_addr);
		ret = getsockname(server_socket, (sockaddr*) & server_addr, &namelen);
		if (ret < 0) {
			error_die("getsocketname");
		}

		*port = server_addr.sin_port;	//分配端口
	}


	//创建监听队列 
	if (listen(server_socket, 5) < 0) {
		error_die("listen");
	}

	return server_socket;

}

//从指定的客户端套接字，读取一行数据，保存到buff中
//返回实际读取到的字节数
int get_line(int sock, char* buff, int size) {

	char c = 0;
	int i = 0;
	while (i < size - 1 &&  c != '\n') {
		int n = recv(sock, &c,1 ,0);
		if (n > 0) {
			if (c == '\r') {
				n = recv(sock, &c, 1, MSG_PEEK );	//向后读一位
				if (n > 0 && c == '\n') {
					recv(sock, &c, 1, 0);
				}
				else {
					c = '\n';		//一些预错处理
				}
			}
			buff[i++] = c;
		}
		else {
			//
			c = '\n';
		}
	}

	buff[i] = 0;	//'\0'
	return i;
}

void unimplement(int client) {
	//向指定的套接字，发送一个提示还没有实现错误的界面
}

void not_found(int client) {
	//文件目录不存在

	//发送404响应
	char buff[1024];

	strcpy(buff, "HTTP/1.0 404 NOT FOUND\r\n");	//版本+空格+状态码+空格+短语+回车换行

	send(client, buff, strlen(buff), 0);

	strcpy(buff, "Server: RockHttpd/0.1\r\n"); //网页名字

	send(client, buff, strlen(buff), 0);


	strcpy(buff, "Content-type:text/html\n"); //消息内容类型
	send(client, buff, strlen(buff), 0);

	strcpy(buff, "\r\n"); //
	send(client, buff, strlen(buff), 0);

	//发送404网页内容
	sprintf(buff, 
		"<HTML>											\
			<TITLE>Not Found</TITLE>					\
			<BODY>										\
			<H2> The resource is unavailable.</H2>		\
			<img src = \"404.webp\" />					\
			</BODY>										\
		</HTML>");

	send(client, buff, strlen(buff), 0);
}

void headers(int client, const char* type) {
	//发送响应包的头信息
	char buff[1024];

	strcpy(buff, "HTTP/1.0 200 OK\r\n");	//版本+空格+状态码+空格+短语+回车换行

	send(client, buff, strlen(buff), 0);

	strcpy(buff, "Server: RockHttpd/0.1\r\n"); //网页名字

	send(client, buff, strlen(buff), 0);


	//strcpy(buff, "Content-type:text/html\n"); //消息内容类型
	//send(client, buff, strlen(buff), 0);
	char buf[1024];
	sprintf(buf, "Content-Type:%s\r\n", type);
	send(client, buf, strlen(buf), 0);

	strcpy(buff, "\r\n"); //
	send(client, buff, strlen(buff), 0);

}

void cat(int client, FILE * resource) {
	//发送文件
	char buff[4096];
	int count = 0;

	//fread(buff, sizeof(char),1, resource);	//一次读一个单元
	while (1) {
		int ret = fread(buff, sizeof(char), sizeof(buff), resource);
		if (ret <= 0) {
			break;
		}
		send(client, buff, ret, 0);
		count += ret;
	}

	printf("共发送[%d]字节给浏览器\n", count);

}

const char* getHeadType(const char* fileName) {
	const char* ret = "text/html";
	const char* p = strrchr(fileName, '.');
	if (!p)	return ret;

	p++;
	if (!strcmp(p, "css")) ret = "text/css";
	else if (!strcmp(p, "jpg")) ret = "image/jpeg";
	else if (!strcmp(p, "png")) ret = "image/png";
	else if (!strcmp(p, "js")) ret = "application/x-javascript";

	return ret;
}


void server_file(int client, const char* fileName) {
	//返回文件内容 
	//char numchars = 1;
	int numchars = 1;	//读的更多
	char buff[1024];

	//读完数据包的剩余数据
	while (numchars > 0 && strcmp(buff, "\n")) {
		numchars = get_line(client, buff, sizeof(buff));
		PRINTF(buff);
	}

	//FILE* resource = fopen(fileName, "r");
	FILE* resource;
	if (strcmp(fileName, "htdocs/index.html") == 0) {
		resource = fopen(fileName, "r");
	}
	else {
		resource = fopen(fileName, "rb");	//读图片文件
	}

	if (resource == NULL) {
		not_found(client);
	}
	else {
		
		//正式发送资源给浏览器
		headers(client, getHeadType(fileName));

		//发送请求的资源信息
		cat(client, resource);

		printf("资源发送完毕！\n");
	}

}

//处理用户请求的线程函数
DWORD WINAPI accept_request(LPVOID arg) {	//win系统线程函数

	//解析 收到的信息
	char buff[1024];	//1k

	int client = (SOCKET)arg;	//客户端套接字

	//读一行数据
	int numchars = get_line(client, buff, sizeof(buff));
	//printf("[%s - %d]%s", __func__, __LINE__, buff);
	//PRINTF(buff);

	char method[255];
	int j = 0, i = 0;
	while (!isspace(buff[j]) && i < sizeof(method) - 1) {
		method[i++] = buff[j++];
	}

	method[i] = 0;
	PRINTF(method);

	//检查请求方法 ， 本地服务器是否支持
	if (stricmp(method, "GET") && stricmp(method, "POST")) {//加i，不区分大小写
		unimplement(client);
		return 0;
	}

	//解析资源文件路径
	char url[255];	//存放请求的完整路径
	i = 0;
	while (isspace(buff[j]) && j < sizeof(buff)) {//跳过空格
		j++;
	}

	while (!isspace(buff[j]) && i < sizeof(url) - 1 && j < sizeof(buff)) {
		url[i++] = buff[j++];
	}
	url[i] = 0;
	PRINTF(url);


	// www.rock.com
	// 127.0.0.1
	// uri / 
	// htdocs/.../index.html

	char path[512] = "";
	sprintf(path, "htdocs%s", url);
	if (path[strlen(path) - 1] == '/') {
		strcat(path, "index.html");
	}
	PRINTF(path);

	struct stat status;
	if (stat(path, &status) == -1) {	//访问该目录失败
		while (numchars > 0 && strcmp(buff, "\n")) {	//读完后序内容
			numchars = get_line(client, buff, sizeof(buff));
		}

		not_found(client);
	}
	else {
		if (status.st_mode & S_IFMT == S_IFDIR) {	//如果是目录
			strcat(path, "/index.html");	//拼接文件
		}

		server_file(client, path);	//发送html文件
	}

	closesocket(client);	//关闭套接字

	return 0;
}

int main(void) {

	unsigned short port = 80;	//代码规范化，端口默认无符号短型
	int server_sock = startup(&port); //如果传入为0，自动分配端口
	printf("httpd服务已启动，正在监听 %d 端口...", port);


	struct sockaddr_in client_addr;		//网络地址
	int client_addr_len = sizeof(client_addr);	//地址长度
	//system("pause");
	while (1) {

		//通用接口 等待发起访问	阻塞式等待用户通过浏览器发起访问,
		//		同时生成一个新的套接字
		int client_sock =  accept(server_sock,
				(sockaddr*) & client_addr, &client_addr_len);
		if (client_sock == -1) {
			error_die("accept ! ! !");
		}

		// 使用client_sock对客户进行访问  X 
		// 进程（可以包含多个线程）
		// 创建新线程， 
		DWORD threadID = 0;
		CreateThread(0, 0, 
			accept_request, 
			(void*)client_sock, 
			0,&threadID);

	}
	//		"/"	 网站服务器资源目录下的 index.html
	closesocket(server_sock);
	return 0;
}