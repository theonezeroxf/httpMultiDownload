#include<stdio.h>
#include<string.h>
#include<stdlib.h>
 
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<netdb.h>
#include <fcntl.h>

#include <sys/mman.h>
#include <pthread.h>
 
#define HTTP_VERSION "HTTP/1.1"
//#define CONNECTION_TYPE "Connection:close\r\n"
#define CONNECTION_TYPE "Connection: close"
#define BUFFER_SIZE 4096
 
char *mm_dst;
char* ip;
const char* resource2;
int get_line(int cfd, char *buf, int size)
{
    int i = 0;
    char c = '\0';
    int n;
    while ((i < size-1) && (c != '\n')) {  
        n = recv(cfd, &c, 1, 0);
        if (n > 0) {     
            if (c == '\r') {            
                n = recv(cfd, &c, 1, MSG_PEEK);
                if ((n > 0) && (c == '\n')) {              
                    recv(cfd, &c, 1, 0);
                } else {                       
                    c = '\n';
                }
            }
            buf[i] = c;
            i++;
        } else {      
            c = '\n';
        }
    }
    buf[i] = '\0';
    
    if (-1 == n)
    	i = n;

    return i;
}

int get_header(int cfd,char *buf,int size){
	int n=0;
	memset(buf,0,BUFFER_SIZE);
	printf("\r\nhead:\n");
	while((n=get_line(cfd,buf,size))>0){
		if(buf[0]=='\n')	break;
		printf(buf);
		memset(buf,0,BUFFER_SIZE);
	}
	printf("head end\n");
	return -1;
}

int get_len(int cfd,char *buf,int size){
	int n=0;
	char key[16];
	char value[16];
	memset(buf,0,BUFFER_SIZE);
	while((n=get_line(cfd,buf,size))>0){
		if(buf[0]=='\n')	break;
		sscanf(buf,"%[^:]: %[^ ]",key,value);
		if(strcmp(key,"Content-Length")==0)	{close(cfd); return atoi(value);}
		memset(buf,0,BUFFER_SIZE);
		memset(key,0,16);
		memset(value,0,16);
	}
	return -1;
}
char *get_name(const char *str){
	int i=0;
	int len=strlen(str);
	for(i=len-1;str[i]!='/';i--);
	return str+i+1;
}
int get_body(int cfd,char *buf,int size){
	int len=0;
	memset(buf,0,BUFFER_SIZE);
    printf("\r\nbody:\n");
	while((len=recv(cfd,buf,BUFFER_SIZE,0))>0){
		printf(buf);
		memset(buf,0,BUFFER_SIZE);
	}
	printf("body end\n");
	return len;

}

char* host_to_ip(const char* hostname){
    struct hostent *host_entry=gethostbyname(hostname);
    //14.215.177.39 -->usigned int(��Ϊipv4��32λ�ģ�ÿ����ָ�,ֵΪ0~255.���ÿ�����unsigned int��ʾ��ÿ8λ��ʾһ��0~255����4��)
    //inet_nota �ǰ� 0x12121212 --->"18.18.18.18" Ҳ���ǽ������ַת���ɡ�.��������ַ�����ʽ
    if(host_entry){
        return inet_ntoa(*(struct in_addr*)*host_entry->h_addr_list);//����h_addr_list�����м���ip��ַ�����ȡ��һ��,��˼�*,Ȼ��ǿת��in_addr*,����inet_ntoaΪֵ���ݣ���˻�Ҫ��*
    }
    return NULL;
}
 
int http_create_socket(char* ip){
    int sockfd=socket(AF_INET,SOCK_STREAM,0);//sockfd�Ľ������int���ͣ���һ���ļ������TCPҪ��SOCK_STREAM,UDP��SOCK_DGRAM
    struct sockaddr_in sin={0};
    sin.sin_family=AF_INET;
    sin.sin_port=htons(80);//httpЭ��˿�Ϊ80
    sin.sin_addr.s_addr=inet_addr(ip);//"�ַ�����ip"��ַתΪuint32_t�������inet_ntoa�����෴
 
    // int ret=connect(sockfd,(sockaddr*)&sin,sizeof(sockaddr_in));
    // if(ret!=0) return -1;
    if (0 != connect(sockfd, (struct sockaddr*)&sin, sizeof(struct sockaddr_in))) {
        return -1;
    }
 
    fcntl(sockfd,F_SETFL,O_NONBLOCK);//����Ϊ������(���socket��������read(),�������������socket�Ƿ�������read()Ҳ�����̷���)�������ļ�״̬��־��F_GETFL����ȡ�ļ�״̬��־��F_SETFL��
    return sockfd;
}
 int http_create_socket2(char* ip){
    int sockfd=socket(AF_INET,SOCK_STREAM,0);//sockfd�Ľ������int���ͣ���һ���ļ������TCPҪ��SOCK_STREAM,UDP��SOCK_DGRAM
    struct sockaddr_in sin={0};
    sin.sin_family=AF_INET;
    sin.sin_port=htons(80);//httpЭ��˿�Ϊ80
    sin.sin_addr.s_addr=inet_addr(ip);//"�ַ�����ip"��ַתΪuint32_t�������inet_ntoa�����෴
 
    if (0 != connect(sockfd, (struct sockaddr*)&sin, sizeof(struct sockaddr_in))) {
        return -1;
    }
    return sockfd;
}

//hostname:github.com
//resource:/wangbojing
char* http_send_request(const char* hostname,const char* resource){
    char* ip=host_to_ip(hostname);
    int sockfd=http_create_socket(ip);
 
    char buffer[BUFFER_SIZE]={0};
    //sprintf(buffer,"GET %s %s\r\n\Host:%s\r\n\%s\r\n\\r\n",resource,HTTP_VERSION,hostname,CONNECTION_TYPE);
	sprintf(buffer,"GET %s %s\r\n\Host:%s\r\n%s\r\n\r\n",resource,HTTP_VERSION,hostname,CONNECTION_TYPE);
    send(sockfd,buffer,strlen(buffer),0);
 
 
    // recv()//��Ϊ�Ƿ�����״̬�������recv����ٹ�ȥ
 
    //select
    fd_set fdread;
    FD_ZERO(&fdread);//�������fd_set���ϣ�����fd_set���ϲ��ٰ����κ��ļ����
    FD_SET(sockfd,&fdread);//��sockfd��Ϊ����״̬(������һ���������ļ����������뼯��֮��)
 
    struct timeval tv;
    tv.tv_sec=5;//5���� select�ɼ�һ��
    tv.tv_usec=0;
 
 
    char* result=(char*)malloc(sizeof(int));
    memset(result,0,sizeof(int));
    while(1){
        int selection=select(sockfd+1,&fdread,NULL,NULL,&tv);//��һ������Ϊ�����ļ������������ֵ+1   ��selectȥ��⣬���io������������
        if(!selection||!FD_ISSET(sockfd,&fdread)){//FD_ISSET�ж�������fd�Ƿ��ڸ�������������fdset��
            break;
        }else{
            memset(buffer,0,BUFFER_SIZE);
            int len=recv(sockfd,buffer,BUFFER_SIZE,0);//��������,����buffer��
            if(len==0){//disconnect
                break;
            }
            result=(char*)realloc(result,(strlen(result)+len+1)*sizeof(char));//���·����ڴ��ؽ�(+1��Ϊ��ĩβ'\0',�������һ���Ļ��ᱻ����)
            strncat(result,buffer,len);//��buffer����copy��resulut��
        }
    }
    return result;
 
}

 void *tfn(void* arg){
	char s[16]={0};
	char e[16]={0};
	char *p=(char*)arg;
	int cfd=http_create_socket2(ip);
	char head[BUFFER_SIZE]={0};
	char buf[BUFFER_SIZE]={0};
	
	sscanf(p,"Range: bytes=%[^-]-%[^]",s,e);
	int start=atoi(s);

	sprintf(head,"GET %s %s\r\n%s\r\n%s\r\n\r\n",resource2,HTTP_VERSION,CONNECTION_TYPE,p);
//	printf("request head:\n%s",head);
	send(cfd,head,strlen(head),0);

	int len=0;
	memset(buf,0,BUFFER_SIZE);
	get_header(cfd,buf,BUFFER_SIZE);
//	len=recv(cfd,buf,BUFFER_SIZE,0);
//	printf("%s",buf);
	while((len=recv(cfd,buf,BUFFER_SIZE,0))>0){
		
		strncpy(mm_dst+start,buf,len);
//		printf("%s",buf);
		memset(buf,0,BUFFER_SIZE);
		start+=len;
	}
	close(cfd);
	pthread_exit(0);
}


int http_send_request2(const char* hostname,const char* resource){
  
	ip=host_to_ip(hostname);
    int sockfd=http_create_socket2(ip);
	char range[3][64]; 
	char buf[BUFFER_SIZE]={0};
	sprintf(buf,"GET %s %s\r\n\Host:%s\r\n%s\r\n\r\n",resource,HTTP_VERSION,hostname,CONNECTION_TYPE);
    send(sockfd,buf,strlen(buf),0);
	char* name=get_name(resource);
	int total_len=get_len(sockfd,buf,BUFFER_SIZE);
    close(sockfd);
	printf("name=%s,len=%d\n",name,total_len);
	//Range: bytes=0-499
	int avg=total_len/3;
	int i=0;
	for(i=0;i<(3-1);i++){
		int start=i*avg;
		int end=(i+1)*avg-1;
		sprintf(range[i],"Range: bytes=%d-%d",start,end);
//		printf("main:%s\n",range[i]);
	}
	int start=i*avg;
	sprintf(range[i],"Range: bytes=%d-",start);
//	printf("main:%s\n",range[i]);
	chdir(".");
	int fd = open(name,O_CREAT|O_RDWR);
	ftruncate(fd,total_len);
	mm_dst=mmap(NULL,total_len,PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	pthread_t tid[3];
	for(i=0;i<3;i++) pthread_create(&tid[i],NULL,tfn,(void*)range[i]);
	for(i=0;i<3;i++) pthread_join(tid[i],NULL);
	munmap(mm_dst,total_len);
	close(fd);
    return 0;
 
}


int main(int argc,char* argv[]){
    if(argc<3) return -1;
	resource2=argv[2];
    http_send_request2(argv[1],argv[2]);
   
}