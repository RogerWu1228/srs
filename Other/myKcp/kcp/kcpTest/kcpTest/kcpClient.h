//
//  kcpClient.h
//  kcpTest
//
//  Created by Roger Wu on 2021/9/15.
//

#ifndef kcpClient_h
#define kcpClient_h

#include "ikcp.h"
#include <arpa/inet.h>

typedef struct {
   
    unsigned char *ipstr;
   
    int port;
   
    ikcpcb *pkcp;
   
    int sockfd;
   
    sockaddr_in addr;//存放服务器的结构体
   
    char buff[488];//存放收发的消息
   
}kcpObj;

/* get system time */
void itimeofday(long *sec, long *usec);

/* get clock in millisecond 64 */
IINT64 iclock64(void);

IUINT32 iclock();

/* sleep in millisecond */
void isleep(unsigned long millisecond);

int udpOutPut(const char *buf, int len, ikcpcb *kcp, void *user);

void init(kcpObj *send);

void loop(kcpObj *send);


#endif /* kcpClient_h */
