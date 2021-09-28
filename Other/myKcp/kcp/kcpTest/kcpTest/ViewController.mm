//
//  ViewController.m
//  kcpTest
//
//  Created by Roger Wu on 2021/9/15.
//

#import "ViewController.h"

#include "kcpClient.h"

@implementation ViewController

- (void)viewDidLoad {
    [super viewDidLoad];

    // Do any additional setup after loading the view.
}

void test() {
    
    //printf("this is kcpClient,请输入服务器 ip地址和端口号：\n");
//    if(argc != 3)
//    {
//        printf("请输入服务器ip地址和端口号\n");
//        return -1;
//    }
//    printf("this is kcpClient\n");
    
//    unsigned char *ipstr = (unsigned char *)argv[1];
//    unsigned char *port  = (unsigned char *)argv[2];
    
    
    unsigned char *ipstr;
    unsigned char *port;
    
    kcpObj send;
    send.ipstr = ipstr;
    send.port = atoi("80");
    
    init(&send);//初始化send,主要是设置与服务器通信的套接字对象
    
    bzero(send.buff,sizeof(send.buff));
    char Msg[] = "Client:Hello!";//与服务器后续交互
    memcpy(send.buff,Msg,sizeof(Msg));
    
    ikcpcb *kcp = ikcp_create(0x1, (void *)&send);//创建kcp对象把send传给kcp的user变量
    kcp->output = udpOutPut;//设置kcp对象的回调函数
    ikcp_nodelay(kcp,0, 10, 0, 0);//(kcp1, 0, 10, 0, 0); 1, 10, 2, 1
    ikcp_wndsize(kcp, 128, 128);
    
    send.pkcp = kcp;
    
    char temp[] = "Conn";//与服务器初次通信
    int  ret = ikcp_send(send.pkcp,temp,(int)sizeof(temp));
        
    printf("ikcp_send发送连接请求： [%s] len=%d ret = %d\n",temp,(int)sizeof(temp),ret);//发送成功的
        
    loop(&send);//循环处理
}

- (void)setRepresentedObject:(id)representedObject {
    [super setRepresentedObject:representedObject];

    // Update the view, if already loaded.
}




@end
