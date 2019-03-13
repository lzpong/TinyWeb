# TinyWeb v1.3.0
auth [lzpong](https://github.com/lzpong)
一个基于libuv的小型Web服务器，可以接受Socket，WebSocket，http协议和设置回调函数。
A tiny web server based on libuv, can accept Socket,WebSocket,or http protocol,and set callBack func's.
```
TinyWeb功能说明

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
13.支持POST较大的数据(支持分包发送的http Post内容)
==============stable,future
14.支持分包发送的http头部(http get)

```
# 使用示例(Use age)
1.包含头文件(include the head file)
```
#include "tinyweb.h"
```
2.设置参数和回调函数(set server configs ,and the callback func's)
```
//配置TinyWeb
tw_config conf;
memset(&conf, 0, sizeof(conf));
conf.dirlist = 1;//是否允许查看目录列表
//conf.ip = NULL;//监听IP,默认"0.0.0.0";
conf.port = 8080;//监听端口
//conf.doc_index = NULL;//默认主页
//配置回调函数
conf.on_request = on_http_request;
conf.on_data = on_socket_read_data;
```
其他配置项详见 `struct tw_config`，在 [tinyweb.h](https://github.com/lzpong/TinyWeb/blob/master/tinyweb.h)。
other congfig items see `struct tw_config` in [tinyweb.h](https://github.com/lzpong/TinyWeb/blob/master/tinyweb.h).

3.启动(start server)
```
//启动TinyWeb
tinyweb_start(uv_default_loop(), &conf);
```
