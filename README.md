# WebServer

Linux下基于c++编写的一个小型Web服务器。

- 使用 **线程池 + 非阻塞socket + epoll** 的并发模型
- 使用**状态机**解析HTTP请求报文，支持解析**GET和POST**请求
- 访问服务器数据库实现web端用户**注册、登录**功能，可以请求服务器**图片和视频文件**
- 实现**异步日志系统**，独立一个线程用于记录服务器运行状态
- 经Webbench压力测试可以实现**五千个的并发连接**数据交换

# 示例

## webbench压力测试

![webbench](https://github.com/mcc-run/WebServer/blob/master/WebServer/introduction/webbench.png)

## 日志系统

![log](https://github.com/mcc-run/WebServer/blob/master/WebServer/introduction/log.png)

## 首页

![home_page](https://github.com/mcc-run/WebServer/blob/master/WebServer/introduction/home_page.png)

## 获取图片

![getpicture](https://github.com/mcc-run/WebServer/blob/master/WebServer/introduction/getpicture.png)

## 获取视频

![getvideo](https://github.com/mcc-run/WebServer/blob/master/WebServer/introduction/getvideo.png)
