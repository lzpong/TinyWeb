
#if TinyWeb_Function_Description //TinyWeb功能说明

auth lzpong 2016/11/24
功能基于 libuv 跨平台库

0.默认编码为 utf - 8
1.只支持使用GET方式
2.支持返回404错误页面
3.支持指定根目录（默认程序所在目录）
4.支持任意格式文件访问(带扩展名)
	a.支持静态网页访问：html / htm
	b.支持其他静态文件：js, css, png, jpeg / jpg, gif, ico, txt, xml, json, log, wam, wav, mp3, apk
	c.支持其他格式文件, 默认文件类型为："application/octet-stream"
5.支持默认index页面(index.html / index.html)，可以自定义设置
6.支持目录列表
7.支持回调
	b.404前回调（未找到页面 / 文件时回调, 此功能便于程序返回自定义功能）

== == == == == == == future
3.支持不带扩展名文件访问
4.支持POST
5.支持WebSocket
6.支持回调
	a.WebSocket 数据回调
7.支持cookie / session
8.支持认证
9.支持大文件响应（下载）

#endif


#include "tinyweb.h"
#include<stdlib.h>
#include<string.h>



//404前回调(未找到页面/文件时回调,此功能便于程序返回自定义功能)；返回0表示没有适合的处理请求，需要发送404错误
char on_request(void* data, uv_stream_t* client, tw_peerAddr* pa, tw_reqHeads* heads)
{
//	struct sockaddr_in serveraddr, peeraddr;
//	char serv_ip[17],peer_ip[17], tmp[1024];
//	int addrlen = sizeof(struct sockaddr);
//	int r;
//
//	//获取clientAddr: http://www.codes51.com/article/detail_113112.html
//	//本地接入地址
//	r = uv_tcp_getsockname((uv_tcp_t*)client, (struct sockaddr*)&serveraddr, &addrlen);
//	//网络字节序转换成主机字符序
//	uv_ip4_name(&serveraddr, (char*)serv_ip, sizeof(serv_ip));
//	//客户端的地址
//	r = uv_tcp_getpeername((uv_tcp_t*)client, (struct sockaddr*)&peeraddr, &addrlen);
//	//网络字节序转换成主机字符序
//	uv_ip4_name(&peeraddr, (char*)peer_ip, sizeof(peer_ip));
//
//	sprintf(tmp, "<h1>Page not found:</h1><url>%s<br>%s<br></url><br><br><br><i>server：%s:%d\t\tpeer：%s:%d</i>\n", heads->path, (heads->query?heads->query:""), serv_ip, ntohs(serveraddr.sin_port), peer_ip, ntohs(peeraddr.sin_port));
//#ifdef _MSC_VER //Windows下需要转换编码
//	size_t ll = strlen(tmp);
//	char *ch = GB2U8(tmp, &ll);
//	tw_send_200_OK(client, "text/html", ch, -1, 0);
//	free(ch);
//#else //linux 下，系统是和源代码文件编码都是是utf8的，就不需要转换
//	tw_send_200_OK(client, "text/html", tmp, -1, 0);
//#endif // _MSC_VER
//
	printf("  Query: %s\n",heads->query);
	printf("  Path: %s\n",heads->path);
	printf("  Host: %s\n",heads->host);
	printf("  Cookie: %s\n", heads->cookie);
	printf("  Range: %lld-%lld\n",heads->Range_frm,heads->Range_to);
	printf("  data(%lld): %s\n", heads->len,heads->data);
	return 0;
}

char on_socket_data(void* data, uv_stream_t* client, tw_peerAddr* pa, membuf_t* buf)
{
	if (buf->size < 1)
		return 1;//防止发生数据为空
	if (pa->flag & 0x2) { //WebSocket
		printf(buf->data, buf->size < 256 ? buf->size : 256);
#ifdef _MSC_VER //Windows下需要转换编码,因为windows系统的编码是GB2312
		if (pa->flag & 0x4) {
			size_t len;
			char *gb, *uc;
			len = buf->size;
			unsigned int l = len;
			gb = U82GB(buf->data, &l);
			len = l;

			printf("ws:%s\nlen=%zd\n", gb,len);

			free(gb);

			len = buf->size;
			l = len;
			uc = enc_u82u(buf->data, &l);
			len = l;
			len /= 2;
			l = len;
			gb = U2GB((wchar_t *)uc, &l);
			len = l;
			printf("-------------------------------------------ws1:len=%zd\n%s\n-------------------------------------------\n", len, gb);
			free(uc);
			free(gb);
		}
		else 
			//linux 下，系统和源代码文件编码都是是utf8的，就不需要转换
#endif // _MSC_VER
			printf("-------------------------------------------ws:len=%zd\n%s\n-------------------------------------------\n", buf->size, buf->data);
		ulong len = buf->size;
		char* p = WebSocketMakeFrame(buf->data, &len, 1);//文本帧
		tw_send_data(client, p, len, 0, 1);
	} else { //Socket
		printf("-------------------------------------------sk:len=%zd\n%s\n-------------------------------------------\n", buf->size, buf->data);
		tw_send_data(client, buf->data, buf->size, 1, 0);
	}
	return 1;
}

char on_close(void* data, uv_stream_t* client, tw_peerAddr* pa)
{
	printf("closed: sk=%zd [%s:%d]  flag=%d\n", pa->sk, pa->ip, pa->port,pa->flag);
	return 0;
}

char on_error(void* data, uv_stream_t* client, tw_peerAddr* pa, int errcode, char* errstr)
{
	printf("error: sk=%zd [%s:%d]  flag=%d  %s\n", pa->sk, pa->ip, pa->port,pa->flag, errstr);
	return 0;
}

char on_connect(void* data, uv_stream_t* client, tw_peerAddr* pa)
{
	printf("connected: sk=%zd [%s:%d]\n",pa->sk,pa->ip,pa->port);
	return 0;
}

int main(int argc, char** argv)
{
	int i;
	char cmd[1024];
	for (i = 0; i < argc; i++)
		printf("arg[%d]:%s\n",i,argv[i]);

	uv_loop_t* loop = uv_loop_new();
	//配置TinyWeb
	tw_config conf;
	memset(&conf, 0, sizeof(conf));
	conf.dirlist = 1;//目录列表
	//conf.ip = NULL;// "127.0.0.1";
	conf.port = 80;
	//conf.doc_dir = NULL;//默认程序文件所在目录
	if (argc > 1)
		conf.doc_dir = argv[1];
	conf.doc_index = NULL;//默认主页
	//
	conf.on_request = on_request;
	conf.on_data = on_socket_data;
	conf.on_close = on_close;
	conf.on_connect = on_connect;
	conf.on_error = on_error;

	//启动TinyWeb
	tinyweb_start(loop, &conf);
	//
	while (1) {
		fgets(cmd, 10, stdin);//the 'gets' function is dangerous and should not be used
		if (strcmpi(cmd, "Q\n") == 0 || strcmpi(cmd, "exit\n") == 0)
			break;
	}
	tinyweb_stop(loop);
	getchar();

	return 0;
}