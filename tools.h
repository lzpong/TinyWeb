#pragma once
#ifndef __TOOLS_H__
#define __TOOLS_H__

#ifdef __cplusplus
extern "C" {
#endif


//-----------------------------------------------------------------------------------membuf c-str  win/unix

typedef struct membuf_t {
	unsigned char* data;
	unsigned long   size;
	unsigned long   buffer_size;
	unsigned char  flag;//标志字节 ([0~7]: [0]是否需要保持连接 [1]是否WebSocket)   lzp 2016/11/28
} membuf_t;
//初始化
void membuf_init(membuf_t* buf, unsigned int initial_buffer_size);
//释放buffer
void membuf_uninit(membuf_t* buf);
//添加数据
unsigned int membuf_append_data(membuf_t* buf, const void* data, unsigned int size);
//插入数据：offset位置，data数据，size数据大小
void membuf_insert(membuf_t* buf, unsigned int offset, void* data, unsigned int size);
//从末尾移除数据（不会填充为NULL，仅更改size）
void membuf_remove(membuf_t* buf, unsigned int offset, unsigned int size);
//清除数据，并缩小buffer大小（数据重置为NULL）
void membuf_clear(membuf_t* buf, unsigned int maxSize);
//扩展buffer大小
void membuf_reserve(membuf_t* buf, unsigned int extra_size);
//截断(释放)多余的内存
void membuf_trunc(membuf_t* buf);

#if defined(_MSC_VER)
#define _INLINE static _inline
#else
#define _INLINE static inline
#endif

_INLINE unsigned int membuf_append_byte(membuf_t* buf, unsigned char b) {
	return membuf_append_data(buf, &b, sizeof(b));
}
_INLINE unsigned int membuf_append_int(membuf_t* buf, int i) {
	return membuf_append_data(buf, &i, sizeof(i));
}
_INLINE unsigned int membuf_append_uint(membuf_t* buf, unsigned int ui) {
	return membuf_append_data(buf, &ui, sizeof(ui));
}
_INLINE unsigned int membuf_append_long(membuf_t* buf, long i) {
	return membuf_append_data(buf, &i, sizeof(i));
}
_INLINE unsigned int membuf_append_ulong(membuf_t* buf, unsigned long ui) {
	return membuf_append_data(buf, &ui, sizeof(ui));
}
_INLINE unsigned int membuf_append_short(membuf_t* buf, short s) {
	return membuf_append_data(buf, &s, sizeof(s));
}
_INLINE unsigned int membuf_append_ushort(membuf_t* buf, unsigned short us) {
	return membuf_append_data(buf, &us, sizeof(us));
}
_INLINE unsigned int membuf_append_float(membuf_t* buf, float f) {
	return membuf_append_data(buf, &f, sizeof(f));
}
_INLINE unsigned int membuf_append_double(membuf_t* buf, double d) {
	return membuf_append_data(buf, &d, sizeof(d));
}
_INLINE unsigned int membuf_append_ptr(membuf_t* buf, void* ptr) {
	return membuf_append_data(buf, &ptr, sizeof(ptr));
}

//-----------------------------------------------------------------------------------文件/文件夹检测  win/unix

//获取工作目录路径,不带'/'
char* getWorkPath();
//获取程序文件所在路径,不带'/'
char* getProcPath();

//路径是否存在(0：不存在  1：存在:文件  2：存在:文件夹)
char isExist(const char* path);
//是否文件(1:是文件  0:非文件/不存在)
char isFile(const char* path);
//是否目录(1:是目录  0;非目录/不存在)
char isDir(const char* path);

//网页，列表目录,need free the return
char* listDir(const char* fullpath, const char* reqPath);

//-----------------------------------------------------------------------------------编码转换  win/unix

#ifdef _MSC_VER
//GB2312 to unicode(need free return) 返回字串长度为：实际长度+2, 末尾\0\0占一字节（需要释放）
wchar_t* GB2U(char* pszGbs, size_t* wLen);
//unicode to utf8(need free return) 返回字串长度为：实际长度+1, 末尾\0占一字节（需要释放）
char* U2U8(wchar_t* wszUnicode, size_t* aLen);
//utf8 to unicode(need free return) 返回字串长度为：实际长度+2, 末尾\0\0占一字节（需要释放）
wchar_t* U82U(char* szU8, size_t* wLen);
//unicode to GB2312(need free return) 返回字串长度为：实际长度+1, 末尾\0占一字节（需要释放）
char* U2GB(wchar_t* wszUnicode, size_t* aLen);
#else
//GB2312 to unicode(need free return) 返回字串长度为：实际长度+2,返回长度小于0为：失败, 末尾\0\0占一字节（需要释放）
char* GB2U(char* pszGbs, size_t* aLen);
//unicode to utf8(need free return) 返回字串长度为：实际长度+1,返回长度小于0为：失败, 末尾\0占一字节（需要释放）
char* U2U8(char* wszUnicode, size_t* aLen);
//utf8 to unicode(need free return) 返回字串长度为：实际长度+2,返回长度小于0为：失败, 末尾\0\0占一字节（需要释放）
char* U82U(char* szU8, size_t* aaLen);
//unicode to GB2312(need free return) 返回字串长度为：实际长度+1,返回长度小于0为：失败, 末尾\0占一字节（需要释放）
char* U2GB(char* wszUnicode, size_t* aLen);
#endif
//GB2312 to utf8(need free return) 返回字串长度为：实际长度+1,返回长度小于0为：失败, 末尾\0占一字节（需要释放）
char* GB2U8(char* pszGbs, size_t* aLen);
//utf8 to GB2312(need free return) 返回字串长度为：实际长度+1,返回长度小于0为：失败, 末尾\0占一字节（需要释放）
char* U82GB(char* szU8, size_t* aLen);

//-----------------------------------------------------------------------------------Base64编码解码  win/unix

//Base64编码,需要释放返回值(need free return)
char* base64_Encode(unsigned char const* bytes_to_encode, unsigned int in_len);
//Base64解码,需要释放返回值(need free return)
char* base64_Decode(char* const encoded_string);

//-----------------------------------------------------------------------------------Hash1加密  win/unix

typedef unsigned int uint;
typedef struct SHA1_CONTEXT {
	char bFinal:1;//是否计算完成
	uint  h0, h1, h2, h3, h4;
	uint  nblocks;
	uint  count;
	unsigned char buf[64];
} SHA1_CONTEXT;
//初始化/重置结构体
void hash1_Reset(SHA1_CONTEXT* hd);

/* Update the message digest with the contents of INBUF with length INLEN. */
void hash1_Write(SHA1_CONTEXT* hd, unsigned char *inbuf, size_t inlen);

/* The routine final terminates the computation and returns the digest.
* The handle is prepared for a new cycle, but adding bytes to the
* handle will the destroy the returned buffer.
* Returns: 20 bytes representing the digest.
*/
void hash1_Final(SHA1_CONTEXT* hd);

//取得hash值: hd->buf
unsigned char* hash1_Get(SHA1_CONTEXT* hd);

//-----------------------------------------------------------------------------------url编码解码  win/unix

//url编码 (len为url的长度)
char* url_encode(const char *url, size_t* len);
//url解码
char* url_decode(char *url);

//-----------------------------------------------------------------------------------WebSocket  win/unix

//WebSocket以文本传输的时候，都为UTF - 8编码，是WebSocket协议允许的唯一编码
//服务端收数据用
typedef struct WebSocketHandle {
	membuf_t buf;//原始帧数据
//data[0]
	unsigned char isEof:1;//是否是结束帧 data[0]>>7
	unsigned char dfExt:3;//是否有扩展定义 (data[0]>>4) & 0x7
	/*帧类型 type 的定义data[0] & 0xF
	0x0表示附加数据帧
	0x1表示文本数据帧
	0x2表示二进制数据帧
	0x3-7暂时无定义，为以后的非控制帧保留
	0x8表示连接关闭
	0x9表示ping
	0xA表示pong
	0xB-F暂时无定义，为以后的控制帧保留 */
	unsigned char type:4;

} WebSocketHandle;
//传入http头,返回WebSocket握手Key,非http升级ws则返回NULL
//需要释放返回值(need free return)
char* WebSocketHandShak(const char* key);
//从帧中取得实际数据 (帧不应超过32位大小)
unsigned long WebSocketGetData(WebSocketHandle* handle,char* data, unsigned long len);
//转换为一个WebSocket帧,无mask
char* WebSocketMakeFrame(const char* data, unsigned int* len);

//-----------------------------------------------------------------------------------工具函数/杂项  win/unix

//获取IPv4地址 (第一个IPv4)
const char* GetIPv4();
//获取IPv6地址 (第一个IPv6)
const char* GetIPv6();



typedef struct tm_u {
	int tm_sec;     /*秒 seconds after the minute - [0,59] */
	int tm_min;     /*分 minutes after the hour - [0,59] */
	int tm_hour;    /*时 hours since midnight - [0,23] */
	int tm_mday;    /*天 day of the month - [1,31] */
	int tm_mon;     /*月 months since January - [1,12] */
	int tm_year;    /*年 years since 1900 */
	int tm_wday;    /*星期 days since Sunday - [0,6 0:周日] */
	int tm_yday;    /*年中的天数 days since January 1 - [0,365] */
	int tm_isdst;   /*夏令时标志 daylight savings time flag */
	time_t tm_vsec; /*时间戳 seconds from 1900/1/1 0:0:0 */
	int tm_usec;    /*微妙 microseconds */
} tm_u;

//获取当前时间信息
tm_u GetLocaTime();
//获取当天已逝去的秒数
inline size_t GetDaySecond();
//字符串转换成时间戳(毫秒),字符串格式为:"2016-08-03 06:56:36"
inline time_t str2stmp(const char *strTime);
//时间戳(毫秒)转换成字符串,字符串格式为:"2016-08-03 06:56:36"
inline char* stmp2str(time_t t, char* str, int strlen);





#ifdef __cplusplus
} // extern "C"
#endif


#endif //__TOOLS_H__