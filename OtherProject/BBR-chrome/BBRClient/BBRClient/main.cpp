//
//  main.cpp
//  BBRClient
//
//  Created by Roger Wu on 2021/10/12.
//

#include <iostream>


#include "client.h"
#include "defs.h"

 void client_test1(){
     Client* client = new Client();
     for(int i=0;i<INTMAX_MAX;i++){// use for loop so we can control runing time
         client->SendPacket();
         client->ReceivePacket();
     }
 }

 int main(int argc,char* argv[]){
     client_test1();
 }
 
