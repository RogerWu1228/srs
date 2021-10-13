//
//  main.cpp
//  BBRServer
//
//  Created by Roger Wu on 2021/10/12.
//

#include <iostream>

#include "src/server.h"


void server_test1(){
    Server* server =  new Server();
    std::cout<<"start to receive data"<<std::endl;
    while (1){
        server->ReceiveData();
    }
}
int main(int argc,char* argv[]){
    server_test1();
}
