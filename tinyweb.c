
#include "tinyweb.h"
#include "tools.h"

#ifdef __GNUC__
#include <uv.h>
#endif // __GNUC__

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <memory.h>

//TinyWeb 增加与完善功能，by lzpong 2016/11/24

uv_tcp_t    _server;

tw_config tw_conf;


//=================================================


//关闭客户端连接后，释放客户端连接的数据
static void after_uv_close_client(uv_handle_t* client) {
	membuf_t* cliInfo = (membuf_t*)client->data;
	//如果是long-link
	if (cliInfo->flag & 0x1) {
		if (cliInfo->flag & 0x2) { //WebSocket
			if (cliInfo->data) {
				WebSocketHandle* hd = (WebSocketHandle*)cliInfo->data;
				membuf_uninit(&hd->buf);
			}
		}
	}
	//清理
	membuf_uninit(cliInfo); //see: tw_on_connection()
	free(client->data); //membuf_t*: request buffer
	free(client);
}

//关闭客户端连接
void tw_close_client(uv_stream_t* client) {
	//关闭连接回调
	if (tw_conf.on_close)
		tw_conf.on_close(client, ((membuf_t*)client->data)->flag);
	uv_close((uv_handle_t*)client, after_uv_close_client);
}

//发送数据后,free数据，关闭客户端连接
static void after_uv_write(uv_write_t* w, int status) {
	if (w->data)
		free(w->data); //copyed data
	//长连接就不关闭了
	membuf_t* cliInfo = (membuf_t*)w->handle->data;
	if (!(cliInfo->flag & 0x1))
		uv_close((uv_handle_t*)w->handle, after_uv_close_client);
	free(w);
}

//发送数据到客户端; 如果是短连接,则发送完后会关闭连接
//data：待发送数据
//len： 数据长度, -1 将自动计算数据长度
//need_copy_data：是否需要复制数据
//need_free_data：是否需要free数据, 如果need_copy_data非零则忽略此参数
void tw_send_data(uv_stream_t* client, const void* data, unsigned int len, int need_copy_data, int need_free_data) {
	uv_buf_t buf;
	uv_write_t* w;
	void* newdata = (void*)data;

	if (data == NULL || len == 0) return;
	if (len == (unsigned int)-1)
		len = strlen((char*)data);

	if (need_copy_data) {
		newdata = malloc(len);
		memcpy(newdata, data, len);
	}

	buf = uv_buf_init((char*)newdata, len);
	w = (uv_write_t*)malloc(sizeof(uv_write_t));
	w->data = (need_copy_data || need_free_data) ? newdata : NULL;
	uv_write(w, client, &buf, 1, after_uv_write); //free w and w->data in after_uv_write()
}

//发送'200 OK' 响应; 不会释放(free)传入的数据(u8data)
//content_type：Content Type 文档类型
//u8data：utf-8编码的数据
//content_length：数据长度，为-1时自动计算(strlen)
//respone_size：获取响应最终发送的数据长度，为0表示放不需要取此长度
void tw_send_200_OK(uv_stream_t* client, const char* content_type, const void* u8data, int content_length, int* respone_size)
{
	int repSize;
	char *data = tw_format_http_respone("200 OK", "text/html", u8data, content_length, &repSize);
	tw_send_data(client, data, repSize, 0, 1);//发送后free data
	if (respone_size)
		*respone_size = repSize;
}


//返回格式华的HTTP响应内容（需要free返回数据）
//status: "200 OK"
//content_type: 文件类型，如："text/html" ；可以调用tw_get_content_type()得到
//content: any utf-8 data, need html-encode if content_type is "text/html"
//content_length: can be -1 if content is c_str (end with NULL)
//respone_size: if not NULL,可以获取发送的数据长度 the size of respone will be writen to request
//returns malloc()ed c_str, need free() by caller
char* tw_format_http_respone(const char* status, const char* content_type, const char* content, int content_length, int* respone_size) {
	int totalsize, header_size;
	char* respone;
	if (content_length < 0)
		content_length = content ? strlen(content) : 0;
	totalsize = strlen(status) + strlen(content_type) + content_length + 128;
	respone = (char*)malloc(totalsize);
	header_size = sprintf(respone, "HTTP/1.1 %s\r\nServer: TinyWeb\r\nConnection: close\r\nContent-Type:%s;charset=utf-8\r\nContent-Length:%d\r\n\r\n",
		status, content_type, content_length);
	assert(header_size > 0);
	if (content) {
		memcpy(respone + header_size, content, content_length);
	}
	if (respone_size)
		*respone_size = header_size + content_length;
	return respone;
}

//发送404响应
static void tw_404_not_found(uv_stream_t* client, const char* pathinfo) {
	char* respone;
	char buffer[128];
	snprintf(buffer, sizeof(buffer), "<h1>404 Not Found</h1><p>%s</p>", pathinfo);
	respone = tw_format_http_respone("404 Not Found", "text/html", buffer, -1, NULL);
	tw_send_data(client, respone, -1, 0, 1);
}
//发送301响应
//
static void tw_301_Moved(uv_stream_t* client, reqHeads heads) {
	int len=76+strlen(heads.path);
	char buffer[512];
	snprintf(buffer, sizeof(buffer), "HTTP/1.1 301 Moved Permanently\r\nServer: TinyWeb\r\nLocation: HTTP://%s%s/\r\nConnection: close\r\n"
		"Content-Type:text/html;charset=utf-8\r\nContent-Length:%d\r\n\r\n<h1>Moved Permanently</h1><p>The document has moved <a href=\"%s\">here</a>.</p>", heads.host, heads.path,len, heads.path);
	tw_send_data(client, buffer, -1, 1, 1);
}

//发送文件到客户端
static char tw_http_send_file(uv_stream_t* client, const char* content_type, const char* file, const char* reqPath) {
	size_t file_size, read_bytes;
	int respone_size;
	char *file_data, *respone;
	FILE* fp = fopen(file, "rb");
	if (fp) {
		fseek(fp, 0, SEEK_END);
		file_size = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		file_data = (char*)malloc(file_size);
		read_bytes = fread(file_data, file_size, 1, fp);
		assert(read_bytes == file_size);
		fclose(fp);

		respone_size = 0;
		respone = tw_format_http_respone("200 OK", content_type, file_data, file_size, &respone_size);
		free(file_data);
		tw_send_data(client, respone, respone_size, 0, 1);
		return 1;
	}
	else {
		tw_404_not_found(client, reqPath);
		return 0;
	}
}

//根据扩展名(不区分大小写)，返回文件类型 content_type
const char* tw_get_content_type(const char* fileExt) {
	const static char* octet = "application/octet-stream";
	if (fileExt)
	{
		const char* p = strrchr(fileExt, '.');
		if (p)
		{
			const char* p2 = strrchr(fileExt, '\\');
			if (p2 == NULL) p2 = strrchr(fileExt, '/');
			if (p2 && p2 < p) {
				fileExt = p;
			}
			else
				return octet;
		}
		else
			return octet;
	}
	if (strcmpi(fileExt, "htm") == 0 || strcmpi(fileExt, "html") == 0)
		return "text/html";
	else if (strcmpi(fileExt, "js") == 0)
		return "text/javascript";
	else if (strcmpi(fileExt, "css") == 0)
		return "text/css";
	else if (strcmpi(fileExt, "json") == 0)
		return "text/json";
	else if (strcmpi(fileExt, "log") == 0 || strcmpi(fileExt, "txt") == 0)
		return "text/plain";
	else if (strcmpi(fileExt, "jpg") == 0 || strcmpi(fileExt, "jpeg") == 0)
		return "image/jpeg";
	else if (strcmpi(fileExt, "png") == 0)
		return "image/png";
	else if (strcmpi(fileExt, "gif") == 0)
		return "image/gif";
	else if (strcmpi(fileExt, "ico") == 0)
		return "image/x-icon";
	else if (strcmpi(fileExt, "xml") == 0)
		return "application/xml";
	else if (strcmpi(fileExt, "wav") == 0)
		return "audio/wav";
	else if (strcmpi(fileExt, "wma") == 0)
		return "audio/x-ms-wma";
	else if (strcmpi(fileExt, "mp3") == 0)
		return "audio/mp3";
	else if (strcmpi(fileExt, "apk") == 0)
		return "application/vnd.android.package-archive";
	else
		return octet;
}

//处理客户端请求
//invoked by tinyweb when GET request comes in
//please invoke write_uv_data() once and only once on every request, to send respone to client and close the connection.
//if not handle this request (by invoking write_uv_data()), you can close connection using tw_close_client(client).
//pathinfo: "/" or "/book/view/1"
//query_stirng: the string after '?' in url, such as "id=0&value=123", maybe NULL or ""
static void tw_request(uv_stream_t* client, reqHeads heads) {
	char fullpath[260];//绝对路径（末尾不带斜杠）
	sprintf(fullpath, "%s%s", tw_conf.doc_dir, (heads.path[0] == '/' ? heads.path + 1 : heads.path));
	//去掉末尾的斜杠
	char *p = &fullpath[strlen(fullpath) - 1];
	while (*p == '/' || *p == '\\')
	{
		*p = 0;
		p --;
	}

	char file_dir = isExist(fullpath);
	//判断 文件或文件夹，或不存在
	switch (file_dir)
	{
	case 1://存在：文件
	{
		char* postfix = strrchr(heads.path, '.');//从后面开始找文件扩展名
		if (postfix)
		{
			postfix++;
			p = postfix + strlen(postfix) - 1;
			while (*p == '/' || *p == '\\')
			{
				*p = 0;
				p--;
			}
		}
		tw_http_send_file(client, postfix?tw_get_content_type(postfix):"application/octet-stream", fullpath, heads.path);
	}
	break;
	case 2://存在：文件夹
	{
		if (heads.path[strlen(heads.path) - 1] != '/')
		{
			tw_301_Moved(client, heads);
			break;
		}
		char tmp[260]; tmp[0] = 0;
		char *s = strdup(tw_conf.doc_index);
		p = strtok(s, ";");
		//是否有默认主页
		while(p)
		{
#ifdef _MSC_VER
			sprintf(tmp, "%s\\%s", fullpath, p);
#else //_GNUC_
			sprintf(tmp, "%s/%s", fullpath, p);
#endif // _MSC_VER

			if (isFile(tmp))
			{
				tw_http_send_file(client, "text/html", tmp, heads.path);
				break;
			}
			tmp[0] = 0;
			p = strtok(NULL, ";");
		}
		free(s);
		//没用默认主页
		if (!tmp[0])
		{
			char *p = "Welcome to TinyWeb.<br>Directory access forbidden.";
			if (tw_conf.dirlist) {
				p = listDir(fullpath, heads.path);
#ifdef _MSC_VER //Windows下需要转换编码
				unsigned int len = strlen(p);
				char* p2 = GB2U8(p, &len);
				free(p);
				p = p2;
				//linux 下，系统是和源代码文件编码都是是utf8的，就不需要转换
#endif // _MSC_VER
			}
			char *respone = tw_format_http_respone("200 OK", "text/html", p, -1, NULL);
			tw_send_data(client, respone, -1, 0, 1);
			if (tw_conf.dirlist && p)
				free(p);
		}
	}
		break;
	default://不存在
		//http请求回调: 404前回调(未找到页面/文件时回调,此功能便于程序返回自定义功能)；返回0表示没有适合的处理请求，需要发送404错误
		if ( !(tw_conf.on_request && tw_conf.on_request(client, heads)) )
			tw_404_not_found(client, heads.path);
		break;
	}
}

//获取http头信息,返回指向 Sec-WebSocket-Key 的指针
static char* tw_get_http_heads(const uv_buf_t* buf, reqHeads* heads) {
	char *key,*end,*p;
	char* data = strstr(buf->base, "\r\n\r\n");
	if (data) {
		*data = 0;
		heads->data = data + 4;
		//是否有 Sec-WebSocket-Key
		key = strstr(buf->base, "Sec-WebSocket-Key:");
		if (key) {
			key += 19;
			while (isspace(*key)) key++;
			end = strchr(key, '\r');
			if (end) *end = 0;
			return key;
		}
		//not http upgrade to WebSocket
		if (buf->base[0] == 'G' && buf->base[1] == 'E' && buf->base[2] == 'T' && buf->base[3] == ' ') {
			heads->method = 1;//GET
		}
		else if (buf->base[0] == 'P' && buf->base[1] == 'O' && buf->base[2] == 'S' && buf->base[3] == 'T' && buf->base[4] == ' ') {
			heads->method = 2;//POST
		}
		//是http get/post协议
		if (heads->method)
		{
			//search path
			heads->path = strchr(buf->base+3, ' ')+1;
			while (isspace(*heads->path)) heads->path++;
			end = strchr(heads->path, ' ');
			if (end) *end = 0;
			//
			url_decode(heads->path);
#ifdef _MSC_VER //Windows下需要转换编码,因为windows系统的编码是GB2312
			size_t len = strlen(heads->path);
			char *gb = U82GB(heads->path, &len);
			strncpy(heads->path, gb, len);
			heads->path[len] = 0;
			free(gb);
			//linux 下，系统和源代码文件编码都是是utf8的，就不需要转换
#endif // _MSC_VER
			//query param
			if (1 == heads->method) { //GET
				p = strchr(heads->path, '?');
				if (p) {
					heads->query = p + 1;
					*p = 0;
				}
			}
			else {  //POST
				heads->query = heads->data;
			}
			//search host
			heads->host = strstr(end + 4, "Host:");
			if (heads->host) {
				heads->host += 5;
				while (isspace(*heads->host)) heads->host++;
				end = strchr(heads->host, '\r');
				if (end) *end = 0;
			}
			//data length
			heads->len = buf->len - (data-buf->base) -4;
			if (heads->len < 1)
				heads->data = NULL, heads->len = 0;
		}
	}
	return NULL;
}

//(循环)读取客户端发送的数据,接收客户的数据
static void on_uv_read(uv_stream_t* client, ssize_t nread, const uv_buf_t* buf) {
	unsigned long Len = buf->len, len;
	char *gb;
	if (nread > 0) {
		membuf_t* cliInfo = (membuf_t*)client->data; //see tw_on_connection()
		assert(cliInfo);
		//得到实际数据长度
		while (buf->base[Len-1] == 0) --Len;
		//WebSocket
		if (cliInfo->flag & 0x2) { //WebSocket
			WebSocketHandle* hd;
			if (cliInfo->data)
				hd = (WebSocketHandle*)cliInfo->data;
			else {
				hd = (WebSocketHandle*)calloc(1, sizeof(WebSocketHandle));
			}
			if(NULL==hd->buf.data)
				membuf_init(&hd->buf, 128);
			hd->buf.flag = cliInfo->flag;
			//
			uint64_t leftlen=WebSocketGetData(hd, buf->base, Len);
			if (hd->isEof)
			{
				char wsclose[2] = { (char)0x88,0 };
				switch (hd->type) {
				case 0: //0x0表示附加数据帧
					break;
				case 1: //0x1表示文本数据帧
#ifdef _MSC_VER //Windows下需要转换编码,因为windows系统的编码是GB2312
					len = hd->buf.size;
					gb = U82GB(hd->buf.data, &len);
					free(hd->buf.data);
					hd->buf.data = gb;
					hd->buf.buffer_size=hd->buf.size = len;
					//linux 下，系统和源代码文件编码都是是utf8的，就不需要转换
#endif // _MSC_VER
					//接收数据回调
					if (tw_conf.on_data)
						tw_conf.on_data(client, &hd->buf);
					break;
				case 2: //0x2表示二进制数据帧
					//接收数据回调
					if (tw_conf.on_data)
						tw_conf.on_data(client, &hd->buf);
					break;
				case 3: //0x3 - 7暂时无定义，为以后的非控制帧保留
				case 4:
				case 5:
				case 6:
				case 7: 
					if (hd->buf.data)
						free(hd->buf.data);
					memset(hd, 0, sizeof(WebSocketHandle));
					break;
				case 8: //0x8表示连接关闭
					tw_send_data(client, wsclose, 2, 1, 0);
					if (hd->buf.data)
						free(hd->buf.data);
					free(hd);
					cliInfo->data = NULL;
					break;
				case 9: //0x9表示ping
				case 10://0xA表示pong
				default://0xB - F暂时无定义，为以后的控制帧保留
					*buf->base += 1;//发送pong
					tw_send_data(client, buf->base, 2, 1, 0);
					if (hd->buf.data)
						free(hd->buf.data);
					memset(hd, 0, sizeof(WebSocketHandle));
					break;
				}
				//释放 membuf , 收到消息时再分配
				if (cliInfo->data) {
					hd->dfExt = hd->isEof = hd->type = 0;
					membuf_uninit(&hd->buf);
				}
			}
			else
				cliInfo->data = (unsigned char*)hd;
		}
		//long-link
		else if(cliInfo->flag & 0x1){ //long-link
			//接收数据回调
			if (tw_conf.on_data){
				membuf_clear(cliInfo, 0);
				membuf_append_data(cliInfo, buf->base, Len);
				tw_conf.on_data(client, cliInfo);
			}
		}
		//http 或 未知
		else { //http 或 未知
			char* p,*p2;
			reqHeads heads;
			memset(&heads, 0, sizeof(reqHeads));
			p=tw_get_http_heads(buf, &heads);
			if (p) { //WebSocket 握手
				cliInfo->flag |= 3;//long-link & WebSocket
				p2=WebSocketHandShak(p);
				tw_send_data(client, p2, -1, 1, 0);
				free(p2);
			}
			else if (heads.method) { //HTTP
				//所有请求全部回调
				if (tw_conf.all_http_callback && tw_conf.on_request && !tw_conf.on_request(client, heads)){
						tw_404_not_found(client, heads.path);
				}
				else
					tw_request(client, heads);
			}
			else { //SOCKET
				cliInfo->flag |= 1;//long-link
				//接收数据回调
				if (tw_conf.on_data) {
					membuf_clear(cliInfo, 0);
					membuf_append_data(cliInfo, buf->base, Len);
					tw_conf.on_data(client, cliInfo);
				}
			}
		}
	}
	else if (nread <= 0) {//在任何情况下出错, read 回调函数 nread 参数都<0，如：出错原因可能是 EOF(遇到文件尾)
		membuf_t mbuf;
		char p2[20] = { 0 };
		sprintf(p2,"%d:\0",nread);
		membuf_init(&mbuf, 128);
		membuf_append_data(&mbuf,p2,strlen(p2));

		const char* p= uv_err_name((int)nread);
		membuf_append_data(&mbuf, p,strlen(p));

		p= uv_strerror((int)nread);
		membuf_append_data(&mbuf, ",",1);
		membuf_append_data(&mbuf, p, strlen(p));

		//FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
		//	FORMAT_MESSAGE_IGNORE_INSERTS, NULL, nread,
		//	MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&p, 0, NULL);
		//if (p) {
		//	membuf_append_data(&mbuf, ",",1);
		//	membuf_append_data(&mbuf, p, strlen(p));
		//}
		//出错信息回调
		if (tw_conf.on_error)
			tw_conf.on_error(client, nread, &mbuf);
		else
			fprintf(stderr, "%s\n", mbuf.data);
		membuf_uninit(&mbuf);
		//关闭连接
		tw_close_client(client);
	}
	//每次使用完要释放
	if (buf->base)
		free(buf->base);
}

//为每次读取数据分配内存缓存
static void on_uv_alloc(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
	buf->base = (char*)calloc(1,suggested_size);
	buf->len = suggested_size;
}

//客户端接入
static void tw_on_connection(uv_stream_t* server, int status) {
	//assert(server == (uv_stream_t*)&_server);
	if (status == 0) {
		//建立客户端信息,在关闭连接时释放 see after_uv_close_client
		uv_tcp_t* client = (uv_tcp_t*)malloc(sizeof(uv_tcp_t));
		//创建客户端的数据缓存块,在关闭连接时释放 see after_uv_close_client
		client->data = malloc(sizeof(membuf_t));
		membuf_init((membuf_t*)client->data, 128);//初始化缓存为128字节
		uv_tcp_init(server->loop, client);//将客户端放入loop
		//接受客户，保存客户端信息
		uv_accept(server, (uv_stream_t*)client);
		client->close_cb = after_uv_close_client;
		//开始读取客户端数据
		uv_read_start((uv_stream_t*)client, on_uv_alloc, on_uv_read);
		//客户端接入回调
		if (tw_conf.on_connect)
			tw_conf.on_connect((uv_stream_t*)client);
	}
}


//==================================================================================================

//start web server, linstening ip:port
//ip is only ipV4, can be NULL or "" or "*", which means "0.0.0.0"
//doc_root_path can be NULL, or requires not end with /
void tinyweb_start(uv_loop_t* loop, tw_config* conf) {
	char p;
	assert(conf != NULL);
	if ( conf->ip == NULL || (conf->ip != NULL && conf->ip[0] == '*'))
		conf->ip = "0.0.0.0";
	struct sockaddr_in addr;
	uv_ip4_addr( conf->ip, conf->port, &addr );

	memset(&tw_conf, 0, sizeof(tw_config));
	tw_conf = *conf;
	//设置主目录（末尾带斜杠）
	if (conf->doc_dir)
	{
		p = conf->doc_dir[strlen(conf->doc_dir) - 1];
		if ( p == '\\' || p=='/')
			tw_conf.doc_dir = strdup(conf->doc_dir);
		else
		{
			char p[256];
#ifdef _MSC_VER
			sprintf(p, "%s\\", conf->doc_dir);
#else
			sprintf(p, "%s/", conf->doc_dir);
#endif // _MSC_VER

			tw_conf.doc_dir = strdup(p);
		}
	}
	else
	{
		char path[260];
#ifdef _MSC_VER
		sprintf(path, "%s\\", getProcPath());
#else
		sprintf(path, "%s/", getProcPath());
#endif // _MSC_VER
		tw_conf.doc_dir = strdup(path);
	}
	printf("WebRoot Dir:%s\n", tw_conf.doc_dir);
	//设置默认主页（分号间隔）
	if (conf->doc_index)
		tw_conf.doc_index = strdup(conf->doc_index);
	else
		tw_conf.doc_index = strdup("index.htm;index.html");
	uv_tcp_init(loop, &_server);
	uv_tcp_bind(&_server, (const struct sockaddr*) &addr, 0);
	uv_listen((uv_stream_t*)&_server, 8, tw_on_connection);

	printf("TinyWeb v1.0.0 is started, listening port %s:%d...\n", tw_conf.ip , tw_conf.port);
	printf("Please access http://%s:%d or http://localhost:%d from you web browser.\n", tw_conf.ip , tw_conf.port, tw_conf.port);
}

//uv_stop以后不能马上执行uv_loop_close()
//貌似关闭及释放loop等资源不是很完善的样子
static void on_uv_walk(uv_handle_t* handl, void* arg)
{
	uv_loop_close((uv_loop_t*)arg);
	free(arg);
	free(tw_conf.doc_dir);
	free(tw_conf.doc_index);
}

//stop TinyWeb
//当执行uv_stop之后，uv_run并不能马上退出，而是要等待其内部循环的下一个iteration到来时才会退出；
//如果提前free掉loop就会导致loop失效。当然也可以sleep几十毫秒然后再close，但这么搞不太雅观。
void tinyweb_stop(uv_loop_t** loop)
{
	uv_walk(*loop, on_uv_walk,*loop);
	uv_stop(*loop);
	*loop = NULL;
}