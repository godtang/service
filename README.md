Windows service template in pure C

* 安装
* 卸载
* 调试
* 线程间同步

服务启动时应使用事件等待，退出服务时set事件，本domo为简单期间，使用了全局变量