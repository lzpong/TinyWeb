
#if TinyWeb_Function_Description //TinyWeb功能说明

auth lzpong 2016/11/24
功能基于 libuv 跨平台库

0.支持设置文档编码,默认 utf-8
1.支持使用HTTP: GET/POST方式访问
2.支持Socket, WebSocket 连接
3.支持返回404错误页面
4.支持指定根目录（默认程序所在目录）
5.支持任意格式文件访问(带/不带扩展名, 文件下载)
	a.支持静态网页访问：html/htm
	b.支持其他静态文件：js, css, png, jpeg/jpg, gif, ico, txt, xml, json, log, wam, wav, mp3, mp4, apk 等
	c.支持其他文件格式, 默认文件类型为："application/octet-stream"
	d.支持不带扩展名文件访问
	e.支持 Range 请求参数下载大文件(Range: bytes=sizeFrom-[sizeTo],支持负反向计算)
6.支持默认index页面(index.html/index.htm)，可以自定义设置
7.支持目录列表
8.不允许访问根目录上级文件或文件夹
9.支持回调
	a.接收到HTTP请求后先回调（此功能便于程序返回自定义功能）,回调失败或返回0时执行普通http响应
	b.WebSocket 数据回调
	c.socket 数据回调
10.支持x64,支持超过2G大文件
11.支持cookie/setcookie
12.支持添加自定义头部信息
==============stable

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
	printf("  sk:%zd Request:\n",pa->sk);
	printf("  Query: %s\n",heads->query);
	printf("  Path: %s\n",heads->path);
	printf("  Host: %s\n",heads->host);
	printf("  Cookie: %s\n", heads->cookie);
	printf("  Range: %lld-%lld\n",heads->Range_frm,heads->Range_to);
	printf("  data(%zd): %s\n", heads->len,heads->data);
	if (!heads->cookie)
	{
		char ck[512];
		tw_make_setcookie(ck, 255,"TINYSSID","FDSAFdfafdsafds", 3600 * 8, NULL, heads->path);
		tw_make_setcookie(ck + strlen(ck),255,"TINYSSID2","faFDSAF45dsafds",  0, heads->host, NULL);
		sprintf(ck + strlen(ck), "WWW-Authenticate: Basic realm=\".\"\r\n");
		size_t len;
		char* rp = tw_format_http_respone(client, "401 Unauthorized", ck, "text/plan", "", -1, &len);
		tw_send_data(client, rp, len, 0, 1);
		return 1;
	}
	return 0;
}

char on_socket_data(void* data, uv_stream_t* client, tw_peerAddr* pa, membuf_t* buf)
{
	if (buf->size < 1)
		return 1;//防止发生数据为空

	if (pa->flag & 0x2) { //WebSocket
		//printf(buf->data, buf->size < 256 ? buf->size : 256);
#ifdef _MSC_VER //Windows下需要转换编码,因为windows系统的编码是GB2312
		//if (pa->flag & 0x4) {
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
			printf("-------------------------------------------ws1:%zd  dlen=%zd\n%s\n-------------------------------------------\n",pa->sk, len, gb);
			free(uc);
			free(gb);
		//}
		//else
			//linux 下，系统和源代码文件编码都是是utf8的，就不需要转换
#endif // _MSC_VER
			printf("-------------------------------------------ws:%zd  dlen=%zd\n%s\n-------------------------------------------\n",pa->sk, buf->size, buf->data);
		//ulong len = buf->size;
		//char* p = WebSocketMakeFrame(buf->data, &len, 1);//文本帧
		tw_send_data(client, buf->data, buf->size, 1, 0);
	} else { //Socket
		printf("-------------------------------------------sk:%zd  dlen=%zd\n%s\n-------------------------------------------\n",pa->sk, buf->size, buf->data);
		tw_send_data(client, buf->data, buf->size, 1, 0);
	}
	return 1;
}

char on_close(void* data, uv_stream_t* client, tw_peerAddr* pa)
{
	printf("closed: sk=%zd [%s:%d]  from:%s:%d    cli:%d\n", pa->sk, pa->ip, pa->port, pa->fip, pa->fport, client->loop->active_tcp_streams);
	return 0;
}

char on_error(void* data, uv_stream_t* client, tw_peerAddr* pa, int errcode, char* errstr)
{
	printf("error: sk=%zd [%s:%d]  from:%s:%d    cli:%d   %s\n", pa->sk, pa->ip, pa->port, pa->fip, pa->fport, client->loop->active_tcp_streams,errstr);
	return 0;
}

char on_connect(void* data, uv_stream_t* client, tw_peerAddr* pa)
{
	printf("connected: sk=%zd [%s:%d]  from:%s:%d    cli:%d\n",pa->sk,pa->ip,pa->port,pa->fip,pa->fport, client->loop->active_tcp_streams);
	return 0;
}
const char* help = 
"TinyWeb v1.2.2 创建迷你Web(静态)\n"
"\n"
"TinyWeb  [[dir|port] | [-d dir] [-d port]]\n"
"\n"
"Use Like:\n"
"TinyWeb           使用当前目录,使用默认80端口\n"
"TinyWeb  -p por   使用当前目录,使用[port]端口\n"
"TinyWeb  [port]   使用当前目录,使用[port]端口\n"
"TinyWeb  -d dir   使用[dir]目录,使用默认80端口\n"
"TinyWeb  [dir]    使用[dir]目录,使用默认80端口\n"
"TinyWeb  -d dir -p port\n"
"                  使用[dir]目录,使用[port]端口\n"
"TinyWeb  -d dir1 -p port1 -d dir2 -p port2 [-d dir -p port]\n"
"                  使用指定的目录和端口,创建多个Web\n";

void startWeb(char* dir, int port)
{
	uv_loop_t* loop = uv_loop_new();
	//配置TinyWeb
	tw_config conf;
	memset(&conf, 0, sizeof(conf));
	conf.dirlist = 1;//目录列表
	//conf.ip = NULL;// "127.0.0.1";
	conf.port = port;
	//conf.doc_dir = NULL;//默认程序文件所在目录
	conf.doc_dir = dir;
	conf.doc_index = NULL;//默认主页
	//
	conf.on_request = on_request;
	conf.on_data = on_socket_data;
	conf.on_close = on_close;
	conf.on_connect = on_connect;
	conf.on_error = on_error;
	//启动TinyWeb
	tinyweb_start(loop, &conf);
}

int main(int argc, char** argv)
{
	int i, t, port=80;
	char * dir=NULL;
	char cmd[11];
	if (argc < 5) { //cmd: "", "dir", "port", "-d dir", "-p port"
		for (i = 1; i < argc;) {
			if (strcmpi(argv[i], "-d") == 0) {
				if (++i < argc)
					dir = argv[i++];
				else
					return printf("need dir\n%s", help), 0;
			}
			if (i < argc && strcmpi(argv[i], "-p") == 0) {
				if ( ++i < argc)
					port = atoi(argv[i++]);
				else
					return printf("need port\n%s", help), 0;
			}
			if (i < argc) {
				t = atoi(argv[i]);
				if (t)
					port=t,i++;
				else
					dir = argv[i++];
			}
		}
		startWeb(dir, port);
	}
	else {
		for (i = 1; i < argc;)
		{
			port = 0;
			dir = NULL;
			if (strcmpi(argv[i], "-d") == 0 && ++i < argc)
				dir = argv[i++];
			else if (strcmpi(argv[i], "-p") == 0 && ++i < argc)
				port = atoi(argv[i++]);
			else
				return printf(help), 0;
			if (strcmpi(argv[i], "-d") == 0 && ++i < argc)
				dir = argv[i++];
			else if (strcmpi(argv[i], "-p") == 0 && ++i < argc)
				port = atoi(argv[i++]);
			else
				return printf(help), 0;
			if(port && dir)
				startWeb(dir, port);
			else
				return printf(help), 0;
		}
	}
	//
	while (1) {
		fgets(cmd, 10, stdin);//the 'gets' function is dangerous and should not be used
		if (strcmpi(cmd, "Q\n") == 0 || strcmpi(cmd, "exit\n") == 0)
			break;
	}
	//tinyweb_stop(loop);
	getchar();

	return 0;
}