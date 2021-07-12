//this file codes is for windows
#ifdef _MSC_VER
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#endif

#ifdef _MSC_VER
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif // !WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <time.h>
#include <io.h>

//#  if defined(WIN32)
//#  define snprintf _snprintf
//#  endif

#else //__GNUC__
#include <errno.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/time.h>
#include <iconv.h>
#endif

#include "tools.h"

//#include "membuf.h"
#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <string.h>
#include <limits.h>
#include <stdarg.h>

//-----------------------------------------------------------------------------------membuf c-str  win/linux
#pragma region membuf c-str

#include <assert.h>
//初始化
void membuf_init(membuf_t* buf, size_t initial_buffer_size) {
	memset(buf, 0, sizeof(membuf_t));
	buf->data = initial_buffer_size > 0 ? (uchar*)calloc(1, initial_buffer_size) : NULL;
	//memset(buf->data, 0, initial_buffer_size);
	buf->buffer_size = initial_buffer_size;
}
//释放buffer
void membuf_uninit(membuf_t* buf) {
	if (buf->data)
		free(buf->data);
	memset(buf, 0, sizeof(membuf_t));
}
//清除数据（数据覆盖为NULL），并缩小buffer大小
void membuf_clear(membuf_t* buf, size_t maxSize) {
	if (buf->data && buf->size) {
		if (maxSize > 1 && buf->buffer_size > maxSize) {
			uchar* p = (uchar*)realloc(buf->data, maxSize);
			//防止realloc分配失败，或返回的地址一样
			assert(p);
			if (p != buf->data)
				buf->data = p;
			buf->size = 0;
			buf->buffer_size = maxSize;
		}
		else {
			buf->size = 0;
		}
		memset(buf->data, 0, buf->buffer_size);
	}
}
////扩展buffer大小
void membuf_reserve(membuf_t* buf, size_t extra_size) {
	if (extra_size > buf->buffer_size - buf->size) {
		//calculate new buffer size
		size_t new_buffer_size = buf->buffer_size == 0 ? extra_size : buf->buffer_size << 1;
		size_t new_data_size = buf->size + extra_size;
		while (new_buffer_size < new_data_size)
			new_buffer_size <<= 1;

		// malloc/realloc new buffer
		uchar* p = (uchar*)realloc(buf->data, new_buffer_size); // realloc new buffer
		//防止realloc分配失败，或返回的地址一样
		assert(p);
		if (p != buf->data)
			buf->data = p;
		memset((buf->data + buf->size), 0, new_buffer_size - buf->size);
		buf->buffer_size = new_buffer_size;
	}
}
//截断(释放)多余的内存 或者增加内存,至 size+4 的大小; 后面4字节填充0
void membuf_trunc(membuf_t* buf) {
	if (buf->buffer_size > (buf->size + 4) || buf->buffer_size < (buf->size + 4)) {
		uchar* p = (uchar*)realloc(buf->data, buf->size + 4); // realloc new buffer
		//防止realloc分配失败，或返回的地址一样
		assert(p);
		if (p && p != buf->data)
			buf->data = p;
		memset((buf->data + buf->size), 0, 4);
		buf->buffer_size = buf->size + 4;
	}
}
//添加C-style字符串
size_t membuf_append(membuf_t* buf, const char* str) {
	if (str == NULL) return 0;
	size_t size = strlen(str);
	membuf_reserve(buf, size);
	memmove((buf->data + buf->size), str, size);
	buf->size += size;
	return size;
}
//添加数据
size_t membuf_append_data(membuf_t* buf, const void* data, size_t size) {
	assert(data && size > 0);
	membuf_reserve(buf, size);
	memmove((buf->data + buf->size), data, size);
	buf->size += size;
	return size;
}
//按格式添加数据
size_t membuf_append_format(membuf_t* buf, const char* fmt, ...) {
	assert(fmt);
	va_list ap, ap2;
	va_start(ap, fmt);
	size_t size = vsnprintf(0, 0, fmt, ap) + 1;
	va_end(ap);
	membuf_reserve(buf, size);
	va_start(ap2, fmt);
	vsnprintf((char*)(buf->data + buf->size), size, fmt, ap2);
	va_end(ap2);
	buf->size += --size;
	return size;
}
//插入数据：offset位置，data数据，size数据大小
void membuf_insert(membuf_t* buf, size_t offset, void* data, size_t size) {
	assert(offset < buf->size);
	membuf_reserve(buf, size);
	memcpy((buf->data + offset + size), buf->data + offset, buf->size - offset);
	memcpy((buf->data + offset), data, size);
	buf->size += size;
}
//从末尾移动数据（不会填充为NULL，仅更改size）
void membuf_move(membuf_t* buf, size_t offset, size_t size) {
	assert(offset < buf->size);
	if (offset + size >= buf->size) {
		buf->size = offset;
	}
	else {
		//memmove() 用来复制内存内容（可以处理重叠的内存块）：void * memmove(void *dest, const void *src, size_t num);
		memmove((buf->data + offset), buf->data + offset + size, buf->size - offset - size);
		buf->size -= size;
	}
	if (buf->buffer_size >= buf->size)
		buf->data[buf->size] = 0;
}

#pragma endregion

//-----------------------------------------------------------------------------------文件/文件夹检测  win/linux
#pragma region 文件/文件夹检测

#ifdef _MSC_VER
#include <direct.h>

//获取工作目录路径,不带'\'
char* getWorkPath() {
	static char CurPath[260] = { 0 };
	GetCurrentDirectory(259, CurPath);
	return CurPath;
}
//获取程序文件所在路径,不带'\'
char* getProcPath() {
	static char CurPath[260] = { 0 };
	GetModuleFileName(GetModuleHandle(NULL), CurPath, 259);
	//获取当前目录绝对路径，即去掉程序名，包括去掉最后的'\'
	size_t i = strlen(CurPath) - 1;
	for (; i > 0 && CurPath[i] != '\\'; --i) {
		CurPath[i] = 0;
	}
	if (i > 2 && CurPath[i] == '\\')
		CurPath[i] = 0;
	return CurPath;
}

//建立目录,递归建立
int makeDir(const char * path, int mod) {
	char pth[513];
	strncpy(pth, path, 512);
	char *p = strrchr(pth, '\\');
	if (!p)
		p = (char*)strrchr(pth, '/');
	if (p) {
		if (strlen(p) == 1)
			*p = 0;
		p = strrchr(pth, '\\');
		if (!p)
			p = (char*)strrchr(pth, '/');
		if (p) {
			*p = 0;
			if (!isExist(pth))
				makeDir(pth, mod);
		}
	}
	return _mkdir(path);
}

//获取文件/文件夹信息
inline struct _finddata_t GetFileInfo(const char* lpPath) {
	struct _finddata_t fileinfo;
	memset(&fileinfo, 0, sizeof(struct _finddata_t));
	intptr_t hFind = _findfirst(lpPath, &fileinfo);
	_findclose(hFind);
	return fileinfo;
}

//路径是否存在(0：不存在  1：存在:文件  2：存在:文件夹)
char isExist(const char* path) {
	struct _finddata_t fd = GetFileInfo(path);
	return (fd.name[0] && fd.attrib) ? ((fd.attrib & FILE_ATTRIBUTE_DIRECTORY) ? 2 : 1) : 0;
}

//是否文件(1:是文件  0:非文件/不存在)
char isFile(const char* path) {
	struct _finddata_t fd = GetFileInfo(path);
	return (fd.name[0] && !(fd.attrib & FILE_ATTRIBUTE_DIRECTORY));
}

//是否目录(1:是目录  0;非目录/不存在)
char isDir(const char* path) {
	struct _finddata_t fd = GetFileInfo(path);
	return (fd.name[0] && (fd.attrib & FILE_ATTRIBUTE_DIRECTORY));
}

//返回列表目录Json字符串,need free the return
char* listDir(const char* fullpath, const char* reqPath) {
	int fnum = 0;
	membuf_t buf;
	membuf_init(&buf, 2048);
	membuf_append_format(&buf, "{\"path\":\"%s\",\"files\":[\r\n", reqPath);

	//文件(size>-1) 或 目录（size=-1）   [name:"file1.txt",mtime:"2016-11-28 16:25:46",size:123],\r\n
	struct _finddatai64_t fdt;
	intptr_t hFind;
	char szFind[256];

	snprintf(szFind, 255, "%s\\*", fullpath);
	hFind = _findfirsti64(szFind, &fdt);
	//[name:"file1.txt",mtime:"2016-11-28 16:25:46",size:123],\r\n
	while (hFind != -1) {//一次查找循环
		//最后修改时间
		struct tm *t = localtime(&fdt.time_write);//年月日 时分秒
		if (fdt.attrib & FILE_ATTRIBUTE_DIRECTORY) {//文件夹
			if (strncmp(fdt.name, ".", 1) == 0) {
				if (_findnexti64(hFind, &fdt))
					break;//下一个文件
				continue;
			}
			membuf_append_format(&buf, "{\"name\":\"%s/\",\"mtime\":\"%d-%02d-%02d %02d:%02d:%02d\",\"size\":\"-\",\"type\":\"D\"},\n", fdt.name, t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
		}
		else {//文件
			fnum++;
			membuf_append_format(&buf, "{\"name\":\"%s\",\"mtime\":\"%d-%02d-%02d %02d:%02d:%02d\",\"size\":%lld,\"type\":\"F\"},\n", fdt.name, t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec, fdt.size);
		}
		if (_findnexti64(hFind, &fdt))
			break;//下一个文件
	}
	_findclose(hFind);
	//membuf_remove(&buf, buf.size-1, 1);
	buf.data[--buf.size] = 0; buf.data[--buf.size] = 0;
	membuf_append_format(&buf, "],total:%d}", fnum);
	//window下需要转换为UTF8编码，以发送给客户端
	membuf_trunc(&buf);
	return (char*)buf.data;
}

#else //_GNUC_

//获取工作目录路径,不带'/'
char* getWorkPath() {
	static char CurPath[260] = { 0 };
	getcwd(CurPath, 259);
	return CurPath;
}

//建立目录,递归建立
int makeDir(const char * path, int mod) {
	char pth[513];
	strncpy(pth, path, 512);
	char *p = strrchr(pth, '\\');
	if (!p)
		p = (char*)strrchr(pth, '/');
	if (p) {
		if (strlen(p) == 1)
			*p = 0;
		p = strrchr(pth, '\\');
		if (!p)
			p = (char*)strrchr(pth, '/');
		if (p) {
			*p = 0;
			if (!isExist(pth))
				makeDir(pth, mod);
		}
	}
	return mkdir(path, mod);
}

//获取程序文件所在路径,不带'/'
char* getProcPath() {
	static char CurPath[260] = { 0 };
	int cnt = readlink("/proc/self/exe", CurPath, 259);
	if (cnt > 0 || cnt < 260) {
		//获取程序路径，即去掉程序名，包括去掉最后的'/'
		int i;
		for (i = cnt - 1; i > 0 && CurPath[i] != '/'; --i) {
			CurPath[i] = 0;
		}
		if (i > 2 && CurPath[i] == '/')
			CurPath[i] = 0;
	}
	return CurPath;
}

//路径是否存在(0：不存在  1：存在:文件  2：存在:文件夹)
char isExist(const char* path) {
	if (path && access(path, F_OK) == 0) {
		struct stat info;
		stat(path, &info);
		if (S_ISDIR(info.st_mode) || S_ISLNK(info.st_mode))//dir or link
			return 2;
		return 1;
	}
	return 0;
}
//是否目录(1:是目录  0;非目录/不存在)
char isDir(const char* path) {
	if (path && access(path, F_OK) == 0) {// && opendir(path)!=NULL)
		struct stat info;
		stat(path, &info);
		if (S_ISDIR(info.st_mode) || S_ISLNK(info.st_mode))//dir or link
			return 1;
	}
	return 0;
}
//是否文件(1:是文件  0:非文件/不存在)
char isFile(const char* path) {
	if (path && access(path, F_OK) == 0) {
		struct stat info;
		stat(path, &info);
		if (S_ISREG(info.st_mode))//普通文件
			return 1;
	}
	return 0;
}

//返回列表目录Json字符串,need free the return
char* listDir(const char* fullpath, const char* reqPath) {
	int fnum = 0;
	char tmp[1024];
	struct tm *mtime;
	DIR *dp;
	struct dirent *fileInfo;
	struct stat statbuf;
	membuf_t buf;
	membuf_init(&buf, 2048);
	membuf_append_format(&buf, "{\"path\":\"%s\",\"files\":[\r\n", reqPath);

	//文件(size>-1) 或 目录（size=-1）   [name:"file1.txt",mtime:"2016-11-28 16:25:46",size:123],\r\n
	if ((dp = opendir(fullpath)) != NULL) {
		while ((fileInfo = readdir(dp)) != NULL) {
			snprintf(tmp, 1023, "%s/%s", fullpath, fileInfo->d_name);
			stat(tmp, &statbuf);//stat函数需要传入绝对路径或相对（工作目录的）路径
			mtime = localtime(&statbuf.st_mtime);
			if (S_ISDIR(statbuf.st_mode)) {
				if (strncmp(fileInfo->d_name, ".", 1) == 0)
					continue;
				membuf_append_format(&buf, "{\"name\":\"%s/\",\"mtime\":\"%d-%02d-%02d %02d:%02d:%02d\",\"size\":\"-\",\"type\":\"D\"},\n", fileInfo->d_name, (1900 + mtime->tm_year), (1 + mtime->tm_mon), mtime->tm_mday, mtime->tm_hour, mtime->tm_min, mtime->tm_sec);
			}
			else {
				fnum++;
				membuf_append_format(&buf, "{\"name\":\"%s\",\"mtime\":\"%d-%02d-%02d %02d:%02d:%02d\",\"size\":%ld,\"type\":\"F\"},\n", fileInfo->d_name, (1900 + mtime->tm_year), (1 + mtime->tm_mon), mtime->tm_mday, mtime->tm_hour, mtime->tm_min, mtime->tm_sec, statbuf.st_size);
			}
		}
		closedir(dp);
	}
	//membuf_remove(&buf, buf.size - 1, 1);
	buf.data[--buf.size] = 0; buf.data[--buf.size] = 0;
	membuf_append_format(&buf, "],total:%d}", fnum);
	membuf_trunc(&buf);
	return (char*)buf.data;
}
#endif

#pragma endregion

//-----------------------------------------------------------------------------------编码转换  win/linux
#pragma region 编码转换

/*****************************************************************************
* 将一个字符的Unicode(UCS-2和UCS-4)编码转换成UTF-8编码.
*
* 参数:
*    unic     字符的Unicode编码值
*    pOutput  指向输出的用于存储UTF8编码值的缓冲区的指针
*    outsize  pOutput缓冲的大小
*
* 返回值:
*    返回转换后的字符的UTF8编码所占的字节数, 如果出错则返回 0 .
*
* 注意:
*     1. UTF8没有字节序问题, 但是Unicode有字节序要求;
*        字节序分为大端(Big Endian)和小端(Little Endian)两种;
*        在Intel处理器中采用小端法表示, 在此采用小端法表示. (低地址存低位)
*     2. 请保证 pOutput 缓冲区有最少有 6 字节的空间大小!
****************************************************************************/
int enc_unicode_to_utf8_one(size_t unic, uchar *pOutput, int outSize) {
	assert(pOutput != NULL);
	assert(outSize >= 6);

	if (unic <= 0x0000007F) {
		// U-00000000 - U-0000007F:  0xxxxxxx
		*pOutput = (unic & 0x7F);
		return 1;
	}
	else if (unic >= 0x00000080 && unic <= 0x000007FF) {
		// * U-00000080 - U-000007FF:  110xxxxx 10xxxxxx
		*pOutput = ((unic >> 6) & 0x1F) | 0xC0;
		*(pOutput + 1) = (unic & 0x3F) | 0x80;
		return 2;
	}
	else if (unic >= 0x00000800 && unic <= 0x0000FFFF) {
		// U-00000800 - U-0000FFFF:  1110xxxx 10xxxxxx 10xxxxxx
		*pOutput = ((unic >> 12) & 0x0F) | 0xE0;
		*(pOutput + 1) = ((unic >> 6) & 0x3F) | 0x80;
		*(pOutput + 2) = (unic & 0x3F) | 0x80;
		return 3;
	}
	else if (unic >= 0x00010000 && unic <= 0x001FFFFF) {
		// U-00010000 - U-001FFFFF:  11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
		*pOutput = ((unic >> 18) & 0x07) | 0xF0;
		*(pOutput + 1) = ((unic >> 12) & 0x3F) | 0x80;
		*(pOutput + 2) = ((unic >> 6) & 0x3F) | 0x80;
		*(pOutput + 3) = (unic & 0x3F) | 0x80;
		return 4;
	}
	else if (unic >= 0x00200000 && unic <= 0x03FFFFFF) {
		// U-00200000 - U-03FFFFFF:  111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
		*pOutput = ((unic >> 24) & 0x03) | 0xF8;
		*(pOutput + 1) = ((unic >> 18) & 0x3F) | 0x80;
		*(pOutput + 2) = ((unic >> 12) & 0x3F) | 0x80;
		*(pOutput + 3) = ((unic >> 6) & 0x3F) | 0x80;
		*(pOutput + 4) = (unic & 0x3F) | 0x80;
		return 5;
	}
	else if (unic >= 0x04000000 && unic <= 0x7FFFFFFF) {
		// U-04000000 - U-7FFFFFFF:  1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
		*pOutput = ((unic >> 30) & 0x01) | 0xFC;
		*(pOutput + 1) = ((unic >> 24) & 0x3F) | 0x80;
		*(pOutput + 2) = ((unic >> 18) & 0x3F) | 0x80;
		*(pOutput + 3) = ((unic >> 12) & 0x3F) | 0x80;
		*(pOutput + 4) = ((unic >> 6) & 0x3F) | 0x80;
		*(pOutput + 5) = (unic & 0x3F) | 0x80;
		return 6;
	}
	return 0;
}

int enc_get_utf8_size(const unsigned char pInput) {
	unsigned char c = pInput;
	// 0xxxxxxx 返回0   0x0
	// 10xxxxxx 不存在  0x80
	// 110xxxxx 返回2   0xC0
	// 1110xxxx 返回3   0xE0
	// 11110xxx 返回4   0xF0
	// 111110xx 返回5   0xF8
	// 1111110x 返回6   0xFC
	if (c < 0x80) return 1;
	if (c >= 0x80 && c < 0xC0) return -1;
	if (c >= 0xC0 && c < 0xE0) return 2;
	if (c >= 0xE0 && c < 0xF0) return 3;
	if (c >= 0xF0 && c < 0xF8) return 4;
	if (c >= 0xF8 && c < 0xFC) return 5;
	if (c >= 0xFC) return 6;
	return 1;
}
/*****************************************************************************
* 将一个字符的UTF8编码转换成Unicode(UCS-2和UCS-4)编码.
*
* 参数:
*    pInput      指向输入缓冲区, 以UTF-8编码
*    Unic        指向输出缓冲区, 其保存的数据即是Unicode编码值,
*                类型为ulong .
*
* 返回值:
*    成功则返回该字符的Unicode编码所占用的字节数; 失败则返回0.
*
* 注意:
*     1. UTF8没有字节序问题, 但是Unicode有字节序要求;
*        字节序分为大端(Big Endian)和小端(Little Endian)两种;
*        在Intel处理器中采用小端法表示, 在此采用小端法表示. (低地址存低位)
****************************************************************************/
int enc_utf8_to_unicode_one(const uchar* pInput, uchar *Unic) {
	assert(pInput != NULL && Unic != NULL);

	// b1 表示UTF-8编码的pInput中的高字节, b2 表示次高字节, ...
	char b1, b2, b3, b4, b5, b6;

	*Unic = 0x0; // 把 *Unic 初始化为全零
	int utfbytes = enc_get_utf8_size(*pInput);
	uchar *pOutput = (uchar *)Unic;

	switch (utfbytes) {
		case 1://1字节
			*pOutput = *pInput;
			break;
		case 2://2字节
			b1 = *pInput;
			b2 = *(pInput + 1);
			if ((b2 & 0xC0) != 0x80)
				return 0;
			*pOutput = (b1 << 6) + (b2 & 0x3F);
			*(pOutput + 1) = (b1 >> 2) & 0x07;
			break;
		case 3:
			b1 = *pInput;
			b2 = *(pInput + 1);
			b3 = *(pInput + 2);
			if (((b2 & 0xC0) != 0x80) || ((b3 & 0xC0) != 0x80))
				return 0;
			*pOutput = (b2 << 6) + (b3 & 0x3F);
			*(pOutput + 1) = (b1 << 4) + ((b2 >> 2) & 0x0F);
			break;
		case 4:
			b1 = *pInput;
			b2 = *(pInput + 1);
			b3 = *(pInput + 2);
			b4 = *(pInput + 3);
			if (((b2 & 0xC0) != 0x80) || ((b3 & 0xC0) != 0x80)
				|| ((b4 & 0xC0) != 0x80))
				return 0;
			*pOutput = (b3 << 6) + (b4 & 0x3F);
			*(pOutput + 1) = (b2 << 4) + ((b3 >> 2) & 0x0F);
			*(pOutput + 2) = ((b1 << 2) & 0x1C) + ((b2 >> 4) & 0x03);
			break;
		case 5:
			b1 = *pInput;
			b2 = *(pInput + 1);
			b3 = *(pInput + 2);
			b4 = *(pInput + 3);
			b5 = *(pInput + 4);
			if (((b2 & 0xC0) != 0x80) || ((b3 & 0xC0) != 0x80)
				|| ((b4 & 0xC0) != 0x80) || ((b5 & 0xC0) != 0x80))
				return 0;
			*pOutput = (b4 << 6) + (b5 & 0x3F);
			*(pOutput + 1) = (b3 << 4) + ((b4 >> 2) & 0x0F);
			*(pOutput + 2) = (b2 << 2) + ((b3 >> 4) & 0x03);
			*(pOutput + 3) = (b1 << 6);
			break;
		case 6:
			b1 = *pInput;
			b2 = *(pInput + 1);
			b3 = *(pInput + 2);
			b4 = *(pInput + 3);
			b5 = *(pInput + 4);
			b6 = *(pInput + 5);
			if (((b2 & 0xC0) != 0x80) || ((b3 & 0xC0) != 0x80)
				|| ((b4 & 0xC0) != 0x80) || ((b5 & 0xC0) != 0x80)
				|| ((b6 & 0xC0) != 0x80))
				return 0;
			*pOutput = (b5 << 6) + (b6 & 0x3F);
			*(pOutput + 1) = (b5 << 4) + ((b6 >> 2) & 0x0F);
			*(pOutput + 2) = (b3 << 2) + ((b4 >> 4) & 0x03);
			*(pOutput + 3) = ((b1 << 6) & 0x40) + (b2 & 0x3F);
			break;
		default:
			utfbytes = 0;
			break;
	}
	return utfbytes;
}

char* enc_u2u8(const char* data, uint* len) {
	size_t t, i;
	membuf_t buf;
	membuf_init(&buf, 128);
	(*len)--;
	for (i = 0; i <= *len; ) {
		if (buf.buffer_size - buf.size < 7)
			membuf_reserve(&buf, 7);
		t = enc_unicode_to_utf8_one(*(uint*)(data + i), (buf.data + buf.size), 7);
		if (t == 0) break;
		buf.size += t;
	}
	membuf_trunc(&buf);
	*len = buf.size;
	return (char*)buf.data;
}

char* enc_u82u(const char* data, uint* len) {
	size_t t, i;
	membuf_t buf;
	membuf_init(&buf, 128);
	for (i = 0; i < *len;) {
		if (buf.buffer_size - buf.size < 4)
			membuf_reserve(&buf, 4);
		t = enc_utf8_to_unicode_one((const uchar*)(data + i), (uchar*)(buf.data + buf.size));
		if (t == 0) break;
		buf.size += 2;
		i += t;
	}
	membuf_trunc(&buf);
	*len = buf.size;
	return (char*)buf.data;
}

#ifdef _MSC_VER
//GB2312 to unicode
wchar_t* GB2U(const char* pszGbs, uint* wLen) {
	*wLen = MultiByteToWideChar(CP_ACP, 0, pszGbs, -1, NULL, 0);
	wchar_t* wStr = (wchar_t*)malloc(*wLen * sizeof(wchar_t));
	MultiByteToWideChar(CP_ACP, 0, pszGbs, -1, wStr, *wLen);
	return wStr;
}
//unicode to utf8
char* U2U8(const wchar_t* wszUnicode, uint* aLen) {
	*aLen = WideCharToMultiByte(CP_UTF8, 0, (PWSTR)wszUnicode, -1, NULL, 0, NULL, NULL);
	char* szStr = (char*)malloc(*aLen * sizeof(char));
	WideCharToMultiByte(CP_UTF8, 0, (PWSTR)wszUnicode, -1, szStr, *aLen, NULL, NULL);
	return szStr;
}
//utf8 to unicode
wchar_t* U82U(const char* szU8, uint* wLen) {
	*wLen = MultiByteToWideChar(CP_UTF8, 0, szU8, -1, NULL, 0);
	wchar_t* wStr = (wchar_t*)malloc(*wLen * sizeof(wchar_t));
	MultiByteToWideChar(CP_UTF8, 0, szU8, -1, wStr, *wLen);
	return wStr;
}
//unicode to GB2312
char* U2GB(const wchar_t* wszUnicode, uint* aLen) {
	*aLen = WideCharToMultiByte(CP_ACP, 0, wszUnicode, -1, NULL, 0, NULL, NULL);
	char* szStr = (char*)malloc(*aLen * sizeof(char));
	WideCharToMultiByte(CP_ACP, 0, wszUnicode, -1, szStr, *aLen, NULL, NULL);
	return szStr;
}
//GB2312 to utf8
char* GB2U8(const char* pszGbs, uint* aLen) {
	*aLen = MultiByteToWideChar(CP_ACP, 0, pszGbs, -1, NULL, 0);
	wchar_t* wStr = (wchar_t*)malloc(*aLen * sizeof(wchar_t));
	MultiByteToWideChar(CP_ACP, 0, pszGbs, -1, wStr, *aLen);

	*aLen = WideCharToMultiByte(CP_UTF8, 0, (PWSTR)wStr, -1, NULL, 0, NULL, NULL);
	char* szStr = (char*)malloc(*aLen * sizeof(char));
	WideCharToMultiByte(CP_UTF8, 0, (PWSTR)wStr, -1, szStr, *aLen, NULL, NULL);
	free(wStr);
	return szStr;
}
//utf8 to GB2312
char* U82GB(const char* szU8, uint* aLen) {
	*aLen = MultiByteToWideChar(CP_UTF8, 0, szU8, -1, NULL, 0);
	wchar_t* wStr = (wchar_t*)malloc(*aLen * sizeof(wchar_t));
	MultiByteToWideChar(CP_UTF8, 0, szU8, -1, wStr, *aLen);

	*aLen = WideCharToMultiByte(CP_ACP, 0, wStr, -1, NULL, 0, NULL, NULL);
	char* szStr = (char*)malloc(*aLen * sizeof(char));
	WideCharToMultiByte(CP_ACP, 0, wStr, -1, szStr, *aLen, NULL, NULL);
	free(wStr);
	return szStr;
}

#else

//代码转换:从一种编码转为另一种编码
size_t code_convert(const char *from_charset, const char *to_charset, const char *inbuf, size_t inlen, char *outbuf, size_t* outlen) {
	iconv_t cd;
	size_t rc = 0, len = *outlen;
	char **pin = &inbuf;
	char **pout = &outbuf;

	cd = iconv_open(to_charset, from_charset);
	if (cd == 0)
		return -1;
	memset(outbuf, 0, len);
	if (iconv(cd, pin, (size_t*)&inlen, pout, (size_t*)&len) == -1)
		rc = -1;
	iconv_close(cd);
	*outlen -= len;//返回已用长度
	return rc;
}

//GB2312 to unicode(need free) 返回字串长度为:实际长度+1, 末尾\0站一字节（需要释放）
char* GB2U(const char* pszGbs, uint* aLen) {
	size_t len = *aLen * 4;
	char *outbuf = (char*)malloc(len + 1); outbuf[0] = 0;
	size_t rc = code_convert("gb2312", "unicode", pszGbs, *aLen, outbuf, &len);
	if (rc < 0) *aLen = rc;
	else *aLen = len + 1;
	return outbuf;
}
//unicode to utf8(need free) 返回字串长度为:实际长度+1, 末尾\0站一字节（需要释放）
char* U2U8(const char* wszUnicode, uint* aLen) {
	size_t len = *aLen;
	char *outbuf = (char*)malloc(len + 1); outbuf[0] = 0;
	size_t rc = code_convert("unicode", "utf-8", wszUnicode, *aLen, outbuf, &len);
	if (rc < 0) *aLen = rc;
	else *aLen = len + 1;
	return outbuf;
}
//utf8 to unicode(need free) 返回字串长度为:实际长度+1, 末尾\0站一字节（需要释放）
char* U82U(const char* szU8, uint* aLen) {
	size_t len = *aLen * 2;
	char *outbuf = (char*)malloc(len + 1); outbuf[0] = 0;
	size_t rc = code_convert("utf-8", "unicode", szU8, *aLen, outbuf, &len);
	if (rc < 0) *aLen = rc;
	else *aLen = len + 1;
	return outbuf;
}
//unicode to GB2312(need free) 返回字串（需要释放）长度为:实际长度+1, 末尾\0站一字节
char* U2GB(const char* wszUnicode, uint* aLen) {
	size_t len = *aLen;
	char *outbuf = (char*)malloc(len + 1); outbuf[0] = 0;
	size_t rc = code_convert("unicode", "gb2312", wszUnicode, *aLen, outbuf, &len);
	if (rc < 0) *aLen = rc;
	else *aLen = len + 1;
	return outbuf;
}

//GB2312 to utf8(need free) 返回字串（需要释放）长度为:实际长度+1, 末尾\0站一字节
char* GB2U8(const char* pszGbs, uint* aLen) {
	size_t len = *aLen * 3;
	char *outbuf = (char*)malloc(len + 1); outbuf[0] = 0;
	size_t rc = code_convert("gb2312", "utf-8", pszGbs, *aLen, outbuf, &len);
	if (rc < 0) *aLen = rc;
	else *aLen = len + 1;
	return outbuf;
}
//utf8 to GB2312(need free) 返回字串（需要释放）长度为:实际长度+1, 末尾\0站一字节
char* U82GB(const char* szU8, uint* aLen) {
	size_t len = *aLen;
	char *outbuf = (char*)malloc(len + 1); outbuf[0] = 0;
	size_t rc = code_convert("utf-8", "gb2312", szU8, *aLen, outbuf, &len);
	if (rc < 0) *aLen = rc;
	else *aLen = len + 1;
	return outbuf;
}

#endif

/***************************************************************************
* 函数名称： UTF8ToUCS2
* 功能描述： 转换UTF8格式到UCS2格式（UCS2是双字节编码，Unicode是其中一种）
* 日	期： 2008-05-22 13:36:56
* 作	者： lianxiuzhu
* 参数说明： binUTF8 - UTF8字节流数组
*			 uCount - UTF8字节流数组中的字节数
*			 binUCS2 - UCS2字节流数组
* 返 回 值： 转换到UCS2字节流数组中的U16单元个数
***************************************************************************/
size_t UTF8ToUCS2(const uchar* binUTF8, size_t uCount, ushort* binUCS2) {
	size_t uLength = 0;
	uchar* szTemp = (uchar*)binUTF8;
	while ((uint)(szTemp - binUTF8) < uCount) {
		if (*szTemp <= 0x7F) //0xxxxxxx
		{
			binUCS2[uLength] = binUCS2[uLength] | (ushort)(*szTemp & 0x7F);
			szTemp = szTemp + 1;
		}
		else if (*szTemp <= 0xDF) //110xxxxx 10xxxxxx
		{
			binUCS2[uLength] = binUCS2[uLength] | (ushort)(*(szTemp + 1) & 0x3F);
			binUCS2[uLength] = binUCS2[uLength] | ((ushort)(*(szTemp) & 0x1F) << 6);
			szTemp = szTemp + 2;
		}
		else if (*szTemp <= 0xEF) //1110xxxx 10xxxxxx 10xxxxxx
		{
			binUCS2[uLength] = binUCS2[uLength] | (ushort)(*(szTemp + 2) & 0x3F);
			binUCS2[uLength] = binUCS2[uLength] | ((ushort)(*(szTemp + 1) & 0x3F) << 6);
			binUCS2[uLength] = binUCS2[uLength] | ((ushort)(*(szTemp) & 0x0F) << 12);
			szTemp = szTemp + 3;
		}
		else {
			return 0;
		}
		uLength++;
	}
	return uLength;
}

#pragma endregion

//-----------------------------------------------------------------------------------url编码解码  win/linux
#pragma region url编码解码

//url编码 (len为buf的长度)
char* url_encode(const char *url, uint* len) {
	if (!url)
		return NULL;
	membuf_t buf;
	const char *p;
	const char urlunsafe[] = "\r\n \"#%&+:;<=>?@[\\]^`{|}";
	const char hex[] = "0123456789ABCDEF";
	char enc[3] = { '%',0,0 };
	len--;
	membuf_init(&buf, strlen(url) + 1);
	for (p = url; *p; p++) {
		if ((p - url) > *len)
			break;
		if (*p < ' ' || *p > '~' || strchr(urlunsafe, *p)) {
			enc[1] = hex[*p >> 4];
			enc[2] = hex[*p & 0x0f];
			membuf_append_data(&buf, enc, 3);
		}
		else {
			membuf_append_data(&buf, p, 1);
		}
	}
	membuf_trunc(&buf);
	*len = buf.size;
	return (char*)buf.data;
}

//url解码
char* url_decode(char *url) {
	char *o, *s;
	uint tmp;

	for (o = s = url; *s; s++, o++) {
		if (*s == '%' && strlen(s) > 2 && sscanf(s + 1, "%2x", &tmp) == 1) {
			*o = (char)tmp;
			s += 2;
		}
		else {
			*o = *s;
		}
	}
	*o = '\0';
	return url;
}

#pragma endregion

//-----------------------------------------------------------------------------------Base64编码解码  win/linux
#pragma region Base64编码解码

char base64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
char base64_end = '=';

#define is_base64(c)  (isalnum(c) || (c == '+') || (c == '/'))

//Base64编码,需要释放返回值(need free return)
char* base64_Encode(const uchar* bytes_to_encode, uint in_len) {
	membuf_t ret;
	int i = 0, j = 0;
	uchar char_array_3[3];
	uchar char_array_4[4];

	membuf_init(&ret, in_len * 3);//初始化缓存字节数为 长度的3被

	while (in_len--) {
		char_array_3[i++] = *(bytes_to_encode++);
		if (i == 3) {
			char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
			char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
			char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
			char_array_4[3] = char_array_3[2] & 0x3f;

			for (i = 0; (i < 4); i++)
				membuf_append_data(&ret, &base64_table[char_array_4[i]], 1);
			i = 0;
		}
	}

	if (i) {
		for (j = i; j < 3; j++)
			char_array_3[j] = '\0';

		char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
		char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
		char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
		char_array_4[3] = char_array_3[2] & 0x3f;

		for (j = 0; (j < i + 1); j++)
			membuf_append_data(&ret, &base64_table[char_array_4[j]], 1);

		while ((i++ < 3))
			membuf_append_data(&ret, &base64_end, 1);
	}
	return (char*)ret.data;
}

//Base64解码,需要释放返回值(need free return)
char* base64_Decode(const char* encoded_string) {
	size_t in_len = strlen(encoded_string);
	int i = 0;
	int j = 0;
	size_t in_ = 0;
	uchar char_array_4[4], char_array_3[3];
	membuf_t ret;
	membuf_init(&ret, strlen(encoded_string) / 3 + 1);

	while (in_len-- && (encoded_string[in_] != '=') && is_base64(encoded_string[in_])) {
		char_array_4[i++] = encoded_string[in_]; in_++;
		if (i == 4) {
			for (i = 0; i < 4; i++)
				//char_array_4[i] = strstr(base64_table,(char*)&char_array_4[i])[0];
				char_array_4[i] = strchr(base64_table, char_array_4[i]) - base64_table;

			char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
			char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
			char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

			for (i = 0; (i < 3); i++)
				membuf_append_data(&ret, &char_array_3[i], 1);
			i = 0;
		}
	}

	if (i) {
		for (j = i; j < 4; j++)
			char_array_4[j] = 0;

		for (j = 0; j < 4; j++)
			//char_array_4[j] = strstr(base64_table, (char*)&char_array_4[j])[0];
			char_array_4[j] = strchr(base64_table, char_array_4[j]) - base64_table;

		char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
		char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
		char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

		for (j = 0; (j < i - 1); j++)
			membuf_append_data(&ret, &char_array_3[j], 1);
	}
	return (char*)ret.data;
}

#pragma endregion

//-----------------------------------------------------------------------------------MD5计算摘要  win/unix
#pragma region MD5计算摘要

/*
* The basic MD5 functions.
*
* F and G are optimized compared to their RFC 1321 definitions for
* architectures that lack an AND-NOT instruction, just like in Colin Plumb's
* implementation.
*/
#define MD5_F(x, y, z)			((z) ^ ((x) & ((y) ^ (z))))
#define MD5_G(x, y, z)			((y) ^ ((z) & ((x) ^ (y))))
#define MD5_H(x, y, z)			(((x) ^ (y)) ^ (z))
#define MD5_H2(x, y, z)			((x) ^ ((y) ^ (z)))
#define MD5_I(x, y, z)			((y) ^ ((x) | ~(z)))
/*
* The MD5 transformation for all four rounds.
*/
#define MD5_STEP(f, a, b, c, d, x, t, s) \
	(a) += f((b), (c), (d)) + (x) + (t); \
	(a) = (((a) << (s)) | (((a) & 0xffffffff) >> (32 - (s)))); \
	(a) += (b)
/*
* SET reads 4 input bytes in little-endian byte order and stores them in a
* properly aligned word in host byte order.
*
* The check for little-endian architectures that tolerate unaligned memory
* accesses is just an optimization.  Nothing will break if it fails to detect
* a suitable architecture.
*
* Unfortunately, this optimization may be a C strict aliasing rules violation
* if the caller's data buffer has effective type that cannot be aliased by
* uint.  In practice, this problem may occur if these MD5 routines are
* inlined into a calling function, or with future and dangerously advanced
* link-time optimizations.  For the time being, keeping these MD5 routines in
* their own translation unit avoids the problem.
*/
#if defined(__i386__) || defined(__x86_64__) || defined(__vax__)
#define MD5_SET(n) \
	(*(uint *)&ptr[(n) * 4])
#define MD5_GET(n) \
	(ctx->block[(n)])/*SET(n)*/
#else
#define MD5_SET(n) \
	(ctx->block[(n)] = \
	(uint)ptr[(n) * 4] | \
	((uint)ptr[(n) * 4 + 1] << 8) | \
	((uint)ptr[(n) * 4 + 2] << 16) | \
	((uint)ptr[(n) * 4 + 3] << 24))
#define MD5_GET(n) \
	(ctx->block[(n)])
#endif

#define MD5_OUT(dst, src) \
	(dst)[0] = (uchar)(src); \
	(dst)[1] = (uchar)((src) >> 8); \
	(dst)[2] = (uchar)((src) >> 16); \
	(dst)[3] = (uchar)((src) >> 24)

// This processes one or more 64-byte data blocks, but does NOT update the bit counters.  There are no alignment requirements.
static const uchar* md5_body(MD5_CONTEXT* ctx, const uchar* data, ulong size) {
	const uchar *ptr;
	uint a, b, c, d;
	uint saved_a, saved_b, saved_c, saved_d;

	ptr = data;

	a = ctx->a;
	b = ctx->b;
	c = ctx->c;
	d = ctx->d;

	do {
		saved_a = a;
		saved_b = b;
		saved_c = c;
		saved_d = d;

		/* Round 1 */
		MD5_STEP(MD5_F, a, b, c, d, MD5_SET(0), 0xd76aa478, 7);
		MD5_STEP(MD5_F, d, a, b, c, MD5_SET(1), 0xe8c7b756, 12);
		MD5_STEP(MD5_F, c, d, a, b, MD5_SET(2), 0x242070db, 17);
		MD5_STEP(MD5_F, b, c, d, a, MD5_SET(3), 0xc1bdceee, 22);
		MD5_STEP(MD5_F, a, b, c, d, MD5_SET(4), 0xf57c0faf, 7);
		MD5_STEP(MD5_F, d, a, b, c, MD5_SET(5), 0x4787c62a, 12);
		MD5_STEP(MD5_F, c, d, a, b, MD5_SET(6), 0xa8304613, 17);
		MD5_STEP(MD5_F, b, c, d, a, MD5_SET(7), 0xfd469501, 22);
		MD5_STEP(MD5_F, a, b, c, d, MD5_SET(8), 0x698098d8, 7);
		MD5_STEP(MD5_F, d, a, b, c, MD5_SET(9), 0x8b44f7af, 12);
		MD5_STEP(MD5_F, c, d, a, b, MD5_SET(10), 0xffff5bb1, 17);
		MD5_STEP(MD5_F, b, c, d, a, MD5_SET(11), 0x895cd7be, 22);
		MD5_STEP(MD5_F, a, b, c, d, MD5_SET(12), 0x6b901122, 7);
		MD5_STEP(MD5_F, d, a, b, c, MD5_SET(13), 0xfd987193, 12);
		MD5_STEP(MD5_F, c, d, a, b, MD5_SET(14), 0xa679438e, 17);
		MD5_STEP(MD5_F, b, c, d, a, MD5_SET(15), 0x49b40821, 22);

		/* Round 2 */
		MD5_STEP(MD5_G, a, b, c, d, MD5_GET(1), 0xf61e2562, 5);
		MD5_STEP(MD5_G, d, a, b, c, MD5_GET(6), 0xc040b340, 9);
		MD5_STEP(MD5_G, c, d, a, b, MD5_GET(11), 0x265e5a51, 14);
		MD5_STEP(MD5_G, b, c, d, a, MD5_GET(0), 0xe9b6c7aa, 20);
		MD5_STEP(MD5_G, a, b, c, d, MD5_GET(5), 0xd62f105d, 5);
		MD5_STEP(MD5_G, d, a, b, c, MD5_GET(10), 0x02441453, 9);
		MD5_STEP(MD5_G, c, d, a, b, MD5_GET(15), 0xd8a1e681, 14);
		MD5_STEP(MD5_G, b, c, d, a, MD5_GET(4), 0xe7d3fbc8, 20);
		MD5_STEP(MD5_G, a, b, c, d, MD5_GET(9), 0x21e1cde6, 5);
		MD5_STEP(MD5_G, d, a, b, c, MD5_GET(14), 0xc33707d6, 9);
		MD5_STEP(MD5_G, c, d, a, b, MD5_GET(3), 0xf4d50d87, 14);
		MD5_STEP(MD5_G, b, c, d, a, MD5_GET(8), 0x455a14ed, 20);
		MD5_STEP(MD5_G, a, b, c, d, MD5_GET(13), 0xa9e3e905, 5);
		MD5_STEP(MD5_G, d, a, b, c, MD5_GET(2), 0xfcefa3f8, 9);
		MD5_STEP(MD5_G, c, d, a, b, MD5_GET(7), 0x676f02d9, 14);
		MD5_STEP(MD5_G, b, c, d, a, MD5_GET(12), 0x8d2a4c8a, 20);

		/* Round 3 */
		MD5_STEP(MD5_H, a, b, c, d, MD5_GET(5), 0xfffa3942, 4);
		MD5_STEP(MD5_H2, d, a, b, c, MD5_GET(8), 0x8771f681, 11);
		MD5_STEP(MD5_H, c, d, a, b, MD5_GET(11), 0x6d9d6122, 16);
		MD5_STEP(MD5_H2, b, c, d, a, MD5_GET(14), 0xfde5380c, 23);
		MD5_STEP(MD5_H, a, b, c, d, MD5_GET(1), 0xa4beea44, 4);
		MD5_STEP(MD5_H2, d, a, b, c, MD5_GET(4), 0x4bdecfa9, 11);
		MD5_STEP(MD5_H, c, d, a, b, MD5_GET(7), 0xf6bb4b60, 16);
		MD5_STEP(MD5_H2, b, c, d, a, MD5_GET(10), 0xbebfbc70, 23);
		MD5_STEP(MD5_H, a, b, c, d, MD5_GET(13), 0x289b7ec6, 4);
		MD5_STEP(MD5_H2, d, a, b, c, MD5_GET(0), 0xeaa127fa, 11);
		MD5_STEP(MD5_H, c, d, a, b, MD5_GET(3), 0xd4ef3085, 16);
		MD5_STEP(MD5_H2, b, c, d, a, MD5_GET(6), 0x04881d05, 23);
		MD5_STEP(MD5_H, a, b, c, d, MD5_GET(9), 0xd9d4d039, 4);
		MD5_STEP(MD5_H2, d, a, b, c, MD5_GET(12), 0xe6db99e5, 11);
		MD5_STEP(MD5_H, c, d, a, b, MD5_GET(15), 0x1fa27cf8, 16);
		MD5_STEP(MD5_H2, b, c, d, a, MD5_GET(2), 0xc4ac5665, 23);

		/* Round 4 */
		MD5_STEP(MD5_I, a, b, c, d, MD5_GET(0), 0xf4292244, 6);
		MD5_STEP(MD5_I, d, a, b, c, MD5_GET(7), 0x432aff97, 10);
		MD5_STEP(MD5_I, c, d, a, b, MD5_GET(14), 0xab9423a7, 15);
		MD5_STEP(MD5_I, b, c, d, a, MD5_GET(5), 0xfc93a039, 21);
		MD5_STEP(MD5_I, a, b, c, d, MD5_GET(12), 0x655b59c3, 6);
		MD5_STEP(MD5_I, d, a, b, c, MD5_GET(3), 0x8f0ccc92, 10);
		MD5_STEP(MD5_I, c, d, a, b, MD5_GET(10), 0xffeff47d, 15);
		MD5_STEP(MD5_I, b, c, d, a, MD5_GET(1), 0x85845dd1, 21);
		MD5_STEP(MD5_I, a, b, c, d, MD5_GET(8), 0x6fa87e4f, 6);
		MD5_STEP(MD5_I, d, a, b, c, MD5_GET(15), 0xfe2ce6e0, 10);
		MD5_STEP(MD5_I, c, d, a, b, MD5_GET(6), 0xa3014314, 15);
		MD5_STEP(MD5_I, b, c, d, a, MD5_GET(13), 0x4e0811a1, 21);
		MD5_STEP(MD5_I, a, b, c, d, MD5_GET(4), 0xf7537e82, 6);
		MD5_STEP(MD5_I, d, a, b, c, MD5_GET(11), 0xbd3af235, 10);
		MD5_STEP(MD5_I, c, d, a, b, MD5_GET(2), 0x2ad7d2bb, 15);
		MD5_STEP(MD5_I, b, c, d, a, MD5_GET(9), 0xeb86d391, 21);

		a += saved_a;
		b += saved_b;
		c += saved_c;
		d += saved_d;

		ptr += 64;
	} while (size -= 64);

	ctx->a = a;
	ctx->b = b;
	ctx->c = c;
	ctx->d = d;

	return ptr;
}
//初始化 结构体
void md5_init(MD5_CONTEXT *ctx) {
	ctx->a = 0x67452301;
	ctx->b = 0xefcdab89;
	ctx->c = 0x98badcfe;
	ctx->d = 0x10325476;

	ctx->lo = 0;
	ctx->hi = 0;
}
//使用长度为 len 的 data 内容更新消息摘要
void md5_update(MD5_CONTEXT* ctx, const uchar* data, ulong len) {
	uint saved_lo;
	ulong used, available;

	saved_lo = ctx->lo;
	if ((ctx->lo = (saved_lo + len) & 0x1fffffff) < saved_lo)
		ctx->hi++;
	ctx->hi += len >> 29;

	used = saved_lo & 0x3f;

	if (used) {
		available = 64 - used;

		if (len < available) {
			memcpy(&ctx->buffer[used], data, len);
			return;
		}

		memcpy(&ctx->buffer[used], data, available);
		data = (const uchar *)data + available;
		len -= available;
		md5_body(ctx, ctx->buffer, 64);
	}

	if (len >= 64) {
		data = md5_body(ctx, data, len & ~(ulong)0x3f);
		len &= 0x3f;
	}

	memcpy(ctx->buffer, data, len);
}
//结束计算并返回摘要, dst 长度不小于16字节
void md5_final(MD5_CONTEXT* ctx, uchar* dst) {
	unsigned long used, available;

	used = ctx->lo & 0x3f;

	ctx->buffer[used++] = 0x80;

	available = 64 - used;

	if (available < 8) {
		memset(&ctx->buffer[used], 0, available);
		md5_body(ctx, ctx->buffer, 64);
		used = 0;
		available = 64;
	}

	memset(&ctx->buffer[used], 0, available - 8);

	ctx->lo <<= 3;
	MD5_OUT(&ctx->buffer[56], ctx->lo);
	MD5_OUT(&ctx->buffer[60], ctx->hi);

	md5_body(ctx, ctx->buffer, 64);

	MD5_OUT(&dst[0], ctx->a);
	MD5_OUT(&dst[4], ctx->b);
	MD5_OUT(&dst[8], ctx->c);
	MD5_OUT(&dst[12], ctx->d);

	memset(ctx, 0, sizeof(*ctx));
}
//直接计算src的摘要, dst 长度不小于16字节
void md5_sum(uchar* dst, const uchar* src, size_t len) {
	MD5_CONTEXT ctx;
	md5_init(&ctx);
	md5_update(&ctx, src, len);
	md5_final(&ctx, dst);
}
//转换摘要为字符串, dst 长度16字节
char* md5_print(const uchar* dst, char *buf) {
	int i;
	for (i = 0; i < 16; i++) {
		sprintf(buf, "%02X", dst[i]);
		buf += 2;
	}
	return buf;
}

#pragma endregion

//-----------------------------------------------------------------------------------Hash1计算摘要  win/linux
#pragma region Hash1计算摘要

/****************
* Rotate a 32 bit integer by n bytes
****************/
#if defined(__GNUC__) && defined(__i386__)
static inline u32 rol(u32 x, int n) {
	__asm__("roll %%cl,%0"
			:"=r" (x)
			: "0" (x), "c" (n));
	return x;
}
#else
#define rol(x,n) ( ((x) << (n)) | ((x) >> (32-(n))) )
#endif

void hash1_Reset(SHA1_CONTEXT* hd) {
	hd->bFinal = 0;
	hd->h0 = 0x67452301;
	hd->h1 = 0xefcdab89;
	hd->h2 = 0x98badcfe;
	hd->h3 = 0x10325476;
	hd->h4 = 0xc3d2e1f0;
	hd->nblocks = 0;
	hd->count = 0;
	memset(hd->buf, 0, 64);
}

/*
* Transform the message X which consists of 16 32-bit-words
*/
static void hash1_transform(SHA1_CONTEXT* hd, uchar *data) {
	uint a, b, c, d, e, tm;
	uint x[16];

	/* get values from the chaining vars */
	a = hd->h0;
	b = hd->h1;
	c = hd->h2;
	d = hd->h3;
	e = hd->h4;

#ifdef BIG_ENDIAN_HOST
	memcpy(x, data, 64);
#else
	{
		int i;
		uchar *p2;
		for (i = 0, p2 = (uchar*)x; i < 16; i++, p2 += 4) {
			p2[3] = *data++;
			p2[2] = *data++;
			p2[1] = *data++;
			p2[0] = *data++;
		}
	}
#endif

#define K1  0x5A827999L
#define K2  0x6ED9EBA1L
#define K3  0x8F1BBCDCL
#define K4  0xCA62C1D6L
#define F1(x,y,z)   ( z ^ ( x & ( y ^ z ) ) )
#define F2(x,y,z)   ( x ^ y ^ z )
#define F3(x,y,z)   ( ( x & y ) | ( z & ( x | y ) ) )
#define F4(x,y,z)   ( x ^ y ^ z )

#define M(i) ( tm = x[i&0x0f] ^ x[(i-14)&0x0f]    \
               ^ x[(i-8)&0x0f] ^ x[(i-3)&0x0f]    \
               , (x[i&0x0f] = rol(tm,1)) )

#define R(a,b,c,d,e,f,k,m)  do { e += rol( a, 5 ) \
            + f( b, c, d )                        \
            + k                                   \
            + m;                                  \
        b = rol( b, 30 );                         \
    } while(0)

	R(a, b, c, d, e, F1, K1, x[0]);
	R(e, a, b, c, d, F1, K1, x[1]);
	R(d, e, a, b, c, F1, K1, x[2]);
	R(c, d, e, a, b, F1, K1, x[3]);
	R(b, c, d, e, a, F1, K1, x[4]);
	R(a, b, c, d, e, F1, K1, x[5]);
	R(e, a, b, c, d, F1, K1, x[6]);
	R(d, e, a, b, c, F1, K1, x[7]);
	R(c, d, e, a, b, F1, K1, x[8]);
	R(b, c, d, e, a, F1, K1, x[9]);
	R(a, b, c, d, e, F1, K1, x[10]);
	R(e, a, b, c, d, F1, K1, x[11]);
	R(d, e, a, b, c, F1, K1, x[12]);
	R(c, d, e, a, b, F1, K1, x[13]);
	R(b, c, d, e, a, F1, K1, x[14]);
	R(a, b, c, d, e, F1, K1, x[15]);
	R(e, a, b, c, d, F1, K1, M(16));
	R(d, e, a, b, c, F1, K1, M(17));
	R(c, d, e, a, b, F1, K1, M(18));
	R(b, c, d, e, a, F1, K1, M(19));
	R(a, b, c, d, e, F2, K2, M(20));
	R(e, a, b, c, d, F2, K2, M(21));
	R(d, e, a, b, c, F2, K2, M(22));
	R(c, d, e, a, b, F2, K2, M(23));
	R(b, c, d, e, a, F2, K2, M(24));
	R(a, b, c, d, e, F2, K2, M(25));
	R(e, a, b, c, d, F2, K2, M(26));
	R(d, e, a, b, c, F2, K2, M(27));
	R(c, d, e, a, b, F2, K2, M(28));
	R(b, c, d, e, a, F2, K2, M(29));
	R(a, b, c, d, e, F2, K2, M(30));
	R(e, a, b, c, d, F2, K2, M(31));
	R(d, e, a, b, c, F2, K2, M(32));
	R(c, d, e, a, b, F2, K2, M(33));
	R(b, c, d, e, a, F2, K2, M(34));
	R(a, b, c, d, e, F2, K2, M(35));
	R(e, a, b, c, d, F2, K2, M(36));
	R(d, e, a, b, c, F2, K2, M(37));
	R(c, d, e, a, b, F2, K2, M(38));
	R(b, c, d, e, a, F2, K2, M(39));
	R(a, b, c, d, e, F3, K3, M(40));
	R(e, a, b, c, d, F3, K3, M(41));
	R(d, e, a, b, c, F3, K3, M(42));
	R(c, d, e, a, b, F3, K3, M(43));
	R(b, c, d, e, a, F3, K3, M(44));
	R(a, b, c, d, e, F3, K3, M(45));
	R(e, a, b, c, d, F3, K3, M(46));
	R(d, e, a, b, c, F3, K3, M(47));
	R(c, d, e, a, b, F3, K3, M(48));
	R(b, c, d, e, a, F3, K3, M(49));
	R(a, b, c, d, e, F3, K3, M(50));
	R(e, a, b, c, d, F3, K3, M(51));
	R(d, e, a, b, c, F3, K3, M(52));
	R(c, d, e, a, b, F3, K3, M(53));
	R(b, c, d, e, a, F3, K3, M(54));
	R(a, b, c, d, e, F3, K3, M(55));
	R(e, a, b, c, d, F3, K3, M(56));
	R(d, e, a, b, c, F3, K3, M(57));
	R(c, d, e, a, b, F3, K3, M(58));
	R(b, c, d, e, a, F3, K3, M(59));
	R(a, b, c, d, e, F4, K4, M(60));
	R(e, a, b, c, d, F4, K4, M(61));
	R(d, e, a, b, c, F4, K4, M(62));
	R(c, d, e, a, b, F4, K4, M(63));
	R(b, c, d, e, a, F4, K4, M(64));
	R(a, b, c, d, e, F4, K4, M(65));
	R(e, a, b, c, d, F4, K4, M(66));
	R(d, e, a, b, c, F4, K4, M(67));
	R(c, d, e, a, b, F4, K4, M(68));
	R(b, c, d, e, a, F4, K4, M(69));
	R(a, b, c, d, e, F4, K4, M(70));
	R(e, a, b, c, d, F4, K4, M(71));
	R(d, e, a, b, c, F4, K4, M(72));
	R(c, d, e, a, b, F4, K4, M(73));
	R(b, c, d, e, a, F4, K4, M(74));
	R(a, b, c, d, e, F4, K4, M(75));
	R(e, a, b, c, d, F4, K4, M(76));
	R(d, e, a, b, c, F4, K4, M(77));
	R(c, d, e, a, b, F4, K4, M(78));
	R(b, c, d, e, a, F4, K4, M(79));

	/* Update chaining vars */
	hd->h0 += a;
	hd->h1 += b;
	hd->h2 += c;
	hd->h3 += d;
	hd->h4 += e;
}

// Update the message digest with the contents of INBUF with length INLEN.
void hash1_Write(SHA1_CONTEXT* hd, uchar *inbuf, size_t inlen) {
	if (hd->bFinal)
		hash1_Reset(hd);
	if (hd->count == 64) { /* flush the buffer */
		hash1_transform(hd, hd->buf);
		hd->count = 0;
		hd->nblocks++;
	}
	if (!inbuf)
		return;
	if (hd->count) {
		for (; inlen && hd->count < 64; inlen--)
			hd->buf[hd->count++] = *inbuf++;
		hash1_Write(hd, NULL, 0);
		if (!inlen)
			return;
	}

	while (inlen >= 64) {
		hash1_transform(hd, inbuf);
		hd->count = 0;
		hd->nblocks++;
		inlen -= 64;
		inbuf += 64;
	}
	for (; inlen && hd->count < 64; inlen--)
		hd->buf[hd->count++] = *inbuf++;
}

/* The routine final terminates the computation and returns the digest.
* The handle is prepared for a new cycle, but adding bytes to the
* handle will the destroy the returned buffer.
* Returns: 20 bytes representing the digest.
*/
void hash1_Final(SHA1_CONTEXT* hd) {
	uint t, msb, lsb;
	uchar *p;

	hash1_Write(hd, NULL, 0); /* flush */;

	t = hd->nblocks;
	/* multiply by 64 to make a byte count */
	lsb = t << 6;
	msb = t >> 26;
	/* add the count */
	t = lsb;
	if ((lsb += hd->count) < t)
		msb++;
	/* multiply by 8 to make a bit count */
	t = lsb;
	lsb <<= 3;
	msb <<= 3;
	msb |= t >> 29;

	if (hd->count < 56) { /* enough room */
		hd->buf[hd->count++] = 0x80; /* pad */
		while (hd->count < 56)
			hd->buf[hd->count++] = 0;  /* pad */
	}
	else { /* need one extra block */
		hd->buf[hd->count++] = 0x80; /* pad character */
		while (hd->count < 64)
			hd->buf[hd->count++] = 0;
		hash1_Write(hd, NULL, 0);  /* flush */;
		memset(hd->buf, 0, 56); /* fill next block with zeroes */
	}
	/* append the 64 bit count */
	hd->buf[56] = msb >> 24;
	hd->buf[57] = msb >> 16;
	hd->buf[58] = msb >> 8;
	hd->buf[59] = msb;
	hd->buf[60] = lsb >> 24;
	hd->buf[61] = lsb >> 16;
	hd->buf[62] = lsb >> 8;
	hd->buf[63] = lsb;
	hash1_transform(hd, hd->buf);

	p = hd->buf;
#ifdef BIG_ENDIAN_HOST
#define X(a) do { *(u32*)p = hd->h##a ; p += 4; } while(0)
#else /* little endian */
#define X(a) do { *p++ = hd->h##a >> 24; *p++ = hd->h##a >> 16; \
        *p++ = hd->h##a >> 8; *p++ = hd->h##a; } while(0)
#endif
	X(0);
	X(1);
	X(2);
	X(3);
	X(4);
#undef X
	//Hash1 operation finally
	hd->bFinal = 1;
}

#pragma endregion

//-----------------------------------------------------------------------------------WebSocket  win/linux
#pragma region WebSocket Tool

//初始化 WebSocketHandle
void WebSocketHandleInit(WebSocketHandle* handle) {
	handle->bFinal = 0;
	handle->bFrame = 1;
	handle->bHead = 0;
	handle->bMask = 0;
	handle->opCode = 0;
	handle->extCode = 0;
	handle->len = 0;
	memset(handle->head, 0, sizeof(handle->head));
	memset(handle->mask, 0, sizeof(handle->mask));
}

//WebSocket握手Key计算
char* WebSocketHandShak(const char* key) {
	char akey[140] = { 0 };
	char acc[170] = { 0 }, *p;
	int len;
	SHA1_CONTEXT hd;

	strncpy(akey, key, 99);
	strncpy(akey + strlen(key), "258EAFA5-E914-47DA-95CA-C5AB0DC85B11", 40);

	hash1_Reset(&hd);
	hash1_Write(&hd, (uchar*)akey, strlen(akey));
	hash1_Final(&hd);

	p = base64_Encode(hd.buf, 20);
	len = snprintf(acc, 169, "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %s\r\n\r\n", p);
	free(p);

	p = (char*)malloc(len + 1);
	memcpy(p, acc, len);
	p[len] = 0;
	return p;
}

void WebSocketDoMask(WebSocketHandle* handle) {
	if (handle->bMask && handle->bFrame) {
		ulong i = 0;
		for (i = 0; i < handle->buf.size; i++)
			handle->buf.data[i] = handle->buf.data[i] ^ handle->mask[i % 4];
	}
}

//接收帧头部,并把剩下数据位置放到buf中
//返回是否已经接收完头部
void WebSocketGetFrameHead(WebSocketHandle* handle, char* data, ulong len) {
	if (!data || data[0] == 0 || len == 0 || handle->bHead)
		return;

	if (NULL == handle->buf.data)
		membuf_init(&handle->buf, 128);
	int l = (int)strlen_x(handle->head, sizeof(handle->head));
	int left = sizeof(handle->head) - l, use = 0;
	if (left > len)
		left = len;
	if (left > 0) {
		memcpy(handle->head + l, data, left);
	}
	l = (int)strlen_x(handle->head, sizeof(handle->head));//收集的 frame 头部长度;
	if (l > 0) {
		handle->bFinal = ((uchar)handle->head[0] >> 7);//是否结束
		handle->extCode = ((uchar)handle->head[0] & 0x70);//扩展码
		handle->opCode = (uchar)handle->head[0] & 0xF;//OPCode
		if (l > 1)
			handle->bMask = ((uchar)handle->head[1] >> 7);
		if (l > 2) {
			ulong dlen = (uchar)data[1] & 0x7f;//Payload长度
			if (dlen < 126) { //如果其值在0-125，则是payload的真实长度(ApplicationData长度,ExtensionData长度为0)
				handle->len = dlen;
				if (handle->bMask) {
					if (l > 5) {//防止头部不够长度的错误
						memcpy(handle->mask, handle->head + 2, 4);
						handle->bHead = 1;
						use = 6;
					}
				}
				else { //没用掩码
					handle->bHead = 1;
					use = 2;
				}
			}
			else if (dlen == 126 && l > 3) { //如果值是126，则后面2个字节形成的16位无符号整型数(ushort)的值是payload的真实长度，掩码就紧更着后面
				handle->len = handle->head[2] * 0x100UL + (uchar)handle->head[3];//逐字节转换
				if (handle->bMask) {
					if (l > 7) {//防止头部不够长度的错误
						memcpy(handle->mask, handle->head + 4, 4);
						handle->bHead = 1;
						use = 8;
					}
				}
				else { //没用掩码
					handle->bHead = 1;
					use = 4;
				}
			}
			else if (l > 10) { //如果值是127，则后面8个字节形成的64位无符号整型数(uint64)的值是payload的真实长度，掩码就紧更着后面
				handle->len = handle->head[6] * 0x1000000ULL + handle->head[7] * 0x10000ULL + handle->head[8] * 0x100ULL + (uchar)handle->head[9];//逐字节转换为ulong
				if (handle->bMask) {
					if (l > 13) {//防止头部不够长度的错误
						memcpy(handle->mask, handle->head + 10, 4);
						handle->bHead = 1;
						use = 14;
					}
				}
				else { //没用掩码
					handle->bHead = 1;
					use = 10;
				}
			}
			if (handle->bHead) {
				//剩余的是数据,放到 buf
				if (use < l) {
					membuf_append_data(&handle->buf, handle->head + use, l - use);
				}
				if (left < len) {
					membuf_append_data(&handle->buf, data + left, len - left);
				}
				handle->bFrame = (handle->len == handle->buf.size);
				WebSocketDoMask(handle);
			}
		}
	}
}

//从帧中取得实际数据
uchar WebSocketGetFrame(WebSocketHandle* handle, char* data, ulong len) {
	if (!handle || !data || len < 1)
		return 0;
	membuf_t* buf = &handle->buf;
	if (handle->bHead == 0) {
		WebSocketGetFrameHead(handle, data, len);
	}
	else if (handle->bFrame == 0) {
		membuf_append_data(buf, data, len);
		handle->bFrame = (handle->len == handle->buf.size);
		WebSocketDoMask(handle);
	}
	return handle->bFrame;
}

//转换为一个WebSocket帧,无mask (need free return)
char* WebSocketMakeFrame(const char* data, ulong* dlen, uchar op) {
	membuf_t buf;
	if (*dlen < 126) {
		membuf_init(&buf, *dlen + 2);
		buf.size = 2;
		//数据长度
		buf.data[1] = (uchar)*dlen;
	}
	else if (*dlen < 65536) {
		membuf_init(&buf, *dlen + 4);
		buf.size = 4;
		buf.data[1] = 0x7E;
		//数据长度
		buf.data[2] = (*dlen >> 8) & 255;
		buf.data[3] = (*dlen) & 255;
	}
	else {
		membuf_init(&buf, *dlen + 10);
		buf.size = 10;
		buf.data[1] = 0x7F;
		//数据长度,64位数据大小
		buf.data[2] = (*dlen >> 56) & 255;
		buf.data[3] = (*dlen >> 48) & 255;
		buf.data[4] = (*dlen >> 40) & 255;
		buf.data[5] = (*dlen >> 32) & 255;
		buf.data[6] = (*dlen >> 24) & 255;
		buf.data[7] = (*dlen >> 16) & 255;
		buf.data[8] = (*dlen >> 8) & 255;
		buf.data[9] = (*dlen) & 255;
	}
	//第一byte,10000000, fin = 1, rsv1 rsv2 rsv3均为0, opcode = 0x01,即数据为文本帧
	buf.data[0] = 0x80 + op;//0x81 最后一个包 |(无扩展协议)| 控制码(0x1表示文本帧)
	if (data != NULL && *dlen > 0)
		membuf_append_data(&buf, data, *dlen);
	*dlen = buf.size;
	//membuf_trunc(&buf);
	return (char*)buf.data;
}

#pragma endregion

//-----------------------------------------------------------------------------------工具/杂项  win/linux
#pragma region 工具/杂项

inline int day_of_year(int y, int m, int d) {
	int k, leap, s;
	int days[13] = { 0,31,28,31,30,31,30,31,31,30,31,30,31 };
	leap = (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
	s = d;
	for (k = 1; k < m; k++) {
		s += days[k];
	}
	if (leap == 1 && m > 2)
		s += 1;
	return s;
}

//获取格林制（GMT）时间: "Wed, 18 Jul 2018 06:02:42 GMT"
//szDate: 存放GMT时间的缓存区(至少 char[30])，外部传入
//szLen: szDate的长度大小
//addSecond: 当前时间加上多少秒
char* getGmtTime(char* szDate, int szLen, int addSecond) {
	time_t rawTime;
	struct tm* timeInfo;
	time(&rawTime);
	rawTime += addSecond;
	timeInfo = gmtime(&rawTime);
	strftime(szDate, szLen, "%a, %d %b %Y %H:%M:%S GMT", timeInfo);
	return szDate;
}

//字符串转换成时间戳(秒),字符串格式为:"2016-08-03 06:56:36"
llong str2stmp(const char *strTime) {
	if (strTime != NULL) {
		struct tm sTime;
		memset(&sTime, 0, sizeof(struct tm));
		sTime.tm_isdst = -1; // daylight savings time flag
#ifdef __GNUC__
		if (strchr(strTime, '-')) {
			if (strlen(strTime) > 10)
				strptime(strTime, "%Y-%m-%d %H:%M:%S", &sTime);
			else
				strptime(strTime, "%Y-%m-%d", &sTime);
		}
		else {
			if (strlen(strTime) > 10)
				strptime(strTime, "%Y/%m/%d %H:%M:%S", &sTime);
			else
				strptime(strTime, "%Y/%m/%d", &sTime);
		}
#else
		if (strlen(strTime) > 10) {
			if (strchr(strTime, '-'))
				sscanf(strTime, "%d-%d-%d %d:%d:%d", &sTime.tm_year, &sTime.tm_mon, &sTime.tm_mday, &sTime.tm_hour, &sTime.tm_min, &sTime.tm_sec);
			else
				sscanf(strTime, "%d/%d/%d %d:%d:%d", &sTime.tm_year, &sTime.tm_mon, &sTime.tm_mday, &sTime.tm_hour, &sTime.tm_min, &sTime.tm_sec);
		}
		else {
			if (strchr(strTime, '-'))
				sscanf(strTime, "%d-%d-%d", &sTime.tm_year, &sTime.tm_mon, &sTime.tm_mday);
			else
				sscanf(strTime, "%d/%d/%d", &sTime.tm_year, &sTime.tm_mon, &sTime.tm_mday);
		}
		sTime.tm_year -= 1900;
		sTime.tm_mon -= 1;
		if (sTime.tm_year > 1100)  //windows 下不能超过 3000-12-31, 千年虫
			sTime.tm_year = 1100;
#endif
		return mktime(&sTime);
	}
	else {
		return time(0);
	}
}

//时间戳(秒)转换成字符串,字符串格式为:"2016-08-03 06:56:36"
char* stmp2str(llong t, char* str, int strlen) {
	if (t < 1000000)
		t = time(0);
	struct tm *sTime = localtime((time_t*)&t);
	if (sTime)
		strftime(str, strlen, "%Y-%m-%d %H:%M:%S", sTime);
	return str;
}

//从头比较字符串,返回相同的长度,不区分大小写
size_t strinstr(const char* s1, const char* s2) {
	const char* cur = s1;
	while (s1 && *s1 > 0 && s2 && *s2 > 0) {
		if (*s1 == *s2 || (isalpha(*s1) && isalpha(*s2) && abs(*s1 - *s2) == 32))
			s1++, s2++;
		else
			break;
	}
	return s1 - cur;
}

//获取字符串长度,包括中间有'\0'字符的
size_t strlen_x(const char* str, size_t len) {
	while (len > 0 && str[len - 1] == 0)
		len--;
	return len;
}

//int32 转二进制字符串
char* u2b(uint n) {
	static char b[33] = { 0 };
	b[31] = '0';
	int i = 31;
	uint p = 1;
	for (; i >= 0; i--, p <<= 1) {
		b[i] = (n&p) ? '1' : '0';
	}
	return b;
}
//int64 转二进制字符串
char* u2b64(ullong n) {
	static char b[65] = { 0 };
	b[63] = '0';
	int i = 63;
	ullong p = 1;
	for (; i >= 0; i--, p <<= 1) {
		b[i] = (n&p) ? '1' : '0';
	}
	return b;
}

void printHex(char* d, int len, int limit, const char* str) {
	printf("--data Hex--------%s\n", str);
	for (int i = 0; i < len && i < limit; i++)
		printf("%0X ", (uchar)d[i]);
	printf("\n------------------\n");
}

#ifdef _MSC_VER
//获取当前时间信息
tm_u GetLocaTime() {
	//struct timeval  tv;
	SYSTEMTIME st;
	tm_u     tmu;

	//gettimeofday(&tv, NULL);
	GetLocalTime(&st);

	tmu.tm_sec = st.wSecond;
	tmu.tm_min = st.wMinute;
	tmu.tm_hour = st.wHour;
	tmu.tm_mday = st.wDay;
	tmu.tm_mon = st.wMonth;
	tmu.tm_year = st.wYear;
	tmu.tm_wday = st.wDayOfWeek;//(st.wDay + 2 * st.wMonth + 3 * (st.wMonth + 1) / 5 + st.wYear + st.wYear / 4 - st.wYear / 100 + st.wYear / 400 + 1) % 7;//基姆拉尔森计算公式
	tmu.tm_yday = day_of_year(st.wYear, st.wMonth, st.wDay);
	tmu.tm_isdst = 0;
	tmu.tm_vsec = GetTickCount();//tv.tv_sec;
	tmu.tm_usec = st.wMilliseconds * 1000;//tv.tv_usec;//

	return tmu;
}
//获取当天已逝去的秒数
size_t GetDaySecond() {
	SYSTEMTIME st;
	GetLocalTime(&st);
	return (st.wHour * 3600 + st.wMinute * 60 + st.wSecond);
}

#else
//获取当前时间信息
tm_u GetLocaTime() {
	struct timeval  tv; //timespec
	struct tm       *p;
	tm_u     tmu;

	gettimeofday(&tv, NULL);
	p = localtime(&tv.tv_sec);

	tmu.tm_sec = p->tm_sec;
	tmu.tm_min = p->tm_min;
	tmu.tm_hour = p->tm_hour;
	tmu.tm_mday = p->tm_mday;
	tmu.tm_mon = p->tm_mon + 1;
	tmu.tm_year = p->tm_year + 1900;
	tmu.tm_wday = p->tm_wday;
	tmu.tm_yday = p->tm_yday;
	tmu.tm_isdst = p->tm_isdst;
	tmu.tm_vsec = tv.tv_sec;
	tmu.tm_usec = tv.tv_usec;
	return tmu;
}
int msleep(unsigned int msecs) {
	struct timeval    tval;
	tval.tv_sec = msecs / 1000;
	tval.tv_usec = (msecs % 1000) * 1000;
	return select(0, NULL, NULL, NULL, &tval);
}
//获取当天已逝去的秒数
size_t GetDaySecond() {
	struct timeval  tv;
	gettimeofday(&tv, NULL);
	return (tv.tv_sec % 86400);
}

#endif // _MSC_VER

static char CurIPv4[17] = { 0 };
static char CurIPv6[50] = { 0 };
static char CurMac[25] = { 0 };
#ifdef _MSC_VER

//#include <WinSock2.h>
#include <Iphlpapi.h>
#pragma comment(lib,"Iphlpapi.lib") //需要添加Iphlpapi.lib库

//获取网卡地址
const char* GetMacAddr() {
	if (CurMac[0] < 1)
		GetIPv4();
	return CurMac;
}
//获取IPv4地址 (第一个IPv4)
const char* GetIPv4() {//详情见：http://www.cnblogs.com/lzpong/p/6137652.html
	if (CurIPv4[0])
		return CurIPv4;
	ulong stSize = sizeof(IP_ADAPTER_INFO);
	//PIP_ADAPTER_INFO结构体指针存储本机网卡信息
	PIP_ADAPTER_INFO pIpAdapterInfo = (PIP_ADAPTER_INFO)malloc(stSize);
	//得到结构体大小,用于GetAdaptersInfo参数
	//调用GetAdaptersInfo函数,填充pIpAdapterInfo指针变量;其中stSize参数既是一个输入量也是一个输出量
	int nRel = GetAdaptersInfo(pIpAdapterInfo, &stSize);

	if (ERROR_BUFFER_OVERFLOW == nRel) {
		//如果函数返回的是ERROR_BUFFER_OVERFLOW
		//则说明GetAdaptersInfo参数传递的内存空间不够,同时其传出stSize,表示需要的空间大小,stSize既是一个输入量也是一个输出量
		//释放原来的内存空间
		free(pIpAdapterInfo);
		//重新申请内存空间用来存储所有网卡信息
		pIpAdapterInfo = (PIP_ADAPTER_INFO)malloc(stSize);
		//再次调用GetAdaptersInfo函数,填充pIpAdapterInfo指针变量
		nRel = GetAdaptersInfo(pIpAdapterInfo, &stSize);
	}
	if (ERROR_SUCCESS == nRel) {
		//可能有多网卡,因此通过循环去判断
		while (pIpAdapterInfo) {
			if (MIB_IF_TYPE_ETHERNET == pIpAdapterInfo->Type) {
				sprintf(CurMac, "%02x:%02x:%02x:%02x:%02x:%02x", //以太网MAC地址的长度是48位
						(uchar)pIpAdapterInfo->Address[0],
						(uchar)pIpAdapterInfo->Address[1],
						(uchar)pIpAdapterInfo->Address[2],
						(uchar)pIpAdapterInfo->Address[3],
						(uchar)pIpAdapterInfo->Address[4],
						(uchar)pIpAdapterInfo->Address[5]);
				IP_ADDR_STRING *pIpAddrString = &(pIpAdapterInfo->IpAddressList);
				strncpy(CurIPv4, pIpAddrString->IpAddress.String, 16);
				break;
			}
			pIpAdapterInfo = pIpAdapterInfo->Next;
		}
	}
	//释放内存空间
	if (pIpAdapterInfo)
		free(pIpAdapterInfo);

	return CurIPv4;
}
//获取IPv6地址 (第一个IPv6)
const char* GetIPv6() {
	return CurIPv6;
}

#else

#include <net/if.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
//#include <sys/socket.h>
//获取 IP,MAC 参见 http://www.cnblogs.com/lzpong/p/6956439.html
//获取 MAC 地址
static char* getMac(char* mac, char* dv) {
	struct   ifreq   ifreq;
	int   sock;
	if (!mac || !dv)
		return mac;
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket ");
		return mac;
	}
	strcpy(ifreq.ifr_name, dv);
	if (ioctl(sock, SIOCGIFHWADDR, &ifreq) < 0) {
		perror("ioctl ");
		return mac;
	}
	sprintf(mac, "%02x:%02x:%02x:%02x:%02x:%02x", //以太网MAC地址的长度是48位
			(unsigned char)ifreq.ifr_hwaddr.sa_data[0],
			(unsigned char)ifreq.ifr_hwaddr.sa_data[1],
			(unsigned char)ifreq.ifr_hwaddr.sa_data[2],
			(unsigned char)ifreq.ifr_hwaddr.sa_data[3],
			(unsigned char)ifreq.ifr_hwaddr.sa_data[4],
			(unsigned char)ifreq.ifr_hwaddr.sa_data[5]);
	return mac;
}
//获取IP地址
static int GetIP_v4_and_v6_linux(int family) {
	struct ifaddrs * ifaplist = NULL, *ifap = NULL;
	void * tmpAddrPtr = NULL;

	getifaddrs(&ifaplist);
	ifap = ifaplist;
	while (ifap != NULL) {
		if (ifap->ifa_addr->sa_family == family) { //AF_INET  check it is IP4
			// is a valid IP4 Address
			tmpAddrPtr = &((struct sockaddr_in *)ifap->ifa_addr)->sin_addr;
			inet_ntop(AF_INET, tmpAddrPtr, CurIPv4, INET_ADDRSTRLEN);
			if (strcmp(CurIPv4, "127.0.0.1") != 0) {
				getMac(CurMac, ifap->ifa_name);
				break;
			}
		}
		else if (ifap->ifa_addr->sa_family == family) { //AF_INET6  check it is IP6
			// is a valid IP6 Address
			tmpAddrPtr = &((struct sockaddr_in *)ifap->ifa_addr)->sin_addr;
			inet_ntop(AF_INET6, tmpAddrPtr, CurIPv6, INET6_ADDRSTRLEN);
			if (strcmp(CurIPv6, "::") != 0 && strcmp(CurIPv6, "::1") != 0) {
				getMac(CurMac, ifap->ifa_name);
				break;
			}
		}
		ifap = ifap->ifa_next;
	}
	if (ifaplist) { freeifaddrs(ifaplist); ifaplist = NULL; }
	return -1;
}

//获取网卡地址
const char* GetMacAddr() {
	if (CurMac[0] < 1)
		GetIP_v4_and_v6_linux(AF_INET);
	return CurMac;
}

//获取IPv4地址 (第一个IPv4)
const char* GetIPv4() {
	if (CurIPv4[0] < 1)
		GetIP_v4_and_v6_linux(AF_INET);
	return CurIPv4;
}
//获取IPv6地址 (第一个IPv6)
const char* GetIPv6() {
	if (CurIPv6[0] < 1)
		GetIP_v4_and_v6_linux(AF_INET6);
	return CurIPv6;
}

#endif // _MSC_VER

#pragma endregion