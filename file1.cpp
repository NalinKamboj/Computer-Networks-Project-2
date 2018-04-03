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
#include <fstream>
#include <stdlib.h>
#include <iostream>
#include <pthread.h>
#include <string.h>

#define BUF_LEN 64
#define MAX_NEIGHBORS 8

using namespace std;

/* 
GENERAL PACKET HEADER - "DESTN:<DESTN>,SRC:<SRC>,FUNC:<FUNC>,TYPE:1/0,MSG:<MSG>"
TYPE: Referes to whether the packet is a reply or a request
FUNCTIONS:  1 - Initial set-up (pinging nodes for obtaining their neighbor information)
            2 - Update weights
            3 - Normal messenger
            4 - SRC node shutting down
*/

//Necessary function signatures
void *router_connection(void *threadID);
void *data_sender(void *threadID);
void packetParser(char *);
void printMenu();
void initShutdown();        //for shutting down the router

struct routerData {
    char src;
    //char src_ip[16];
    int port;
    int num_neighbors;
}router1;

struct nRouter{     //Struct for storing neighbor router info
    char src;
    int port;
    int weight;
}neighbors[MAX_NEIGHBORS];

class nRouters {
    private:
    char src;
    int port;
    int weight;
    
    public:
    nRouters() {
        src = -1;
    }

    char getSrc(){
        return src;
    }

    int getPort(){
        return port;
    }

    int getWeigjt(){
        return weight;
    }

    void setSrc(char s){
        src = s;
    }

    void setPort(int p){
        port = p;
    }

    void setWeight(int w){
        weight = w;
    }
};

struct edge {
    char v1;
    char v2;
    int weight;
};
//GENERAL PACKET HEADER - "DESTN:<DESTN>,SRC:<SRC>,FUNC:<FUNC>,TYPE:1/0,PORT:<PORT>,MSG:<MSG>"

void packandsend(int index, int funcType, int type = 1, string message = "", struct edge *ed = NULL) {
    int sendLen = sizeof(struct sockaddr_in);
    int numBytes = 0;
    //string finalMessage = router1.src + message;
    string finalMessage;
    char destn = neighbors[index].src;
    string destn_s(1,destn);
    if(funcType == 1){
        if(type != 0)
            finalMessage = "DESTN:"+destn_s+",SRC:"+router1.src+",FUNC:1,TYPE:1,MSG:"+message;
        else{
            finalMessage = "DESTN:"+destn_s+",SRC:"+router1.src+",FUNC:1,TYPE:0,MSG:"+message;

        }
    }
    else if(funcType == 2) {
        string v1_s(1, ed->v1);
        string v2_s(1, ed->v2);
        message = "V1-" + v1_s + ",V2-" + v2_s + ",W-" + to_string(ed->weight);
        finalMessage = "DESTN:"+destn_s+",SRC:"+router1.src+",FUNC:2,TYPE:1,MSG:"+message;
        //cout<<finalMessage<<endl;
    }
    else if(funcType == 3) {
        finalMessage = "DESTN:"+destn_s+",SRC:"+router1.src+",FUNC:3,TYPE1,MSG:"+message;
    }
    else
        cout<<"\t\tSender: Wrong function type :( "<<endl;
    char sendBuf[(finalMessage.length())+1];
    strcpy(sendBuf, finalMessage.c_str());
    int sendSock = socket(AF_INET, SOCK_DGRAM, 0);
    if(sendSock<0)
        cout<<"\n\t\tSender: Cannot create socket!\n\n";
    
    sockaddr_in addrSend = {};    //zero-int, sin_port is 0 which picks a random port for bind
    addrSend.sin_family = AF_INET;
    addrSend.sin_port = htons(neighbors[index].port);
    addrSend.sin_addr.s_addr = inet_addr("127.0.0.1");        //Default for now.

    bind(sendSock, (struct sockaddr *) &addrSend, sizeof(addrSend));     //Not checking failure for now..
    //    cout<<"\n\t\t BIND FAIL";    
    if((numBytes=(sendto(sendSock, sendBuf, strlen(sendBuf), 0, (struct sockaddr *) &addrSend, sendLen))) == -1) {
        cout<<"\t\tSender: Couldn't send. :("<<endl;
        return;
    }
    cout<<"\t\tSender: Message sent "<<numBytes<<" to "<<neighbors[index].src<<" on port "
        << neighbors[index].port <<" using port: "<<sendSock<<endl; 
    close(sendSock);   
}

int main(int argc, char **argv){
    /*
    int sendLen = 0;
    //struct hostent *hp;
    const char *test = "This is a test message!";
    //int result = 0;
    int sendSock = socket(AF_INET, SOCK_DGRAM, 0);
    if(sendSock<0)
        cout<<"Cannot create socket!\n\n";
    //char szIP[100];

    sockaddr_in addrListen = {};    //zero-int, sin_port is 0 which picks a random port for bind
    addrListen.sin_family = AF_INET;
    addrListen.sin_port = 9000;
    addrListen.sin_addr.s_addr = inet_addr("127.0.0.1");

    if(bind(sendSock, (struct sockaddr *) &addrListen, sizeof(addrListen)) < 0)
        cout<<"FAIL";
    while(1){
        sendLen = sizeof(struct sockaddr_in);
        sendto(sendSock, test, strlen(test), 0, (struct sockaddr *) &addrListen, sendLen);
    }
    */
    char buf[BUF_LEN];
    //nRouter* neighbors = new nRouter[MAX_NEIGHBORS];
    //nRouters* neighbors = new nRouters[MAX_NEIGHBORS];
    pthread_t connection_thread, sender_thread;
    //pthread_attr_t senderAttr;

    //Before starting anything, initializing all neighbor costs to 1000 (infinity) and src to -1
    for(int i = 0; i<MAX_NEIGHBORS; i++) {
        neighbors[i].weight = 1000;
        neighbors[i].src = -1;
    }

    if(argc<3)
        cout<<"Please enter all details\n";
    //strcpy(router1.src_ip, argv[1]);
    router1.src = argv[1][0];
    router1.port = atoi(argv[2]);
    router1.num_neighbors = 0;      //initially set the number of neighbors as zero

    //Reading details for initializing router neighbors
    ifstream initFile;

    initFile.open("INIT_FILE.txt");
    if(!initFile) {
        cerr << "Unable to open the file!";
        exit(1);
    }
    initFile.getline(buf, BUF_LEN);
    if(initFile.is_open()) {
        char c;
        //initFile>>c;
        cout<<"Main: Current source: "<<router1.src<<endl;
        while(initFile) {
            initFile >> c;
            //cout<<"\nBEFORE IF: "<<c<<endl;
            if(router1.src == c) {
                router1.num_neighbors++;
                initFile>>neighbors[router1.num_neighbors-1].src>>neighbors[router1.num_neighbors-1].port
                >>neighbors[router1.num_neighbors-1].weight;
                cout<<neighbors[router1.num_neighbors-1].src<<endl;
                cout<<neighbors[router1.num_neighbors-1].port<<endl;
                cout<<neighbors[router1.num_neighbors-1].weight<<endl;
            } else {
                //int temp;
                //initFile>>c;
                //initFile>>temp;
                initFile.getline(buf, BUF_LEN);
                memset(buf, 0, BUF_LEN);
            }
            c = -1;
        }
        initFile.close();
    }
    cout<<"\nMain: Initializing Router using given INIT_FILE ... \n";
    for(int i = 0; i<router1.num_neighbors; i++) {
        cout<<"\nNeighbor "<<i+1<<": \n \t NAME: "<< neighbors[i].src<<"\n\t PORT: "<<neighbors[i].port
        <<"\n\t WEIGHT: "<<neighbors[i].weight<<endl;
    }

    cout<<"Main: Now attempting to setup the connection..."<<endl;
    int i = 1;
    void *status;
    //pthread_attr_init(&senderAttr);
    //pthread_attr_setdetachstate(&senderAttr, PTHREAD_CREATE_JOINABLE);
    pthread_create(&connection_thread, NULL, router_connection, (void *) i);
    pthread_create(&sender_thread, NULL, data_sender, (void *) (i+1));
    pthread_join(sender_thread, &status);       //we dont care about status, for now.
    pthread_cancel(connection_thread);
    //pthread_join(connection_thread, &status);
    cout<<"\nMain: All threads closed, shutting down node..."<<endl;
    return 0;
}

void *router_connection(void *threadID) {
    cout<<"\tConnection: Connection thread is running..."<<endl;
    int threadNum = (intptr_t) threadID;
    int routerSock;
    struct sockaddr_in routerAddr;
    struct sockaddr_storage senderAddr;
    char sendBuf[BUF_LEN], recvBuf[BUF_LEN];
    int recvLen;

    socklen_t addrLen = sizeof(senderAddr);

    if((routerSock = socket(AF_INET, SOCK_DGRAM, 0))<0)
        cout<<"Connection: Thread number "<<threadNum<<" couldn't create socket. :("<<endl;

    //Bind socket to our router 
    memset((char *)&routerAddr, 0, sizeof(routerAddr));         
    routerAddr.sin_family = AF_INET;
    routerAddr.sin_port = htons(router1.port);
    routerAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if(bind(routerSock, (struct sockaddr *) &routerAddr, sizeof(routerAddr)) < 0)
    {
        cerr<<"Connection: Router binding failed. Killing thread..."<<endl;
        exit(1);
    }

    for(;;) {
        cout<<"\tConnection: Router " << router1.src<<" listening on port " << router1.port << endl;
        //cout<<"Gaandu!";
        recvLen = recvfrom(routerSock, recvBuf, BUF_LEN, 0, (struct sockaddr *) &senderAddr, &addrLen);
        cout<<"\n\tConnection: Received "<<recvLen<<" bytes."<<endl;
        if(recvLen > 0) {
            recvBuf[recvLen] = 0;
            //cout<<"\tConnection: Received message from " << recvBuf[0] <<" : " << recvBuf << endl;
            packetParser(recvBuf);
            //break;
        }
    }
    close(routerSock);
}

void *data_sender(void *threadID) {
    int threadNum;
    string temp;    //for storing message temporarily to pass to packandsend
    char destn;
    int type = -1;
    string message;
    //memset(sendBuf, 0, sizeof(sendBuf));        //Initializing our buffer to 0.
    threadNum = (intptr_t) threadID;
    int senderSock = -1;    //Not sending to any client initially.
    int response = 1;
    sleep(1.5);
    while(response) {
        cout<<"Messenger: Enter the the destination (enter " << router1.src << " to shutdown router): ";
        cin>>destn;
        cout<<destn<<endl;
        if(destn == router1.src) {
            //initShutdown();
        }
        else {
            cout<<"Messenger: Please enter message type (1 - Initialization, 2 - Update Weight, 3 - Message): ";
            cin>>type;
            struct edge e1;
            switch(type){
                case 1:
                for(int i = 0; i<MAX_NEIGHBORS; i++) 
                    packandsend(i, type);
                break;
                case 2:
                cout<<"Messenger: Enter the initial node: ";
                cin.ignore();
                cin>>e1.v1;
                toupper(e1.v1);
                cout<<"Messenger: Enter the ending node: ";
                cin.ignore();
                cin>>e1.v2;
                toupper(e1.v2);
                cout<<"Messenger: Enter the new weight of the edge: ";
                cin.ignore();
                cin>>e1.weight;
                cout<<"V1 " <<e1.v1<<" V2 "<<e1.v2<<endl;
                for(int i = 0; i<MAX_NEIGHBORS; i++) {
                    if(neighbors[i].src == destn) 
                        packandsend(i, type, 1, "", &e1);
                }
                break;
                case 3:
                cout<<"Messenger: Enter message for "<<destn<<": ";
                cin.ignore();
                getline(cin, message);
                for(int i = 0; i<MAX_NEIGHBORS;i++) {
                    if(neighbors[i].src == destn) {
                        packandsend(i, type, 1);
                    }
                    else
                        cout<<"Messenger: Destination not found. "<<endl;
                }
                break;
                default:
                break;
            }
        }
        sleep(0.5);
        //cout<<"Messenger: You entered: \n\t\t"<<message<<endl;
        cout<<"\nMessenger: Do you want to continue? (1: yes, 0: shut down router): ";
        cin>>response;
    }
}

void packetParser(char *buf) {
    cout<<"Parser received " << buf << endl;
    //All these varaiables will always be found at these pos due to structure of packet
    char src = buf[12];
    char destn = buf[6];
    int type = buf[26] - '0';
    int funcType = buf[19] - '0';  
    string message;
    switch(funcType) {
        case 1:
        //Send back info of all my neighbors to the src with type - 0 (reply/ack)
        if(funcType == 1 && type == 1) {
            for(int i = 0; i<MAX_NEIGHBORS; i++) {
                char src = neighbors[i].src;
                string src_s(1, src);
                char destn;
                if(neighbors[i].src != -1){
                    message = "N" + to_string(i+1) + "-" + src_s + ",W" + to_string(i+1) + "-"
                    + to_string(neighbors[i].weight);
                    cout<<"\t\t"<<message<<endl; 
                    packandsend(i, funcType, 0, message);     //Replying with my router's neighbor info
                }
            }
        }
        break;
        case 2:
        break;
        case 3:
        break;
    }
}