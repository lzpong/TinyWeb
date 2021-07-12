#include "tinyweb.h"
#include "tools.h"

#ifdef __GNUC__
#include <uv.h>
#define _strncmpi strncasecmp
#define strcmpi strcasecmp
#endif // __GNUC__

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <memory.h>

//TinyWeb 增加与完善功能，by lzpong 2016/11/24

//值大,发送文件时磁盘和CPU性能更好,占用内存增加
#define TW_SEND_SIZE 1024*1024*16

typedef struct tw_file_t {
	//uchar flag;   //连接的标志
	FILE* fp;    //文件指针
	uchar* buff;//文件发送缓存
	unsigned long long fsize;//文件大小
	unsigned long long lsize;//文件中要发送的块剩余大小
}tw_file_t;

typedef struct tw_client {
	tw_peerAddr pa;//客户端连接的地址
	tw_file_t ft;//发往客户端的文件,断点续传记录
	WebSocketHandle hd;
	tw_reqHeads heads;//Http 头部(如果是http)
	membuf_t buf;//http post 分包接收的缓存
}tw_client;
//=================================================


//关闭客户端连接后，释放客户端连接的数据
static void after_uv_close_client(uv_handle_t* client) {
	tw_config* tw_conf = (tw_config*)(client->loop->data);
	tw_client* clidata = (tw_client*)client->data;
	//如果有发送文件
	if (clidata->ft.fp)
		fclose(clidata->ft.fp);
	if (clidata->ft.buff)
		free(clidata->ft.buff);
	//如果是WebSocket
	//if (clidata->pa.flag & 0x2)
	membuf_uninit(&clidata->hd.buf);
	//http post 分包接收的缓存
	membuf_uninit(&clidata->buf);
	//关闭连接回调
	if (tw_conf->on_close)
		tw_conf->on_close(tw_conf->udata, (uv_stream_t*)client, &clidata->pa);
	free(client->data);
	free(client);
}

//关闭客户端连接
void tw_close_client(uv_stream_t* client) {
	//tw_client* clidata = (tw_client*)client->data;
	uv_close((uv_handle_t*)client, after_uv_close_client);
}

//发送数据后,free数据，关闭客户端连接
static void after_uv_write(uv_write_t* w, int err) {
	tw_client* clidata = (tw_client*)w->handle->data;
	//长连接就不关闭了
	if (!err && clidata->ft.fp)
		tw_http_send_file(w->handle, NULL, NULL, NULL, NULL);
	else if (!(clidata->pa.flag & 0x1) || clidata->pa.flag & 0x80) {
		if (w->handle->flags & 0x01 && !(clidata->pa.flag & 0x80)) {
			tw_config* tw_conf = (tw_config*)(w->handle->loop->data);
			if (tw_conf->on_error)
				tw_conf->on_error(tw_conf->udata, w->handle, &clidata->pa, err, "handle Has been closed");
			else
				printf("after_uv_write sk:%zd　error: handle Has been closed\n", clidata->pa.sk);
		}
		else
			uv_close((uv_handle_t*)w->handle, after_uv_close_client);
	}
	if (w->data)
		free(w->data); //sended data need free
	free(w);
}

//发送数据到客户端; 如果是短连接,则发送完后会关闭连接
//data：待发送数据
//len： 数据长度, -1 将自动计算数据长度
//need_copy_data：是否需要复制数据; 如果是 WebSocket 就是 opCode
//need_free_data：是否需要free数据; 如果need_copy_data非零则忽略此参数
void tw_send_data(uv_stream_t* client, const void* data, size_t len, char need_copy_data, char need_free_data) {
	tw_client* clidata = (tw_client*)client->data;
	uv_buf_t buf;
	uv_write_t* w;
	void* newdata = (void*)data;

	if (len == (size_t)-1)
		len = strlen((char*)data);

	if (clidata->pa.flag & 0x2) {
		newdata = WebSocketMakeFrame(data, &len, need_copy_data);
		if (need_free_data)
			free(data);
		need_free_data = 1;
	}
	else if (need_copy_data) {
		newdata = malloc(len);
		memcpy(newdata, data, len);
	}

	buf = uv_buf_init((char*)newdata, len);
	w = (uv_write_t*)malloc(sizeof(uv_write_t));
	w->data = (need_copy_data || need_free_data) ? newdata : NULL;
	uv_write(w, client, &buf, 1, after_uv_write); //free w and w->data in after_uv_write()
}

//制造头部 SetCookie 字段和值
//cookie: 缓存区(至少 42+strlen(key)+strlen(val)+strlen(domain)+strlen(path) )
//ckLen: cookie的长度
//expires: 多少秒后过期
//domain: Domain, 域名或IP地址,或NULL
//path: Path, 可以是 heads->path,或NULL
void tw_make_setcookie(char* cookie, int ckLen, const char* key, const char* val, int expires, const char* domain, const char* path) {
	int len = 0;
	snprintf(cookie, ckLen, "Set-Cookie: %s=%s", key, val);
	len = strlen(cookie);
	if (expires > 0 && len < ckLen)
		snprintf(cookie + len, ckLen - len, "; Max-Age=%d", expires);
	len = strlen(cookie);
	if (domain && len < ckLen)
		snprintf(cookie + len, ckLen - len, "; Domain=%s", domain);
	len = strlen(cookie);
	if (path && len < ckLen)
		snprintf(cookie + len, ckLen - len, "; Path=%s", path);
	len = strlen(cookie);
	if (len + 3 <= ckLen) {
		cookie[len] = '\r';
		cookie[len + 1] = '\n';
		cookie[len + 2] = 0;
	}
}
//制造头部 delete cookie
void tw_make_delcookie(char* del_cookie, int ckLen, char* key) {
	snprintf(del_cookie, ckLen, "Delete-Cookie: %s\r\n", key);
}

//发送'200 OK' 响应; 不会释放(free)传入的数据(u8data)
//content_type：Content Type 文档类型
//u8data：utf-8编码的数据
//content_length：数据长度，为0或-1时自动计算(strlen)(c_str, end with NULL)
//respone_size：获取响应最终发送的数据长度，为0表示放不需要取此长度
void tw_send_200_OK(uv_stream_t* client, const char* ext_heads, const char* content_type, const void* u8data, size_t content_length, size_t* respone_size) {
	size_t repSize;
	const char *type = strchr(content_type, '/');
	//有'.'    没有'/'   至少有两个'/'    '/'是在开头    '/'是在末尾
	//都要重新取文件类型
	if (type) {
		if (strchr(content_type, '.'))// 有'.'
			type = tw_get_content_type(content_type);
		else {
			type = strchr(type + 1, '/');
			if (type)//至少有两个'/'
				type = tw_get_content_type(content_type);
			else {
				type = strchr(content_type, '/');
				if (type == content_type || type == (content_type + strlen(content_type) - 1))
					type = tw_get_content_type(content_type);
				else
					type = content_type;
			}
		}
	}//没有'/'
	else
		type = tw_get_content_type(content_type);
	char *data = tw_format_http_respone(client, "200 OK", ext_heads, type, u8data, content_length, &repSize);
	tw_send_data(client, data, repSize, 0, 1);//发送后free data
	if (respone_size)
		*respone_size = repSize;
}

//返回格式华的HTTP响应内容（需要free返回数据）
//status: "200 OK"
//content_type: 文件类型，如："text/html" ；可以调用tw_get_content_type()得到
//content: any utf-8 data, need html-encode if content_type is "text/html"
//content_length: 0或-1自动计算 content 长度(c_str, end with NULL)
//respone_size: if not NULL,可以获取发送的数据长度 the size of respone will be writen to request
//returns malloc()ed c_str, need free() by caller
char* tw_format_http_respone(uv_stream_t* client, const char* status, const char* ext_heads, const char* content_type, const char* content, size_t content_length, size_t* respone_size) {
	size_t totalsize, header_size;
	char* respone;
	char szDate[30];
	getGmtTime(szDate, 30, 0);
	ext_heads == NULL ? ext_heads = "" : 0;
	tw_config* tw_conf = (tw_config*)(client->loop->data);
	if (content_length == 0 || content_length == (size_t)-1)
		content_length = content ? strlen(content) : 0;
	totalsize = strlen(status) + strlen(ext_heads) + strlen(content_type) + content_length + 158;
	respone = (char*)malloc(totalsize + 1);
	header_size = snprintf(respone, totalsize, "HTTP/1.1 %s\r\nDate: %s\r\nServer: TinyWeb\r\nConnection: close\r\nContent-Type:%s; charset=%s\r\nContent-Length:%zd\r\n%s\r\n"
						   , status, szDate, content_type, tw_conf->charset, content_length, ext_heads);
	assert(header_size > 0);
	if (content_length)
		memcpy(respone + header_size, content, content_length + 1);

	if (respone_size)
		*respone_size = header_size + content_length;
	return respone;
}

//发送404响应
static void tw_404_not_found(uv_stream_t* client, const char* pathinfo, const char* ext_heads) {
	char* respone;
	char buffer[128];
	snprintf(buffer, sizeof(buffer), "<h1>404 Not Found</h1><p>%s</p>", pathinfo);
	respone = tw_format_http_respone(client, "404 Not Found", ext_heads, "text/html", buffer, -1, NULL);
	tw_send_data(client, respone, -1, 0, 1);
}

//发送301响应,路径永久重定位
void tw_301_Moved(uv_stream_t* client, tw_reqHeads* heads, const char* ext_heads) {
	size_t len = 76 + strlen(heads->path);
	char buffer[2245];
	char szDate[30];
	ext_heads == NULL ? ext_heads = "" : 0;
	getGmtTime(szDate, 30, 0);
	tw_config* tw_conf = (tw_config*)(client->loop->data);
	snprintf(buffer, sizeof(buffer), "HTTP/1.1 301 Moved Permanently\r\nDate: %s\r\n"
			 "Server: TinyWeb\r\nLocation: http://%s%s%s%s\r\nConnection: close\r\n"
			 "Content-Type:text/html;charset=%s\r\nContent-Length:%zd\r\n%s\r\n"
			 "<h1>Moved Permanently</h1><p>The document has moved <a href=\"%s%s%s\">here</a>.</p>"
			 , szDate
			 , heads->host, heads->path, (heads->query[0] ? "?" : ""), (heads->query[0] ? heads->query : "")
			 , tw_conf->charset, len, ext_heads
			 , heads->path, (heads->query[0] ? "?" : ""), (heads->query[0] ? heads->query : ""));
	tw_send_data(client, buffer, -1, 1, 1);
}
//发送302响应,路径临时重定位
void tw_302_Moved(uv_stream_t* client, tw_reqHeads* heads, const char* ext_heads) {
	char buffer[2245];
	char szDate[30];
	ext_heads == NULL ? ext_heads = "" : 0;
	getGmtTime(szDate, 30, 0);
	tw_config* tw_conf = (tw_config*)(client->loop->data);
	snprintf(buffer, sizeof(buffer), "HTTP/1.1 302 Moved Temporarily\r\nDate: %s\r\n"
			 "Server: TinyWeb\r\nLocation: http://%s%s%s%s\r\nConnection: close\r\n"
			 "Content-Type:text/html;charset=%s\r\nContent-Length:0\r\n%s\r\n"
			 , szDate
			 , heads->host, heads->path, (heads->query[0] ? "?" : ""), (heads->query[0] ? heads->query : "")
			 , tw_conf->charset, ext_heads);
	tw_send_data(client, buffer, -1, 1, 1);
}

//http协议发送文件,异步
//file_path: 文件路径
void tw_http_send_file(uv_stream_t* client, tw_reqHeads* heads, const char* ext_heads, const char* content_type, const char* file_path) {
	char *respone;
	char szDate[30];
	tw_config* tw_conf = (tw_config*)(client->loop->data);
	tw_client* clidata = (tw_client*)client->data;
	tw_file_t* filet = &clidata->ft;

	//发送头部
	if (!filet->fp && file_path) {
		filet->fp = fopen(file_path, "rb");
		if (filet->fp) {
#ifdef _WIN64
			_fseeki64(filet->fp, 0, SEEK_END);
			filet->fsize = _ftelli64(filet->fp);
#else
			fseek(filet->fp, 0, SEEK_END);
			filet->fsize = ftell(filet->fp);
#endif
			if (heads->Range_frm < 0)//(负数:从文件末尾反过来的位置,即fileSize-sizeFrom)
				heads->Range_frm = filet->fsize + heads->Range_frm;
			if (heads->Range_to <= 0)//(负数:从文件末尾反过来的位置,即fileSize-sizeTo)
				heads->Range_to = filet->fsize + heads->Range_to;
			if (filet->fsize < (unsigned long long)heads->Range_frm)//开始位置大于文件
				heads->Range_frm = filet->fsize;
			if (heads->Range_to < heads->Range_frm || (unsigned long long)heads->Range_to>filet->fsize)//Range_to 可能没有,或不正确,表示整个文件大小
				heads->Range_to = filet->fsize;
			//要下载区段的size
			filet->lsize = heads->Range_to - heads->Range_frm;
#ifdef _WIN64
			_fseeki64(filet->fp, heads->Range_frm, SEEK_SET);
#else
			fseek(filet->fp, heads->Range_frm, SEEK_SET);
#endif
			ext_heads == NULL ? ext_heads = "" : 0;
			getGmtTime(szDate, 30, 0);
			respone = (char*)malloc(300 + 1);
			int respone_size;
			if (heads->Range_frm == 0) //200 OK
				respone_size = snprintf(respone, 300, "HTTP/1.1 200 OK\r\nDate: %s\r\nServer: TinyWeb\r\nConnection: close\r\nContent-Type:%s;charset=%s\r\nAccept-Range: bytes\r\nContent-Length:%llu\r\n%s\r\n"
										, szDate, content_type, tw_conf->charset, filet->fsize, ext_heads);
			else //206 Partial Content
				respone_size = snprintf(respone, 300, "HTTP/1.1 206 Partial Content\r\nDate: %s\r\nServer: TinyWeb\r\nConnection: close\r\nContent-Type:%s;charset=%s\r\nAccept-Range: bytes\r\nContent-Range: %lld-%lld/%llu\r\nContent-Length:%llu\r\n%s\r\n"
										, szDate, content_type, tw_conf->charset, heads->Range_frm, heads->Range_to, filet->fsize, filet->lsize, ext_heads);
			tw_send_data(client, respone, respone_size, 0, 1);
		}
		else {
			printf("the file not found to send: %s\n", heads->path);
			tw_404_not_found(client, heads->path, ext_heads);
		}
	}
	else { //发送文件
		size_t read_size = 0;// read_bytes;
		if (filet->fp) {
			if (filet->lsize > 0) {
				if (!filet->buff)
					filet->buff = (uchar*)malloc(TW_SEND_SIZE + 1);
				//fread 返回实际读取的单元个数。如果小于count，则可能文件结束或读取出错；
				//可以用ferror()检测是否读取出错，用feof()函数检测是否到达文件结尾。如果size或count为0，则返回0。
				if (filet->lsize > TW_SEND_SIZE)
					read_size = fread(filet->buff, sizeof(char), TW_SEND_SIZE, filet->fp);
				else
					read_size = fread(filet->buff, sizeof(char), filet->lsize, filet->fp);
				filet->lsize -= read_size;
				tw_send_data(client, filet->buff, read_size, 0, 0);
			}
			else {
				fclose(filet->fp);
				filet->fp = 0;
				filet->fsize = 0;
			}
		}
	}
}

//根据扩展名(不区分大小写)，返回文件类型 content_type
const char* tw_get_content_type(const char* fileExt) {
	const static char* octet = "application/octet-stream";
	if (fileExt) {
		//不管什么路径名或者文件名, 只要最后面有点(.),就认为是有扩展名的
		const char *p = strrchr(fileExt, '.');
		if (p) { // /aaa.txt
			fileExt = p + 1;
		}
	}
	else //否则没有扩展名
		return octet;
	if (strcmpi(fileExt, "htm") == 0 || strcmpi(fileExt, "html") == 0)
		return "text/html";
	else if (strcmpi(fileExt, "js") == 0)
		return "application/javascript";
	else if (strcmpi(fileExt, "css") == 0)
		return "text/css";
	else if (strcmpi(fileExt, "json") == 0)
		return "application/json";
	else if (strcmpi(fileExt, "log") == 0 || strcmpi(fileExt, "txt") == 0 || strcmpi(fileExt, "ini") == 0
			 || strcmpi(fileExt, "config") == 0 || strcmpi(fileExt, "conf") == 0 || strcmpi(fileExt, "cfg") == 0
			 || strcmpi(fileExt, "sh") == 0 || strcmpi(fileExt, "bat") == 0)
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
	else if (strcmpi(fileExt, "xhtml") == 0)
		return "application/xhtml+xml";
	else if (strcmpi(fileExt, "swf") == 0)
		return "application/x-shockwave-flash";
	else if (strcmpi(fileExt, "svg") == 0)
		return "image/svg-xml";
	else if (strcmpi(fileExt, "wav") == 0)
		return "audio/wav";
	else if (strcmpi(fileExt, "mid") == 0 || strcmpi(fileExt, "midi") == 0)
		return "audio/midi";
	else if (strcmpi(fileExt, "wma") == 0)
		return "audio/x-ms-wma";
	else if (strcmpi(fileExt, "mp3") == 0)
		return "audio/mp3";
	else if (strcmpi(fileExt, "3gp") == 0)
		return "video/3gpp";
	else if (strcmpi(fileExt, "avi") == 0)
		return "video/x-msvideo";
	else if (strcmpi(fileExt, "mkv") == 0)
		return "video/x-matroska";
	else if (strcmpi(fileExt, "mp4") == 0)
		return "video/mp4";
	else if (strcmpi(fileExt, "rmvb") == 0)
		return "video/vnd.rn-realvideo";
	else if (strcmpi(fileExt, "flv") == 0)
		return "flv-application/octet-stream";// "video/x-flv";
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
static void tw_request(uv_stream_t* client, tw_reqHeads* heads) {
	tw_config* tw_conf = (tw_config*)(client->loop->data);
	char fullpath[260];//绝对路径（末尾不带斜杠）
	snprintf(fullpath, 259, "%s/%s", tw_conf->doc_dir, (heads->path[0] == '/' ? heads->path + 1 : heads->path));
	//去掉末尾的斜杠
	char *p = &fullpath[strlen(fullpath) - 1];
	while (*p == '/' || *p == '\\')
		*p = 0, p--;

	char file_dir = isExist(fullpath);
	//判断 文件或文件夹，或不存在
	switch (file_dir) {
		case 1://存在：文件
		{
			char* postfix = strrchr(heads->path, '.');//从后面开始找文件扩展名
			if (postfix) {
				postfix++;
				p = postfix + strlen(postfix) - 1;
				while (*p == '/' || *p == '\\')
					*p = 0, p--;
			}
			tw_http_send_file(client, heads, NULL, tw_get_content_type(postfix), fullpath);
		}
		break;
		case 2://存在：文件夹
		{
			if (heads->path[strlen(heads->path) - 1] != '/') //文件夹要检测末尾'/'
			{
				int len = strlen(heads->path);
				if (len >= sizeof(heads->path) - 1)
					len = sizeof(heads->path) - 2;
				heads->path[len] = '/';
				heads->path[len + 1] = 0;
				tw_301_Moved(client, heads, NULL);
				break;
			}
			char tmp[360]; tmp[0] = 0;
			char *s = strdup(tw_conf->doc_index);
			p = strtok(s, ";");
			//是否有默认主页
			while (p) {
				snprintf(tmp, 359, "%s/%s", fullpath, p);
				if (isFile(tmp)) {
					tw_http_send_file(client, heads, NULL, "text/html", tmp);
					break;
				}
				tmp[0] = 0;
				p = strtok(NULL, ";");
			}
			free(s);
			//没用默认主页
			if (!tmp[0]) {
				char* p2 = NULL;
				uint len;
				membuf_t buf;
				membuf_init(&buf, 1024 * 2);
				char *body = "Welcome to TinyWeb.<br>Directory access forbidden.";
				if (tw_conf->dirlist) {
					membuf_append(&buf, "<!DOCTYPE html><html><head><title>Index of ");//+path
#ifdef _MSC_VER
					if (strnicmp(tw_conf->charset, "utf", 3) == 0) {//utf-8
						len = strlen(heads->path);
						p2 = GB2U8(heads->path, &len);
						membuf_append(&buf, p2);
					}
					else
#endif // _MSC_VER
						membuf_append(&buf, heads->path);
					membuf_append_format(&buf, "</title><meta name=\"renderer\" content=\"webkit\"><meta charset=\"%\">\r\n", tw_conf->charset);
					membuf_append(&buf, "</head><body><h1>Index of ");//+path
					if (p2) {
						membuf_append(&buf, p2);
						free(p2);
					}
					else {
						membuf_append(&buf, heads->path);
					}
					membuf_append(&buf, "</h1>\r\n"
								  "<table>\r\n"
								  "<thead><tr><th><a href=\"javascript:fssort('type')\">@</a></th><th><a href=\"javascript:fssort('name')\">Name</a></th><th><a href=\"javascript:fssort('size')\">Size</a></th><th><a href=\"javascript:fssort('mtime')\">Last modified</a></th></tr>"
								  "<tr><th colspan=\"4\"><hr style=\"margin:1px;\"></th></tr></thead>\r\n"
								  "<tbody id=\"tbody\"></tbody>"
								  "<tfoot><tr><th colspan=\"4\"><hr></th></tr></tfoot>"
								  "</table>"
								  "<address>TinyWeb Server</address>"
								  "</body></html>\r\n<script type=\"text/javascript\">\r\nvar files=");//+files
					body = listDir(fullpath, heads->path);
#ifdef _MSC_VER
					if (strnicmp(tw_conf->charset, "utf", 3) == 0) {//utf-8
						len = strlen(body);
						p2 = GB2U8(body, &len);
						free(body);
						body = p2;
					}
#endif // _MSC_VER
					membuf_append(&buf, body);
					free(body);
					membuf_append(&buf, "; \r\nvar html = \"\", p=files.path[files.path.length-1];\n"
								  "function fsshow(){var html='';for (var r in files.files){r=files.files[r];html+='<tr><td>'+r.type+\"</td><td><a href='\"+r.name+\"'>\"+r.name+'</td><td>'+r.size+'</td><td>'+r.mtime+'</td></tr>';}document.querySelector('tbody').innerHTML = html;}\n"
								  "if(p!='/'){files.path+='/';}\n"
								  "files.files.sort(function(a,b){var n=a.type.localeCompare(b.type);if(n)return n;else return a.name.localeCompare(b.name);});\n"
								  "fsshow();\n"
								  "function fssort(n){files.files.sort(function(a,b){if(typeof a[n]=='number')return a[n]-b[n];return a[n].localeCompare(b[n])});fsshow();}\n"
								  "</script>");
				}
				else
					membuf_append_data(&buf, body, strlen(body));
				char *respone = tw_format_http_respone(client, "200 OK", NULL, "text/html", (char*)buf.data, buf.size, NULL);
				tw_send_data(client, respone, -1, 0, 1);
				membuf_uninit(&buf);
			}
		}
		break;
		default://不存在
			tw_404_not_found(client, heads->path, NULL);
			break;
	}
}

//获取http头信息,返回指向 Sec-WebSocket-Key 的指针
static char* tw_get_http_heads(const uv_buf_t* buf, int len, tw_reqHeads* heads) {
	char *key = NULL, *start, *head, *p;
	char delims[] = "\r\n";
	char* data = strstr(buf->base, "\r\n\r\n");
	if (data) {
		*data = 0;
		heads->data = data += 4;
		heads->len = len - (data - buf->base);
		//是http get/post协议
		if (buf->base[0] == 'G' && buf->base[1] == 'E' && buf->base[2] == 'T' && buf->base[3] == ' ') {
			heads->method = 1;//GET
		}
		else if (buf->base[0] == 'P' && buf->base[1] == 'O' && buf->base[2] == 'S' && buf->base[3] == 'T' && buf->base[4] == ' ') {
			heads->method = 2;//POST
		}
		//是http get/post协议
		if (heads->method) {
			char *path = "", *query = "";
			head = strtok(buf->base, delims);
			//search path
			path = strchr(head + 3, ' ') + 1;
			while (isspace(*path)) path++;
			start = strchr(path, ' ');
			if (start) *start = 0;
			//url含有转义编码字符
			if (strstr(path, "%") != 0) {
				url_decode(heads->path);
#ifdef _MSC_VER //Windows下需要转换编码,因为windows系统的编码是GB2312
				size_t len = strlen(path);
				char *gb = U82GB(path, &(unsigned int)len);
				strncpy(path, gb, len);
				path[len] = 0;
				free(gb);
				//linux 下，系统和源代码文件编码都是是utf8的，就不需要转换
#endif // _MSC_VER
			}
			//query param
			p = strchr(path, '?');
			if (p) {
				query = p + 1;
				*p = 0;
			}
			//确保开头为'/'
			if (*path != '/') {
				path--;
				*path = '/';
				*(path - 1) = 0;
			}
			//确保结尾不是"/.."
			p = strrchr(path, '.');
			if (p && *(p + 1) == 0 && *(p - 1) == '.' && *(p - 2) == '/') {
				*(p + 1) = '/';
				*(p + 2) = 0;
			}
			////------------尽可能的合并 "../"  "/./"
			if (strstr(path, "./") != 0) {
				//去掉"/./"
				while ((p = strstr(path, "/./")))
					memmove(p, p + 2, strlen(p + 2) + 1);
				//尽可能的合并"../"
				while ((p = strstr(path, "/.."))) {//存在 ..
					if ((p - path) <= 1) {
						if ((start = strchr(path + 2, '/')))
							path = start;
						else
							*p = 0;
						continue;
					}
					*(p - 1) = 0;
					start = strrchr(path, '/');
					if (start == NULL)
						start = path;
					key = strchr(p + 2, '/');
					if (key)
						p = key;
					else
						break;
					memmove(start, p, strlen(p) + 1);
				}
			}
			snprintf(heads->path, 512, "%s", path);
			snprintf(heads->query, 1500, "%s", query);

			key = NULL;
			//从第二行开始循环处理 头部
			head = strtok(NULL, delims);
			while (head) {
				//是否有 Sec-WebSocket-Key
				//http upgrade to WebSocket
				if ((start = strstr(head, "Sec-WebSocket-Key: "))) {
					key = start + 19;
				}
				//search host
				else if ((start = strstr(head, "Host: "))) {
					snprintf(heads->host, 260, "%s", start + 6);
				}
				//Range: bytes=sizeFrom-[sizeTo]  (sizeTo 可能没有,或不正确,表示整个文件大小)
				// (sizeFrom 为负数,表示从文件末尾反过来的位置,即fileSize-sizeFrom)
				//Range: bytes=sizeFrom-[sizeTo],sizeFrom-[sizeTo][,sizeFrom-[sizeTo]]  这种多段不支持,只支持一段
				else if ((start = strstr(head, "Range: "))) {
					start += 7;
					start = strstr(start, "bytes=");
					if (start)
						start += 6;
					p = strstr(start + 1, "-");//防止 sizeFrom 为负数
					heads->Range_to = 0;
					if (p)//可能有 sizeTo
					{
						heads->Range_frm = strtoll(start, &p, 10);
						p++;//跳过 '-'
						if (*p)
							heads->Range_to = strtoll(p, NULL, 10);
					}
					else //没有 sizeTo
						heads->Range_frm = strtol(start, NULL, 10);
				}
				//Content-Length: 3543
				else if ((start = strstr(head, "Content-Length: "))) {
					heads->contentLen = atoi(start + 16);
				}
				//Cookie: xxxxx
				else if ((start = strstr(head, "Cookie: "))) {
					snprintf(heads->cookie, 260, "%s", start + 8);
				}
				//下一行 头部
				head = strtok(NULL, delims);
			}
			if (heads->contentLen < 1)
				heads->data = NULL, heads->len = 0;
		}
	}
	return key;
}

//on_read_WebSocket
static void on_read_websocket(uv_stream_t* client, char* data, size_t Len) {
	tw_config* tw_conf = (tw_config*)(client->loop->data);
	tw_client* clidata = (tw_client*)client->data;
	WebSocketHandle* hd = &clidata->hd;
	if (NULL == hd->buf.data)
		membuf_init(&hd->buf, 128);

	WebSocketGetFrame(hd, data, Len);
	//不是收完一帧,要继续收
	if (!hd->bHead || !hd->bFrame)
		return;
	clidata->pa.flag = bitRemove(clidata->pa.flag, 0xF0);
	//--------------此处仅当做每帧都是结尾帧------------
	//if (hd->bFinal)
	{
		clidata->pa.flag |= (uchar)(hd->opCode << 4);
		switch (hd->opCode) {
			case 0: //0x0表示附加数据帧
				break;
			case 1: //0x1表示文本数据帧
			case 2: //0x2表示二进制数据帧
				//接收数据回调
				if (tw_conf->on_data)
					tw_conf->on_data(tw_conf->udata, client, &clidata->pa, &hd->buf);
				break;
			case 3: case 4: case 5: case 6: case 7: //0x3 - 7暂时无定义，为以后的非控制帧保留
				//membuf_uninit(hd->buf);
				//memset(hd, 0, sizeof(WebSocketHandle));
				break;
			case 8: //0x8表示连接关闭
				*(data + 1) = 0;//无数据
				tw_send_data(client, hd->buf.data, hd->buf.size, hd->opCode, 0);
				if (hd->buf.size > 1) { //错误信息
					int errcode = (hd->buf.data[0] << 8) + hd->buf.data[1];
					char errstr[60] = { 0 };
					if (hd->buf.size > 2)
						snprintf(errstr, 59, "wserr:%d %s", errcode, hd->buf.data + 2);
					else
						snprintf(errstr, 59, "wserr:%d", errcode);
					if (tw_conf->on_error) //出错信息回调
						tw_conf->on_error(tw_conf->udata, client, &clidata->pa, 0, errstr);
					else
						fprintf(stderr, errstr);
				}
				break;
			case 9: //0x9表示ping
			case 10://0xA表示pong
			default://0xB - F暂时无定义，为以后的控制帧保留
				*data += 1;//发送pong
				*(data + 1) = 0;//无数据
				tw_send_data(client, data, 2, 1, 0);
				//memset(hd, 0, sizeof(WebSocketHandle));
				break;
		}
		//释放 membuf , 收到消息时再分配
		WebSocketHandleInit(&clidata->hd);
		membuf_uninit(&hd->buf);
	}
}

//(循环)读取客户端发送的数据,接收客户的数据
static void on_uv_read(uv_stream_t* client, ssize_t nread, const uv_buf_t* buf) {
	tw_config* tw_conf = (tw_config*)(client->loop->data);
	tw_client* clidata = (tw_client*)client->data;
	membuf_t mbuf;
	if (nread > 0) {
		assert(clidata);
		//WebSocket
		if (clidata->pa.flag & 0x2) //WebSocket
			on_read_websocket(client, buf->base, nread);
		//long-link
		else if (clidata->pa.flag & 0x1) { //SOCKET //long-link
			//接收数据回调
			if (tw_conf->on_data) {
				mbuf.data = buf->base;
				mbuf.size = nread;
				mbuf.buffer_size = buf->len;
				tw_conf->on_data(tw_conf->udata, client, &clidata->pa, &mbuf);
			}
		}
		//http post继续接收 未收完的数据
		else if (clidata->heads.method == 2) {
			if (nread) {
				membuf_append_data(&clidata->buf, buf->base, nread);
				clidata->heads.len += nread;
			}
			if (clidata->heads.len >= clidata->heads.contentLen) {
				//所有请求全部回调,返回非0表示已处理
				clidata->heads.data = clidata->buf.data;
				if (tw_conf->on_request == 0 || 0 == tw_conf->on_request(tw_conf->udata, client, &clidata->pa, &clidata->heads))
					tw_request(client, &clidata->heads);
			}
		}
		//未知
		else { //http 或 未知
			char* p, *p2;
			//tw_reqHeads heads;
			//memset(&heads, 0, sizeof(tw_reqHeads));
			p = tw_get_http_heads(buf, nread, &clidata->heads);//get Sec-WebSocket-Key ?
			if (p) { //WebSocket 握手
				p2 = WebSocketHandShak(p);
				tw_send_data(client, p2, -1, 1, 0);
				free(p2);
				clidata->pa.flag |= 3;//long-link & WebSocket
			}
			else if (clidata->heads.method) { //HTTP
				if (!clidata->heads.path && clidata->heads.path[0] != '/') {//路径没有 '/' 开头
					tw_301_Moved(client, &clidata->heads, NULL);
				}
				else {//http post 数据
					if (clidata->heads.len >= clidata->heads.contentLen) {
						//所有请求全部回调,返回非0表示已处理
						if (tw_conf->on_request == 0 || 0 == tw_conf->on_request(tw_conf->udata, client, &clidata->pa, &clidata->heads))
							tw_request(client, &clidata->heads);
					}
					else {//跟随头部的 post 数据未发送完
						membuf_init(&clidata->buf, 128);
						if (clidata->heads.len)
							membuf_append_data(&clidata->buf, clidata->heads.data, clidata->heads.len);
					}
				}
			}
			else { //SOCKET
				clidata->pa.flag |= 1;//long-link
				//接收数据回调
				if (tw_conf->on_data) {
					mbuf.data = buf->base;
					mbuf.size = nread;
					mbuf.buffer_size = buf->len;
					tw_conf->on_data(tw_conf->udata, client, &clidata->pa, &mbuf);
				}
			}
		}
	}
	else if (nread <= 0) {//在任何情况下出错, read 回调函数 nread 参数都<0，如：出错原因可能是 EOF(遇到文件尾)
		if (nread != UV_EOF) {
			if (tw_conf->on_error) {
				char errstr[60] = { 0 };
				snprintf(errstr, 59, "%d:%s,%s", (int)nread, uv_err_name((int)nread), uv_strerror((int)nread));
				//出错信息回调
				tw_conf->on_error(tw_conf->udata, client, &clidata->pa, nread, errstr);
			}
			else
				fprintf(stderr, "%d:%s,%s\n", (int)nread, uv_err_name((int)nread), uv_strerror((int)nread));
		}
		//关闭连接. 读取长度为0,或是错误值,都应该关闭连接
		tw_close_client(client);
	}
	//每次使用完要释放
	if (buf->base)
		free(buf->base);
}

//为每次读取数据分配内存缓存
static void on_uv_alloc(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
	buf->base = (char*)calloc(1, suggested_size);
	buf->len = suggested_size;
}

//获取客户端的 socket,ip,port
static char tw_getPeerAddr(uv_stream_t* client, tw_peerAddr* pa) {
	struct sockaddr_in peeraddr[2], hostaddr[2]; //Windows 下不声明成数组(且数组长度大于1),getpeername会失败(errno=10014)
	int addrlen = sizeof(peeraddr);
	//memset(pa, 0, sizeof(PeerAddr));
	//客户端的地址
	if (client->type == UV_TCP) {
#ifdef WIN32
		pa->sk = ((uv_tcp_t*)client)->socket;
#else
		addrlen /= 2;
#if defined(__APPLE__)
		if (client->select)
			pa->sk = client->select;
		else
#endif
			pa->sk = client->io_watcher.fd;
#endif
		int er = uv_tcp_getpeername((uv_tcp_t*)client, (struct sockaddr*)peeraddr, &addrlen);
		if (er < 0)
			memset(peeraddr, 0, addrlen);
		er = uv_tcp_getsockname((uv_tcp_t*)client, (struct sockaddr*)hostaddr, &addrlen);
		if (er < 0)
			memset(hostaddr, 0, addrlen);
	}
	else if (client->type == UV_UDP) {
#ifdef WIN32
		pa->sk = ((uv_udp_t*)client)->socket;
#else
		pa->sk = client->io_watcher.fd;
#endif
		int er = uv_udp_getsockname((uv_udp_t*)client, (struct sockaddr*)peeraddr, &addrlen);
		if (er < 0)
			memset(peeraddr, 0, addrlen);
		er = uv_udp_getsockname((uv_udp_t*)client, (struct sockaddr*)hostaddr, &addrlen);
		if (er < 0)
			memset(hostaddr, 0, addrlen);
	}
	else
		return 1;
	//网络字节序转换成主机字符序
	uv_ip4_name(peeraddr, pa->ip, sizeof(pa->ip));
	pa->port = ntohs(peeraddr[0].sin_port);
	uv_ip4_name(hostaddr, pa->fip, sizeof(pa->ip));
	pa->fport = ntohs(hostaddr[0].sin_port);

	return 0;
}

//客户端接入
static void tw_on_connection(uv_stream_t* server, int status) {
	//assert(server == (uv_stream_t*)&_server);
	tw_client* cli;
	if (status == 0) {
		//建立客户端信息,在关闭连接时释放 see after_uv_close_client
		uv_tcp_t* client = (uv_tcp_t*)calloc(1, sizeof(uv_tcp_t));
		//创建客户端的数据缓存块,在关闭连接时释放 see after_uv_close_client
		cli = client->data = calloc(1, sizeof(tw_client));
		cli->hd.bFrame = 1;
		uv_tcp_init(server->loop, client);//将客户端放入loop
		//接受客户，保存客户端信息
		uv_accept(server, (uv_stream_t*)client);
		client->close_cb = after_uv_close_client;
		//取客户端 socket,ip,port;
		tw_getPeerAddr((uv_stream_t*)client, &cli->pa);
		//开始读取客户端数据
		uv_read_start((uv_stream_t*)client, on_uv_alloc, on_uv_read);
		//客户端接入回调
		tw_config* tw_conf = (tw_config*)(server->loop->data);
		if (tw_conf->on_connect)
			tw_conf->on_connect(tw_conf->udata, (uv_stream_t*)client, &cli->pa);
	}
}

//==================================================================================================

//TinyWeb 线程开始运行
static void tw_run(uv_loop_t* loop) {
	tw_config* tw_conf = (tw_config*)loop->data;
	printf("TinyWeb v1.2.2 is started, listening on %s:%d\n", tw_conf->ip, tw_conf->port);
	uv_run(loop, UV_RUN_DEFAULT);
	uv_stop(loop);
	if (!uv_loop_close(loop) && loop != uv_default_loop()) {
		uv_loop_delete(loop);
	}
	printf("TinyWeb v1.2.2 is stopped, listening on %s:%d\n", tw_conf->ip, tw_conf->port);
	free(tw_conf->doc_dir);
	free(tw_conf->doc_index);
	free(tw_conf->charset);
	free(tw_conf);
}

//start web server, start with the config
//loop: if is NULL , it will be uv_default_loop()
//conf: the server config
int tinyweb_start(uv_loop_t* loop, tw_config* conf) {
	int ret;
	assert(conf != NULL);
	if (conf->ip == NULL || (conf->ip != NULL && conf->ip[0] == '*'))
		conf->ip = "0.0.0.0";
	struct sockaddr_in addr;
	uv_ip4_addr(conf->ip, conf->port, &addr);

	tw_config* tw_conf = calloc(1, sizeof(tw_config));
	memcpy(tw_conf, conf, sizeof(tw_config));

	//设置主目录（末尾不带斜杠）
	if (conf->doc_dir)
		tw_conf->doc_dir = strdup(conf->doc_dir);
	else
		tw_conf->doc_dir = strdup("./");

	printf("WebRoot port:%d  Dir:%s\n", tw_conf->port, tw_conf->doc_dir);
	//设置默认主页（分号间隔）
	if (conf->doc_index && strcmpi(conf->doc_index, "") != 0)
		tw_conf->doc_index = strdup(conf->doc_index);
	else
		tw_conf->doc_index = strdup("index.htm;index.html");
	//设置more编码
	if (conf->charset)
		tw_conf->charset = strdup(conf->charset);
	else
		tw_conf->charset = strdup("utf-8");
	if (loop == NULL)
		loop = uv_default_loop();
	ret = uv_tcp_init(loop, &tw_conf->_server);
	if (ret < 0)
		return ret;
	ret = uv_tcp_bind(&tw_conf->_server, (const struct sockaddr*) &addr, 0);
	if (ret < 0)
		return ret;
	ret = uv_listen((uv_stream_t*)&tw_conf->_server, 8, tw_on_connection);
	if (ret < 0)
		return ret;
	loop->data = tw_conf;
	//开始线程
	uv_thread_t hare_id;
	uv_thread_create(&hare_id, (uv_thread_cb)tw_run, loop);
	return 0;
}

static void on_close_cb(uv_handle_t* handle) {}

//stop TinyWeb
//当执行uv_stop之后，uv_run并不能马上退出，而是要等待其内部循环的下一个iteration到来时才会退出；
//如果提前free掉loop就会导致loop失效。当然也可以sleep几十毫秒然后再close，但这么搞不太雅观。
//uv_stop以后不能马上执行uv_loop_close()
//貌似关闭及释放loop等资源不是很完善的样子
void tinyweb_stop(uv_loop_t* loop) {
	if (loop == NULL)
		loop = uv_default_loop();
	uv_stop(loop);
	if (loop->data)
		uv_close((uv_handle_t*)&((tw_config*)loop->data)->_server, on_close_cb);
	uv_loop_close(loop);
}
