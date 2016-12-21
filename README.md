# TinyWeb v1.0.0
auth [lzpong]
A tiny web server based on libuv, can accept Socket,WebSocket,or http protocol,and callBack func's
```
TinyWeb功能说明

auth lzpong 2016/11/24
功能基于 libuv 跨平台库

0.默认编码为 utf-8
1.支持使用GET/POST方式
2.支持返回404错误页面
3.支持指定根目录（默认程序所在目录）
4.支持任意格式文件访问(带扩展名,小文件下载)
	a.支持静态网页访问：html/htm
	b.支持其他静态文件：js,css,png,jpeg/jpg,gif,ico,txt,xml,json,log,wam,wav,mp3,apk
	c.支持其他格式文件,默认文件类型为："application/octet-stream"
	d.支持不带扩展名文件访问
5.支持默认index页面(index.html/index.html)，可以自定义设置
6.支持目录列表
7.支持Socket, WebSocket
8.支持回调
	a.404前回调（未找到页面/文件时回调,此功能便于程序返回自定义功能）
	b.WebSocket 数据回调
	c.socket 数据回调

==============future
1.支持cookie/session
2.支持认证
3.支持大文件响应（下载）
```