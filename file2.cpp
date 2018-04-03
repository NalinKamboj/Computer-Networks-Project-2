#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <memory.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <errno.h>
#include <stdlib.h>
#include <iostream>
using namespace std;

int main(){
    int sendLen = 0;
    char buf[64];
    //char *test = "This is a test message!";
    int result = 0;
    int sendSock = socket(AF_INET, SOCK_DGRAM, 0);
    if(sendSock<0)
        cout<<"Cannot create socket!\n\n";
    char szIP[100];

    sockaddr_in addrListen = {};    //zero-int, sin_port is 0 which picks a random port for bind
    addrListen.sin_family = AF_INET;
    addrListen.sin_port = 9000;
    addrListen.sin_addr.s_addr = inet_addr("127.0.0.1");

    if(bind(sendSock, (struct sockaddr *) &addrListen, sizeof(addrListen)) < 0)
        cout<<"FAIL";
    while(1){
        sendLen = sizeof(struct sockaddr_in);
        recvfrom(sendSock, buf, 64, 0, (struct sockaddr *) &addrListen, (socklen_t *) &sendLen);
        cout<<buf<<endl;
    }

}