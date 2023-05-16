#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>

//����ͨ����Ҫ������ͷ�ļ�����Ҫ���صĿ��ļ�
#include <WinSock2.h>
#pragma comment(lib, "WS2_32.lib")

#define PRINTF(str) printf("[%s - %d]"#str"=%s\n", __func__, __LINE__, str);
//void PRINTF(const char* str) {
//	printf("[%s - %d]%s", __func__, __LINE__, str);
//}

void error_die(const char* str) {	//������Ϣ
	perror(str);
	exit(1);
}


int startup(unsigned short* port) {	//����ָ�룬��̬���Ӷ˿�
							//��tinyhttpd�������¾�������
	//ʵ�������ʼ��
	//����ֵ��һ���׽��֣����������׽��֣����˿�

	//1.����ͨ�ų�ʼ��
	WSADATA data;
	int ret = WSAStartup(
		MAKEWORD(1,1),	//1.1�汾��Э��
		&data
		);
	if (ret != 0) {
		error_die("WSAStsrtup");
	}

	//2.�����׽���
	int server_socket = socket(	PF_INET, //�׽��ֵ�����(�����׽��֣��ļ��׽���)
		SOCK_STREAM	,	//�����������ݱ�
		IPPROTO_TCP	//Э��
		);			//������������ר��ͨ�����䣬
					//���ݱ�������·�ߺ���д���
	if (server_socket == -1) {
		//Ӧ���������飬��ӡ������ʾ������
		error_die("�׽���");
	}

	//3.�����׽������ԣ��˿ڿɸ���
	int opt = 1;
	ret = setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
	if (ret == -1) {
		error_die("setsockopt");
	}

	//���÷������˵������ַ
	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(server_addr));	//����
	server_addr.sin_family = AF_INET;	//�����ַ����
	server_addr.sin_port = htons(*port);	//����ת����short
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);	//�����Է���

	//���׽��ֺ������ַ
	ret = bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr));
	if (ret < 0) {
		error_die("bind");
	}

	//���portΪ0����̬����һ���˿�
	if (*port == 0) {
		int namelen = sizeof(server_addr);
		ret = getsockname(server_socket, (sockaddr*) & server_addr, &namelen);
		if (ret < 0) {
			error_die("getsocketname");
		}

		*port = server_addr.sin_port;	//����˿�
	}


	//������������ 
	if (listen(server_socket, 5) < 0) {
		error_die("listen");
	}

	return server_socket;

}

//��ָ���Ŀͻ����׽��֣���ȡһ�����ݣ����浽buff��
//����ʵ�ʶ�ȡ�����ֽ���
int get_line(int sock, char* buff, int size) {

	char c = 0;
	int i = 0;
	while (i < size - 1 &&  c != '\n') {
		int n = recv(sock, &c,1 ,0);
		if (n > 0) {
			if (c == '\r') {
				n = recv(sock, &c, 1, MSG_PEEK );	//����һλ
				if (n > 0 && c == '\n') {
					recv(sock, &c, 1, 0);
				}
				else {
					c = '\n';		//һЩԤ����
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
	//��ָ�����׽��֣�����һ����ʾ��û��ʵ�ִ���Ľ���
}

void not_found(int client) {
	//�ļ�Ŀ¼������

	//����404��Ӧ
	char buff[1024];

	strcpy(buff, "HTTP/1.0 404 NOT FOUND\r\n");	//�汾+�ո�+״̬��+�ո�+����+�س�����

	send(client, buff, strlen(buff), 0);

	strcpy(buff, "Server: RockHttpd/0.1\r\n"); //��ҳ����

	send(client, buff, strlen(buff), 0);


	strcpy(buff, "Content-type:text/html\n"); //��Ϣ��������
	send(client, buff, strlen(buff), 0);

	strcpy(buff, "\r\n"); //
	send(client, buff, strlen(buff), 0);

	//����404��ҳ����
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
	//������Ӧ����ͷ��Ϣ
	char buff[1024];

	strcpy(buff, "HTTP/1.0 200 OK\r\n");	//�汾+�ո�+״̬��+�ո�+����+�س�����

	send(client, buff, strlen(buff), 0);

	strcpy(buff, "Server: RockHttpd/0.1\r\n"); //��ҳ����

	send(client, buff, strlen(buff), 0);


	//strcpy(buff, "Content-type:text/html\n"); //��Ϣ��������
	//send(client, buff, strlen(buff), 0);
	char buf[1024];
	sprintf(buf, "Content-Type:%s\r\n", type);
	send(client, buf, strlen(buf), 0);

	strcpy(buff, "\r\n"); //
	send(client, buff, strlen(buff), 0);

}

void cat(int client, FILE * resource) {
	//�����ļ�
	char buff[4096];
	int count = 0;

	//fread(buff, sizeof(char),1, resource);	//һ�ζ�һ����Ԫ
	while (1) {
		int ret = fread(buff, sizeof(char), sizeof(buff), resource);
		if (ret <= 0) {
			break;
		}
		send(client, buff, ret, 0);
		count += ret;
	}

	printf("������[%d]�ֽڸ������\n", count);

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
	//�����ļ����� 
	//char numchars = 1;
	int numchars = 1;	//���ĸ���
	char buff[1024];

	//�������ݰ���ʣ������
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
		resource = fopen(fileName, "rb");	//��ͼƬ�ļ�
	}

	if (resource == NULL) {
		not_found(client);
	}
	else {
		
		//��ʽ������Դ�������
		headers(client, getHeadType(fileName));

		//�����������Դ��Ϣ
		cat(client, resource);

		printf("��Դ������ϣ�\n");
	}

}

//�����û�������̺߳���
DWORD WINAPI accept_request(LPVOID arg) {	//winϵͳ�̺߳���

	//���� �յ�����Ϣ
	char buff[1024];	//1k

	int client = (SOCKET)arg;	//�ͻ����׽���

	//��һ������
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

	//������󷽷� �� ���ط������Ƿ�֧��
	if (stricmp(method, "GET") && stricmp(method, "POST")) {//��i�������ִ�Сд
		unimplement(client);
		return 0;
	}

	//������Դ�ļ�·��
	char url[255];	//������������·��
	i = 0;
	while (isspace(buff[j]) && j < sizeof(buff)) {//�����ո�
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
	if (stat(path, &status) == -1) {	//���ʸ�Ŀ¼ʧ��
		while (numchars > 0 && strcmp(buff, "\n")) {	//�����������
			numchars = get_line(client, buff, sizeof(buff));
		}

		not_found(client);
	}
	else {
		if (status.st_mode & S_IFMT == S_IFDIR) {	//�����Ŀ¼
			strcat(path, "/index.html");	//ƴ���ļ�
		}

		server_file(client, path);	//����html�ļ�
	}

	closesocket(client);	//�ر��׽���

	return 0;
}

int main(void) {

	unsigned short port = 80;	//����淶�����˿�Ĭ���޷��Ŷ���
	int server_sock = startup(&port); //�������Ϊ0���Զ�����˿�
	printf("httpd���������������ڼ��� %d �˿�...", port);


	struct sockaddr_in client_addr;		//�����ַ
	int client_addr_len = sizeof(client_addr);	//��ַ����
	//system("pause");
	while (1) {

		//ͨ�ýӿ� �ȴ��������	����ʽ�ȴ��û�ͨ��������������,
		//		ͬʱ����һ���µ��׽���
		int client_sock =  accept(server_sock,
				(sockaddr*) & client_addr, &client_addr_len);
		if (client_sock == -1) {
			error_die("accept ! ! !");
		}

		// ʹ��client_sock�Կͻ����з���  X 
		// ���̣����԰�������̣߳�
		// �������̣߳� 
		DWORD threadID = 0;
		CreateThread(0, 0, 
			accept_request, 
			(void*)client_sock, 
			0,&threadID);

	}
	//		"/"	 ��վ��������ԴĿ¼�µ� index.html
	closesocket(server_sock);
	return 0;
}