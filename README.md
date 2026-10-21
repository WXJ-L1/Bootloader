#Bootloader
1.Flash内存布局规划
Flash(1MB 1024KB):
0x08000000  Bootlader   32KB
0x08008000  App         480KB
0x08080000  升级缓冲区   384KB
0x080E0000  参数缓冲区   128KB
            日志存储区（可选）
特性：
    Flash只能按扇区擦除，不能单字节
    擦除为1，编程为0，通常以字或者半字编程

2.修改链接脚本（告诉链接器如何组织程序的各个段，并将其放置到正确的内存位置）
Bootloader链接脚本:STM32F407VGTx_FLASH.ld
应用程序链接脚本:STM32F407VGTx_FLASH.ld
在应用程序的system_stm32f4xx.c中，添加中断向量表重定位：
    Bootloader和应用程序都有自己的中断向量表
    跳转到应用程序后，需要使用应用程序的中断向量表

3.启动流程
上电/复位
CPU读取Bootloader中断向量表：
    读取0x08000000地址-加载初始MSP
    读取0x08000004地址-加载Reset_Handler地址
执行Bootloader的Reset_Handler
硬件初始化
系统初始化、进入main()

检查是否需要进入升级模式：
    是：固件下载
        写入Upgrade区
        校验固件
        复制到App
        复位重启
        恢复模式
    否：检查Application是否有效
        关闭相关中断和外设
        设置VTOR为Application向量表地址
        设置Application的MSP
        跳转Application的Reset_Handler
