#pragma once
#ifndef __TOOLS_H__
#define __TOOLS_H__

#ifdef __cplusplus
extern "C" {
#endif

	typedef unsigned char       uchar;
	typedef unsigned short      ushort;
	typedef unsigned int        uint;
	typedef unsigned long       ulong;
	typedef long long           llong;
	typedef unsigned long long  ullong;

#define bitAdd(a,b) ((a)|(b))        //加上二进制位
#define bitHas(a,b) ((a)&(b))        //是否此有二进制位
#define bitRemove(a,b) ((a)&~(b))    //减去二进制位
#define Min(a,b) (((a)>(b))?(b):(a)) //最小值
#define Max(a,b) (((a)>(b))?(a):(b)) //最大值

	//-----------------------------------------------------------------------------------membuf c-str  win/unix

	typedef struct membuf_t {
		uchar* data;
		size_t  size;
		size_t  buffer_size;
	} membuf_t;
	//初始化
	void membuf_init(membuf_t* buf, size_t initial_buffer_size);
	//释放buffer
	void membuf_uninit(membuf_t* buf);
	//添加数据
	size_t membuf_append_data(membuf_t* buf, const void* data, size_t size);
	//按格式添加数据
	size_t membuf_append_format(membuf_t* buf, const char* fmt, ...);
	//插入数据：offset位置，data数据，size数据大小
	void membuf_insert(membuf_t* buf, size_t offset, void* data, size_t size);
	//从末尾移动数据（不会填充为NULL，仅更改size）
	void membuf_move(membuf_t* buf, size_t offset, size_t size);
	//清除数据，并缩小buffer大小（数据重置为NULL）
	void membuf_clear(membuf_t* buf, size_t maxSize);
	//扩展buffer大小
	void membuf_reserve(membuf_t* buf, size_t extra_size);
	//截断(释放)多余的内存 或者增加内存,至 size+4 的大小; 后面4字节填充0
	void membuf_trunc(membuf_t* buf);

#if defined(_MSC_VER)
#define _INLINE static _inline
#else
#define _INLINE static inline
#endif

	_INLINE size_t membuf_append_byte(membuf_t* buf, uchar b) {
		return membuf_append_data(buf, &b, sizeof(b));
	}
	_INLINE size_t membuf_append_int(membuf_t* buf, int i) {
		return membuf_append_data(buf, &i, sizeof(i));
	}
	_INLINE size_t membuf_append_uint(membuf_t* buf, uint ui) {
		return membuf_append_data(buf, &ui, sizeof(ui));
	}
	_INLINE size_t membuf_append_long(membuf_t* buf, long i) {
		return membuf_append_data(buf, &i, sizeof(i));
	}
	_INLINE size_t membuf_append_ulong(membuf_t* buf, ulong ui) {
		return membuf_append_data(buf, &ui, sizeof(ui));
	}
	_INLINE size_t membuf_append_short(membuf_t* buf, short s) {
		return membuf_append_data(buf, &s, sizeof(s));
	}
	_INLINE size_t membuf_append_ushort(membuf_t* buf, ushort us) {
		return membuf_append_data(buf, &us, sizeof(us));
	}
	_INLINE size_t membuf_append_float(membuf_t* buf, float f) {
		return membuf_append_data(buf, &f, sizeof(f));
	}
	_INLINE size_t membuf_append_double(membuf_t* buf, double d) {
		return membuf_append_data(buf, &d, sizeof(d));
	}
	_INLINE size_t membuf_append_ptr(membuf_t* buf, void* ptr) {
		return membuf_append_data(buf, &ptr, sizeof(ptr));
	}

	//-----------------------------------------------------------------------------------文件/文件夹检测  win/unix

	//获取工作目录路径,不带'/'
	char* getWorkPath();
	//获取程序文件所在路径,不带'/'
	char* getProcPath();
	//建立目录,递归建立 (mod: linux系统需要,权限模式,, windows系统不需要)
	int makeDir(const char* path, int mod);
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
	wchar_t* GB2U(char* pszGbs, uint* wLen);
	//unicode to utf8(need free return) 返回字串长度为：实际长度+1, 末尾\0占一字节（需要释放）
	char* U2U8(wchar_t* wszUnicode, uint* aLen);
	//utf8 to unicode(need free return) 返回字串长度为：实际长度+2, 末尾\0\0占一字节（需要释放）
	wchar_t* U82U(char* szU8, uint* wLen);
	//unicode to GB2312(need free return) 返回字串长度为：实际长度+1, 末尾\0占一字节（需要释放）
	char* U2GB(wchar_t* wszUnicode, uint* aLen);
#else
	//GB2312 to unicode(need free return) 返回字串长度为：实际长度+2,返回长度小于0为：失败, 末尾\0\0占一字节（需要释放）
	char* GB2U(char* pszGbs, uint* aLen);
	//unicode to utf8(need free return) 返回字串长度为：实际长度+1,返回长度小于0为：失败, 末尾\0占一字节（需要释放）
	char* U2U8(char* wszUnicode, uint* aLen);
	//utf8 to unicode(need free return) 返回字串长度为：实际长度+2,返回长度小于0为：失败, 末尾\0\0占一字节（需要释放）
	char* U82U(char* szU8, uint* aaLen);
	//unicode to GB2312(need free return) 返回字串长度为：实际长度+1,返回长度小于0为：失败, 末尾\0占一字节（需要释放）
	char* U2GB(char* wszUnicode, uint* aLen);
#endif
	//GB2312 to utf8(need free return) 返回字串长度为：实际长度+1,返回长度小于0为：失败, 末尾\0占一字节（需要释放）
	char* GB2U8(char* pszGbs, uint* aLen);
	//utf8 to GB2312(need free return) 返回字串长度为：实际长度+1,返回长度小于0为：失败, 末尾\0占一字节（需要释放）
	char* U82GB(char* szU8, uint* aLen);

	char* enc_u82u(char* data, uint* len);
	char* enc_u2u8(char* data, uint* len);

	//-----------------------------------------------------------------------------------Base64编码解码  win/unix

	//Base64编码,需要释放返回值(need free return)
	char* base64_Encode(uchar const* bytes_to_encode, uint in_len);
	//Base64解码,需要释放返回值(need free return)
	char* base64_Decode(char* const encoded_string);

	//-----------------------------------------------------------------------------------Hash1加密  win/unix

	typedef struct SHA1_CONTEXT {
		char bFinal : 1;//是否计算完成
		uint  h0, h1, h2, h3, h4;
		uint  nblocks;
		uint  count;
		uchar buf[64];
	} SHA1_CONTEXT;
	//初始化/重置结构体
	void hash1_Reset(SHA1_CONTEXT* hd);

	/* Update the message digest with the contents of INBUF with length INLEN. */
	void hash1_Write(SHA1_CONTEXT* hd, uchar *inbuf, size_t inlen);

	/* The routine final terminates the computation and returns the digest.
	* The handle is prepared for a new cycle, but adding bytes to the
	* handle will the destroy the returned buffer.
	* Returns: 20 bytes representing the digest.
	*/
	void hash1_Final(SHA1_CONTEXT* hd);

	//取得hash值: hd->buf
	uchar* hash1_Get(SHA1_CONTEXT* hd);

	//-----------------------------------------------------------------------------------url编码解码  win/unix

	//url编码 (len为url的长度)
	char* url_encode(const char *url, uint* len);
	//url解码
	char* url_decode(char *url);

	//-----------------------------------------------------------------------------------WebSocket  win/unix

	//WebSocket以文本传输的时候，都为UTF - 8编码，是WebSocket协议允许的唯一编码
	//服务端收数据用
	typedef struct WebSocketHandle {
		membuf_t buf;//原始帧数据
	//data[0]
		uchar isEof : 1;//是否是结束帧 data[0]>>7
		uchar dfExt : 3;//是否有扩展定义 (data[0]>>4) & 0x7
		/*控制码/帧类型 type 的定义data[0] & 0xF
		0x0表示附加数据帧
		0x1表示文本数据帧
		0x2表示二进制数据帧
		0x3-7暂时无定义，为以后的非控制帧保留
		0x8表示连接关闭
		0x9表示ping
		0xA表示pong
		0xB-F暂时无定义，为以后的控制帧保留 */
		uchar type : 4;
	} WebSocketHandle;
	//传入http头,返回WebSocket握手Key,非http升级ws则返回NULL
	//需要释放返回值(need free return)
	char* WebSocketHandShak(const char* key);
	//从帧中取得实际数据 (帧不应超过32位大小)
	ulong WebSocketGetData(WebSocketHandle* handle, char* data, ulong len);
	//转换为一个WebSocket帧,无mask (need free return)
	//op:控制码/帧类型  0x0表示附加数据帧   0x1表示文本数据帧 ...
	char* WebSocketMakeFrame(const char* data, ulong* len, uchar op);

	//-----------------------------------------------------------------------------------工具函数/杂项  win/unix

	//获取IPv4地址 (第一个IPv4)
	const char* GetIPv4();
	//获取IPv6地址 (第一个IPv6)
	const char* GetIPv6();
	//获取网卡地址
	const char* GetMacAddr();

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
		llong tm_vsec; /*时间戳 seconds from 1900/1/1 0:0:0 */
		int tm_usec;    /*微妙 microseconds */
	} tm_u;

	//获取当前时间信息
	tm_u GetLocaTime();

	//获取当天已逝去的秒数
	size_t GetDaySecond();

	//获取格林制（GMT）时间: "Wed, 18 Jul 2018 06:02:42 GMT"
	//szDate: 存放GMT时间的缓存区(至少 char[30])，外部传入
	void getGmtTime(char* szDate);

	//字符串转换成时间戳(秒),字符串格式为:"2016-08-03 06:56:36"
	llong str2stmp(const char *strTime);

	//时间戳(秒)转换成字符串,字符串格式为:"2016-08-03 06:56:36"
	char* stmp2str(llong t, char* str, int strlen);

	//从头比较字符串,返回相同的长度,不区分大小写
	size_t strinstr(const char* s1, const char* s2);

	//int32 转二进制字符串
	char* u2b(uint n);
	//int64 转二进制字符串
	char* u2b64(ullong n);

#ifdef __cplusplus
} // extern "C"
#endif

#endif //__TOOLS_H__