#include <iostream>
#include <unordered_map>
#include "quic_time.h"
#include "quic_chromium_clock.h"
void quic_time_test1(){
    QuicChromiumClock clock;
    while (1)
    {
        std::cout<<clock.Now()<<std::endl;
    }
};

int main(int argc, char* argv[]){
    quic_time_test1();
};

