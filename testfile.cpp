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
#include <vector>

#define BUF_LEN 256
#define MAX_ROUTERS 8       //Hardcoded maximum number of routers on our network

using namespace std;

/* 
GENERAL PACKET HEADER - "DESTN:<DESTN>,SRC:<SRC>,FUNC:<FUNC>,TYPE:1/0,MSG:<MSG>"
TYPE: Referes to whether the packet is a reply or a request
FUNCTIONS:  1 - Initial set-up (pinging nodes for obtaining their neighbor information)
            2 - Update weights (NOT IMPLEMENTED)
            3 - Normal messenger
*/

//Necessary function signatures
void *router_connection(void *threadID);
void *data_sender(void *threadID);
void packetParser(char *);
void printMenu();
void BellmanFord(struct graph*, int);
void printMenu();
void writeDVInfo();

int menuStage;      //Global variable for storing what menu to print on the screen.

struct DV {
    char node;
    //int port;
    int shortestDist;
    char nextNode;
}dvinfo[MAX_ROUTERS];

struct routerData {     //MY ROUTER'S data
    char src;
    //char src_ip[16];
    int port;
    int num_neighbors;
}router1;

struct nRouter{     //Struct for storing neighbor router info
    char src;
    int port;
    int weight;
}neighbors[MAX_ROUTERS];

struct destinationRouter {      //Struct used for storing destination parameters
    int port;
    char destn;
};

struct edge {
    char v1;
    char v2;
    int weight;
};

struct graph {
    int V, E;
    vector<struct edge*> edges;
};
struct graph* maingraph;            //GRAPH FOR STORING ALL EDGES

//GENERAL PACKET FORMAT - "DESTN:<DESTN>,SRC:<SRC>,FUNC:<FUNC>,TYPE:1/0,PORT:<PORT>,MSG:<MSG>"

void packandsend(struct destinationRouter *destnR, int funcType, int type = 1, string message = "", struct edge *ed = NULL,
int storeorsend = 0, char actualDestination = -1) {
    int sendLen = sizeof(struct sockaddr_in);
    int numBytes = 0;
    //string finalMessage = router1.src + message;
    string finalMessage;
    char destn = destnR->destn;
    string destn_s(1,destn);
    if(funcType == 1){
        if(type != 0)
            finalMessage = "DESTN:"+destn_s+",SRC:"+router1.src+",FUNC:1,TYPE:1,PORT:"+to_string(router1.port)+",MSG:"+
            message+","+destn_s;
        else{
            finalMessage = "DESTN:"+destn_s+",SRC:"+router1.src+",FUNC:1,TYPE:0,PORT:"+to_string(router1.port)+",MSG:"+
            message+","+destn_s;

        }
    }
    else if(funcType == 2) {
        string v1_s(1, ed->v1);
        string v2_s(1, ed->v2);
        message = "V1-" + v1_s + ",V2-" + v2_s + ",W-" + to_string(ed->weight);
        finalMessage = "DESTN:"+destn_s+",SRC:"+router1.src+",FUNC:2,TYPE:1,PORT:"+to_string(router1.port)+",MSG:"+message;
        //cout<<finalMessage<<endl;
    }
    else if(funcType == 3) {
        if(storeorsend == 0)
            finalMessage = "DESTN:"+destn_s+",SRC:"+router1.src+",FUNC:3,TYPE:1,PORT:"+to_string(router1.port)+
            ",MSG:"+message+","+destn_s;
        else {
            cout<<"\nSTOREORSEND "<<storeorsend<<" DESTN: "<<actualDestination<<"\n";
            finalMessage = "DESTN:"+destn_s+",SRC:"+router1.src+",FUNC:3,TYPE:1,PORT:"+to_string(router1.port)+
            ",MSG:"+message+","+actualDestination;
        }
    }
    else
        cout<<"\n\t\tSender: Wrong function type :( "<<endl;

    char sendBuf[(finalMessage.length())+1];
    strcpy(sendBuf, finalMessage.c_str());
    int sendSock = socket(AF_INET, SOCK_DGRAM, 0);
    if(sendSock<0)
        cout<<"\n\t\tSender: Cannot create socket!\n\n";
    
    sockaddr_in addrSend = {};    //zero-int, sin_port is 0 which picks a random port for bind
    addrSend.sin_family = AF_INET;
    addrSend.sin_port = htons(destnR->port);
    addrSend.sin_addr.s_addr = inet_addr("127.0.0.1");        //Default for now.

    bind(sendSock, (struct sockaddr *) &addrSend, sizeof(addrSend));     //Not checking failure for now..
    //    cout<<"\n\t\t BIND FAIL";    
    if((numBytes=(sendto(sendSock, sendBuf, strlen(sendBuf), 0, (struct sockaddr *) &addrSend, sendLen))) == -1) {
        cout<<"\n\t\tSender: Couldn't send. :("<<endl;
        return;
    }
    cout<<"\n\t\tSender: Message sent "<<numBytes<<" to "<<destnR->destn<<" on port "
        << destnR->port <<" using port: "<<sendSock<<"\n"<<"\t\tMESSAGE: "<< sendBuf; 
    close(sendSock);   
}

struct graph* initGraph () {
    struct graph* g = new graph;
    g->E = 0;
    g->V = 0;
    g->edges.resize(0);
    return g;
}

void printGraph(struct graph* g) {
    cout<<"\n\t--------------------------------------------------";
    cout<<"\n\t| GRAPH: Vertices- "<<g->V<<" Edges - "<<g->E<<"\t\t\t |";
    for(int i=0;i<g->edges.size();i++)
        cout<<"\n\t| Starting node: "<<g->edges[i]->v1 <<", Ending node: "<<g->edges[i]->v2<<", Weight: "
        <<g->edges[i]->weight<<"\t |";
    cout<<"\n\t--------------------------------------------------";
}

void insertEdge(struct graph* g, char x, char y, int w) {
    int flag = 0;
    //cout<<"TEMPGRAPH: "<<tempGraph.E<<" "<<tempGraph.V<<endl;
    edge *e = (struct edge*)malloc(sizeof(struct edge));
    e->v1 = x;
    e->v2 = y;
    e->weight = w;
    DV *tempDV = (struct DV*)malloc(sizeof(struct DV));
    if(g->V == 0){
        g->E++;
        g->V+=2;
        g->edges.push_back(e);
        cout<<"\n\t\t\tGRAPHER: Added First Edge: \t"<<g->edges[0]->v1<<"->"<<g->edges[0]->v2<<" W: "<<g->edges[0]->weight;
    }
    else{
        for(int i = 0; i<g->edges.size(); i++){     //Check if given edge already exists in graph
            if(g->edges[i]->v1 == x) {
                if(g->edges[i]->v2 == y) {
                    cout<<"\n\t\t\tGRAPHER: Edge already exists."<<endl;
                    flag = 1;
                    if(g->edges[i]->weight != w) {
                        cout<<"\n\t\t\tGRAPHER: Updating weight..."<<endl;
                        g->edges[i]->weight = w;
                        flag = 1;
                    }
                }             
            }
        }
        if(flag == 0) {     //Adding edge if it doesn't already exist in the graph
            int index, flag1 = 1, flag2= 1;            
            g->E++;
            //g->V+=2;
            g->edges.push_back(e);
            for(int i = 0; i<g->edges.size(); i++)
            {
                if(g->edges[i]->v1 == x || g->edges[i]->v2 == y)
                    flag1 = 0;
                if(g->edges[i]->v1 == y || g->edges[i]->v2 == x)
                    flag2 = 0;
            }                
            g->V = g->V + flag1 + flag2;
            index = g->edges.size()-1;
            cout<<"\n\t\t\tGRAPHER: Inserted new edge***: \t"<<g->edges[index]->v1
            <<"->"<<g->edges[index]->v2<<" W: "<<g->edges[index]->weight;
        }
    }
    cout<<"\n--------------------------------------------------------------------------\n";
    //sleep(2);
    //BellmanFord(maingraph, router1.src);
}

struct discoveredRouters {
    char src;
    int port;
};

vector<struct discoveredRouters*> discRouters;

// A utility function used to print the solution
void printArr()
{
    /*
    //cout<<"\nN: "<<n;
    cout<<"\n\t---------------------------------\n\t| Vertex   Distance from Source |";
    for (int i = 0; i < MAX_ROUTERS; ++i) {
        char ch = (char)(i+65);
        cout<<"\n\t| "<<ch<<"\t\t  "<<dist[i]<<"\t\t|";
    }
    cout<<"\n\t---------------------------------\n";
    */
    cout<<"\n\t---------------------------------------------------------\n\t| Vertex\tDistance from Source\tPrevious Node\t|";
    for (int i = 0; i < MAX_ROUTERS; ++i) {
        char ch = (char)(i+65);
        if(dvinfo[i].nextNode == -1)
            cout<<"\n\t| "<<dvinfo[i].node<<"\t\t\t"<<dvinfo[i].shortestDist<<"\t\t\t\t|";
        else    
            cout<<"\n\t| "<<dvinfo[i].node<<"\t\t\t"<<dvinfo[i].shortestDist<<"\t\t\t"<< dvinfo[i].nextNode << "\t|";
    }
    cout<<"\n\t---------------------------------------------------------\n";
}

int main(int argc, char **argv){
    char buf[BUF_LEN];
    pthread_t connection_thread, sender_thread;
    
    //Before starting anything, initializing all neighbor costs to 1000 (infinity) and src to -1
    for(int i = 0; i<MAX_ROUTERS; i++) {
        neighbors[i].weight = 1000;
        neighbors[i].src = -1;
    }

    if(argc<3)
    {
        cout<<"Please enter all details\n";
        exit(0);
    }
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
            } else {
                initFile.getline(buf, BUF_LEN);
                memset(buf, 0, BUF_LEN);
            }
            c = -1;
        }
        initFile.close();
    }
    cout<<"\nMain: Initializing Router using given INIT_FILE ... \n";
    for(int i = 0; i<router1.num_neighbors; i++) {
        cout<<"\n    Neighbor "<<i+1<<": \n \t NAME: \t\t"<< neighbors[i].src<<"\n\t PORT: \t\t"<<neighbors[i].port
        <<"\n\t WEIGHT: \t"<<neighbors[i].weight<<endl;
    }
    //Create initial graph using initialized neighbors... initialize empty graph elements to -1 to denote they're empty
    
    maingraph = initGraph();
    for(int i = 0; i<router1.num_neighbors; i++) 
        insertEdge(maingraph, router1.src, neighbors[i].src, neighbors[i].weight);

    cout<<"\nMain: Initial graph..."<<endl;
    printGraph(maingraph);
    
    BellmanFord(maingraph, (int)router1.src);

    cout<<"\nMain: Now attempting to setup the connection..."<<endl;
    int i = 1;
    void *status;
    pthread_create(&connection_thread, NULL, router_connection, (void *) i);
    pthread_create(&sender_thread, NULL, data_sender, (void *) (i+1));
    pthread_join(sender_thread, &status);       //we dont care about status, for now.
    pthread_cancel(connection_thread);
    cout<<"\nMain: All threads closed, shutting down node..."<<endl;
    return 0;
}

void *router_connection(void *threadID) {
    cout<<"\n\tConnection: Connection thread is running...";
    int threadNum = (intptr_t) threadID;
    int routerSock;
    struct sockaddr_in routerAddr;
    struct sockaddr_storage senderAddr;
    char sendBuf[BUF_LEN], recvBuf[BUF_LEN];
    int recvLen;

    socklen_t addrLen = sizeof(senderAddr);

    if((routerSock = socket(AF_INET, SOCK_DGRAM, 0))<0)
        cout<<"\n\tConnection: Thread number "<<threadNum<<" couldn't create socket. :(";

    //Bind socket to our router 
    memset((char *)&routerAddr, 0, sizeof(routerAddr));         
    routerAddr.sin_family = AF_INET;
    routerAddr.sin_port = htons(router1.port);
    routerAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if(bind(routerSock, (struct sockaddr *) &routerAddr, sizeof(routerAddr)) < 0)
    {
        cerr<<"\n\tConnection: Router binding failed. Killing thread...";
        exit(1);
    }

    for(;;) {
        recvLen = recvfrom(routerSock, recvBuf, BUF_LEN, 0, (struct sockaddr *) &senderAddr, &addrLen);
        cout<<"\n\tConnection: Received "<<recvLen<<" bytes.";
        if(recvLen > 0) {
            recvBuf[recvLen] = 0;
            packetParser(recvBuf);
        }
    }
    close(routerSock);
}

void *data_sender(void *threadID) {
    menuStage = 1;
    int threadNum;
    string temp;    //for storing message temporarily to pass to packandsend
    char destn;
    int type = -1;
    string message;
    string finalMsg;
    threadNum = (intptr_t) threadID;
    int senderSock = -1;    //Not sending to any client initially.
    int response = 1;
    sleep(1.5);
    while(response) {
        menuStage = 1;
        cout<<"\n\nMESSENGER: Enter the the destination (enter " << router1.src << " for router functions): ";
        cin>>destn;
        //cout<<"\t"<<destn<<endl;
        struct edge *e1 = (struct edge*)malloc(sizeof(struct edge));
        struct destinationRouter destnR; 
        if(destn == router1.src) {
            menuStage = 2;
            int ch;
            cout<<"\n----------------------------------------------------------------------------";
            cout<<"\nMESSENGER: 1 - Initialize Nodes, 2 - Shutdown, 3 - View Routing Table\n\t Enter choice: ";
            cin>>ch;
            switch(ch) {
                case 1:
                type = 1;
                for(int i = 0; i<MAX_ROUTERS; i++)
                {
                    if(neighbors[i].src != -1) {
                        destnR.destn = neighbors[i].src;
                        destnR.port = neighbors[i].port;
                        packandsend(&destnR, type);
                    }
                }
                break;
                
                case 2:
                pthread_exit(NULL);
                /*
                for(int i=0; i<router1.num_neighbors; i++){     //Update all neighbors about router shutdown
                    destnR.destn = neighbors[i].src;
                    destnR.port = neighbors[i].port;
                    packandsend(&destnR, 5);
                }
                for(int i =0; i<MAX_ROUTERS; i++){
                    if(dvinfo[i].node != -1){
                        destnR.destn = dvinfo[i].node;
                    }
                }*/
                break;

                case 3:
                printArr();
                break;
                default:
                cout<<"\n\nMESSENGER: Wrong choice entered.";
                break;
            }
        }
        else {
            menuStage = 3;
            cout<<"\nMESSENGER: Enter message for "<<destn<<": ";
            cin.ignore();
            getline(cin, message);
            finalMsg = message + router1.src;
            cout<<"MESSAGE TO BE SENT: "<<finalMsg;
            int flag = 0;
            for(int i = 0; i<router1.num_neighbors;i++) {
                if(neighbors[i].src == destn) {
                    destnR.destn = destn;
                    destnR.port = neighbors[i].port;
                    flag = 1;
                    packandsend(&destnR, 3, 1, finalMsg);
                    break;
                }
            }
            if(flag==1)
                goto skip;
            char prevNode;
            if(flag == 0) {  //Router is not a neighbor, check if it exists in discovered routers
                for(int i = 0; i<MAX_ROUTERS; i++) {
                    if(dvinfo[i].node == destn) {
                        prevNode = dvinfo[i].nextNode;
                        flag = 1;
                    }
                }
            }
            if(flag == 1) { //Router exists in discoverd routers, now find the neighbor to which we need to transmit packet
                for(int i = 0; i<router1.num_neighbors; i++) {
                    if(prevNode == neighbors[i].src) {
                        flag = 0;       //NEIGHBOR FOUND!
                        destnR.destn = prevNode;
                        destnR.port = neighbors[i].port;
                        packandsend(&destnR, 3, 1, finalMsg, NULL, 1, destn);
                        goto skip;
                    }
                }
                while(flag){
                    prevNode = dvinfo[prevNode%65].nextNode;
                    for(int i = 0; i<router1.num_neighbors; i++) {
                        if(prevNode == neighbors[i].src) {
                            destnR.destn = prevNode;
                            destnR.port = neighbors[i].port;
                            packandsend(&destnR, 3, 1, finalMsg, NULL, 1, destn);
                            flag = 0;       //NEIGHBOR FOUND!
                            goto skip;
                        }
                    }
                }
                cout<<"\nMESSENGER: Packet forwarded to "<<prevNode << " for Router " << destn << "\n";
            } else 
                cout<<"\nMESSENGER: Router not found on network.\n";                
        }
        sleep(0.5);
        skip:
        //cout<<"Messenger: You entered: \n\t\t"<<message<<endl;
        cout<<"\nMessenger: Do you want to continue? (1: yes, 0: shut down router): ";
        cin>>response;
    }
}


//GENERAL PACKET HEADER - "DESTN:<DESTN>,SRC:<SRC>,FUNC:<FUNC>,TYPE:1/0,PORT:<PORT>,MSG:<MSG>,<ACTUALDESTN>*"
// * - optional (only required is packet is to be forwarded to another router)
void packetParser(char *buf) {
    cout<<"\n- - - - - - - - - - - - - - - - - - - - - -\n";
    cout<<"\n\t\tRECEIVED MESSAGE: \n\t"<<buf;
    cout<<"\n- - - - - - - - - - - - - - - - - - - - - -\n";
    //All these varaiables will always be found at these pos due to structure of packet
    char originalsrc = buf[12];
    char destn = buf[6];
    char port[5];
    for(int i = 0; i<5;i++)
        port[i] = buf[33+i];
    int port_i = atoi(port);
    int type = buf[26] - '0';
    int funcType = buf[19] - '0';  
    struct destinationRouter DR;
    string message;

    int index = 0;
    char actualDestn;
    if(type == 1) {
        for(int i = 0; i<6;i++) {         //After 6th comma is the actual destination of the packet
            index++;
            while(buf[index]!=',')
                index++;
        }
    }
    else{
        for(int i = 0; i<8;i++) {         //After 8th comma is the actual destination of the packet
            index++;
            while(buf[index]!=',')
                index++;
        }
    }
    index++;        //Finally the buffer seeker is referring to the <ACTUALDESTN> part of the packet
    actualDestn = buf[index];


    if(actualDestn == router1.src) { //Check if packet is for our router or meant to be passed on....
        switch(funcType) {
            case 1:
            //Send back info of all my neighbors to the src with type - 0 (reply/ack)
            if(type == 1) {
                for(int i = 0; i<router1.num_neighbors; i++) {
                    char src = neighbors[i].src;
                    string src_s(1, src);
                    if(neighbors[i].src != -1){
                        message = "N" + to_string(i+1) + "-" + src_s + ",W" + to_string(i+1) + "-"
                        + to_string(neighbors[i].weight)+",P-"+to_string(neighbors[i].port);
                        //cout<<"\t\t"<<message<<endl;
                        DR.destn = originalsrc;
                        DR.port = port_i;
                        packandsend(&DR, funcType, 0, message);     //Replying with my router's neighbor info
                    }
                }
            }else if(type == 0){
                int w,index = -1;
                int flag = 0;
                int port_y;
                char y;
                for(int i = 0; i<6;i++) {         //Data after 6th : contains the message containing neighbor info
                    index++;
                    while(buf[index]!=':')
                        index++;
                }
                index+=4;
                y = buf[index];
                index+=5;
                w = buf[index] - '0';
                index+=4;
                char yport[5];
                for(int i = 0; i<5;i++)
                    yport[i] = buf[index+i];
                port_y = atoi(yport);
                
                //Check if the vertex is already discovered. If not, add it to the list of DISCOVERED ROUTERS
                if (y == router1.src)
                    flag = 1;

                if(flag == 0) {
                    for(int i = 0; i<router1.num_neighbors ;i++) {
                        if(y == neighbors[i].src)
                            flag = 1;
                    }
                }
                if(flag == 0){
                    for(int i = 0; i<discRouters.size();i++)
                        if(discRouters[i]->src == y)
                            flag = 1;
                }
                //If flag = 0, it means the node sending this DV is new!
                if(flag == 0) {
                    discoveredRouters *node = (struct discoveredRouters*) malloc(sizeof(struct discoveredRouters));
                    node->src = y;
                    node->port = port_i;
                    discRouters.push_back(node);
                    cout<<"\n\n\t---------------------------------";
                    cout<<"\n\t| Discovered new node: "<<node->src<<"\t|";
                    cout<<"\n\t---------------------------------\n";
                    DR.destn = y;
                    DR.port = port_y;
                    packandsend(&DR, funcType, 1);  //Ask the newly discovered node for its neighbors.
                }

                insertEdge(maingraph, originalsrc, y, w);
                BellmanFord(maingraph, (int)router1.src);
            }
            break;

            case 2:
            break;

            case 3:
            int index, ind1, ind2;
            for(int i = 0; i<6; i++) {
                index++;
                while(buf[index] != ',')
                    index++;
            }
            index--;
            cout<<"\n-----------------------------------------------------------------------\n";
            cout<<"\n\t\tMessage from " << buf[index] << ": \n\t\t"<<buf<<"\n\n";  
            cout<<"\n-----------------------------------------------------------------------\n";
            //printMenu();
            break;
        }
    } else {        //Passing on the packet...
        int flag = 0;
        int ind1, ind2;
        ind1 = 0;
        for(int i = 0; i<6; i++) {
            ind1++;
            while(buf[ind1] != ':')
                ind1++;
        }
        ind1++;
        ind2 = index-1;

        string tempMessage(buf+ind1, buf+ind2);
        for(int i = 0; i<router1.num_neighbors;i++) {
            if(neighbors[i].src == actualDestn) {
                DR.destn = actualDestn;
                DR.port = neighbors[i].port;
                flag = 1;
                packandsend(&DR, 3, 1, tempMessage);
                cout<<"\nParser: Message sent to neighbor "<<actualDestn<<"\n";
                break;
            }
        }

        char prevNode;
        if(flag == 1)
            goto parserSkip;

        if(flag == 0) {  //Router is not a neighbor, check if it exists in discovered routers
            for(int i = 0; i<MAX_ROUTERS; i++) {
                if(dvinfo[i].node == actualDestn) {
                    prevNode = dvinfo[i].nextNode;
                    flag = 1;
                }
            }
        }


        if(flag == 1) {      //Router exists in discoverd routers, now find the neighbor to which we need to transmit packet
            for(int i = 0; i<router1.num_neighbors; i++) {
                if(prevNode == neighbors[i].src) {
                    flag = 0;       //NEIGHBOR FOUND!
                    DR.destn = prevNode;
                    DR.port = neighbors[i].port;
                    packandsend(&DR, 3, 1, tempMessage, NULL, 1, actualDestn);
                    goto parserSkip;
                }
            }
            while(flag){
                prevNode = dvinfo[prevNode%65].nextNode;
                for(int i = 0; i<router1.num_neighbors; i++) {
                    if(prevNode == neighbors[i].src) {
                        DR.destn = prevNode;
                        DR.port = neighbors[i].port;
                        packandsend(&DR, 3, 1, tempMessage, NULL, 1, actualDestn);
                        flag = 0;       //NEIGHBOR FOUND!
                        goto parserSkip;
                    }
                }
            }
            cout<<"\nMESSENGER: Packet forwarded to "<<prevNode << " for Router " << actualDestn<<"\n";
        } else cout<<"\nMESSENGER: Router not found on network. :(\n";
    }
    parserSkip:
    printMenu();
}

void BellmanFord(struct graph* g, int src)    
{
    int V = g->V;
    int E = g->E;
    
    // Step 1: Initialize distances from src to all other vertices
    // as INFINITE
    for (int i = 0; i < MAX_ROUTERS; i++)
    {
        dvinfo[i].node = (char) (i+65);
        dvinfo[i].shortestDist = 10000;
        dvinfo[i].nextNode = -1;
    }
    
    //Assuming A is first vertex and so on...
    dvinfo[src%65].shortestDist = 0;

    // Step 2: Relax all edges |V| - 1 times.
    for (int i = 1; i <= V-1; i++)
    {
        for (int j = 0; j < E; j++)
        {
            int u = g->edges[j]->v1;
            int v = g->edges[j]->v2;
            int weight = g->edges[j]->weight;
            //if (dist[u-65] != 10000 && dist[u-65] + weight < dist[v-65])
            //    dist[v-65] = dist[u-65] + weight;
            if(dvinfo[u%65].shortestDist != 10000 && dvinfo[u%65].shortestDist + weight < dvinfo[v%65].shortestDist) {
                dvinfo[v%65].shortestDist = dvinfo[u%65].shortestDist + weight;
                dvinfo[v%65].nextNode = dvinfo[u%65].node;
            }
        }
    } 
    cout<<"\nRouting table regenerated.\n";
    cout<<"\n----------------------------------------------------------------------------------\n";
    writeDVInfo();
    return;
}

void printMenu() {
    switch(menuStage) {
        case 1:
        cout<<"\n\nMESSENGER: Enter the the destination (enter " << router1.src << " for router functions): ";
        break;

        case 2:
        cout<<"\nMESSENGER: 1 - Initialize Nodes, 2 - Shutdown, 3 - View Routing Table\t Enter choice: ";
        break;
        
        case 3:
        cout<<"\nMESSENGER: Enter message for router: ";
        break;
    }
}

void writeDVInfo() {
    string src(1, router1.src);
    string fileName = src+"_table.txt";
    ofstream newFile(fileName);
    if(newFile.is_open()) {
        newFile<<"ROUTER SHORTEST_DISTANCE PREV_NODE\n";
        for(int i = 0; i<MAX_ROUTERS; i++){
            if(dvinfo[i].node!=-1) {
                if(dvinfo[i].node == router1.src)
                    newFile << dvinfo[i].node<<"\t\t\t"<< dvinfo[i].shortestDist << "\t\t\t\t*\n";
                else
                    newFile << dvinfo[i].node<<"\t\t\t"<< dvinfo[i].shortestDist << "\t\t\t\t" << dvinfo[i].nextNode << "\n";
            }
        }
    }
    else {
        cout<<"\n-----------------------------------------------------------------------------\nFILE WRITER: Couldn't open file";
        cout<<"\n-----------------------------------------------------------------------------\n";
    }
}