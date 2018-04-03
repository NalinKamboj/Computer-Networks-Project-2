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

#define BUF_LEN 128
#define MAX_ROUTERS 6       //Hardcoded maximum number of routers on our network

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
void BellmanFord(struct graph*, int);

struct DV {
    char dest;
    int port;
    int shortestDist;
    char nextNode;
};
vector<struct DV*> dvData;

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

void packandsend(struct destinationRouter *destnR, int funcType, int type = 1, string message = "", struct edge *ed = NULL) {
    int sendLen = sizeof(struct sockaddr_in);
    int numBytes = 0;
    //string finalMessage = router1.src + message;
    string finalMessage;
    char destn = destnR->destn;
    string destn_s(1,destn);
    if(funcType == 1){
        if(type != 0)
            finalMessage = "DESTN:"+destn_s+",SRC:"+router1.src+",FUNC:1,TYPE:1,PORT:"+to_string(router1.port)+",MSG:"+message;
        else{
            finalMessage = "DESTN:"+destn_s+",SRC:"+router1.src+",FUNC:1,TYPE:0,PORT:"+to_string(router1.port)+",MSG:"+message;

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
        finalMessage = "DESTN:"+destn_s+",SRC:"+router1.src+",FUNC:3,TYPE1,PORT:"+to_string(router1.port)+",MSG:"+message;
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
    addrSend.sin_port = htons(destnR->port);
    addrSend.sin_addr.s_addr = inet_addr("127.0.0.1");        //Default for now.

    bind(sendSock, (struct sockaddr *) &addrSend, sizeof(addrSend));     //Not checking failure for now..
    //    cout<<"\n\t\t BIND FAIL";    
    if((numBytes=(sendto(sendSock, sendBuf, strlen(sendBuf), 0, (struct sockaddr *) &addrSend, sendLen))) == -1) {
        cout<<"\t\tSender: Couldn't send. :("<<endl;
        return;
    }
    cout<<"\t\tSender: Message sent "<<numBytes<<" to "<<destnR->destn<<" on port "
        << destnR->port <<" using port: "<<sendSock<<endl; 
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
                    cout<<"\t\t\tGRAPHER: Edge already exists."<<endl;
                    flag = 1;
                    if(g->edges[i]->weight != w) {
                        cout<<"\t\t\tGRAPHER: Updating weight..."<<endl;
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
    //sleep(2);
    //BellmanFord(maingraph, router1.src);
}


// A utility function used to print the solution
void printArr(int dist[])
{
    //cout<<"\nN: "<<n;
    cout<<"\n\t---------------------------------\n\t| Vertex   Distance from Source |";
    for (int i = 0; i < MAX_ROUTERS; ++i) {
        char ch = (char)(i+65);
        cout<<"\n\t| "<<ch<<"\t\t  "<<dist[i]<<"\t\t|";
    }
    cout<<"\n\t---------------------------------\n";
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
                //cout<<neighbors[router1.num_neighbors-1].src<<endl;
                //cout<<neighbors[router1.num_neighbors-1].port<<endl;
                //cout<<neighbors[router1.num_neighbors-1].weight<<endl;
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
        cout<<"\n\tConnection: Router " << router1.src<<" listening on port " << router1.port;
        //cout<<"Gaandu!";
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
    int threadNum;
    string temp;    //for storing message temporarily to pass to packandsend
    char destn;
    int type = -1;
    string message;
    threadNum = (intptr_t) threadID;
    int senderSock = -1;    //Not sending to any client initially.
    int response = 1;
    sleep(1.5);
    while(response) {
        cout<<"\n\nMessenger: Enter the the destination (enter " << router1.src << " for router functions): ";
        cin>>destn;
        //cout<<"\t"<<destn<<endl;
        struct edge *e1 = (struct edge*)malloc(sizeof(struct edge));
        struct destinationRouter destnR; 
        if(destn == router1.src) {
            int ch;
            cout<<"Messenger: 1 - Initialize Nodes, 2 - Shutdown, 3 - View Graph\t Enter choice: ";
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
                    return 0;
                case 3:
                printGraph(maingraph);
                break;
                default:
                cout<<"\nMessenger: Wrong choice entered.";
                break;
            }
        }
        else {
            cout<<"Messenger: Please enter message type (2 - Update Weight, 3 - Message): ";
            cin>>type;
            switch(type){
                case 1:
                for(int i = 0; i<MAX_ROUTERS; i++)
                {
                    destnR.destn = neighbors[i].src;
                    destnR.port = neighbors[i].port;
                    packandsend(&destnR, type);
                }
                break;
                case 2:
                cout<<"Messenger: Enter the initial node: ";
                cin.ignore();
                cin>>e1->v1;
                toupper(e1->v1);
                cout<<"Messenger: Enter the ending node: ";
                cin.ignore();
                cin>>e1->v2;
                toupper(e1->v2);
                cout<<"Messenger: Enter the new weight of the edge: ";
                cin.ignore();
                cin>>e1->weight;
                for(int i = 0; i<MAX_ROUTERS; i++) {
                    if(neighbors[i].src == destn){
                        destnR.port = neighbors[i].port;
                        destnR.destn = destn;
                        packandsend(&destnR, type, 1, "", e1);
                    } 
                }
                break;
                case 3:
                cout<<"Messenger: Enter message for "<<destn<<": ";
                cin.ignore();
                getline(cin, message);
                for(int i = 0; i<MAX_ROUTERS;i++) {
                    if(neighbors[i].src == destn) {
                        destnR.destn = destn;
                        destnR.port = neighbors[i].port;
                        packandsend(&destnR, type, 1, message);
                    }
                }
                break;
                default:
                cout<<"Messenger: Enter correct choice please. "<<endl;
                break;
            }
        }
        sleep(0.5);
        //cout<<"Messenger: You entered: \n\t\t"<<message<<endl;
        cout<<"\nMessenger: Do you want to continue? (1: yes, 0: shut down router): ";
        cin>>response;
    }
}


//GENERAL PACKET HEADER - "DESTN:<DESTN>,SRC:<SRC>,FUNC:<FUNC>,TYPE:1/0,PORT:<PORT>,MSG:<MSG>"
void packetParser(char *buf) {
    cout<<"\n\t\tMessage received!\n\t"<< buf;
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
    switch(funcType) {
        case 1:
        //Send back info of all my neighbors to the src with type - 0 (reply/ack)
        if(type == 1) {
            for(int i = 0; i<MAX_ROUTERS; i++) {
                char src = neighbors[i].src;
                string src_s(1, src);
                if(neighbors[i].src != -1){
                    message = "N" + to_string(i+1) + "-" + src_s + ",W" + to_string(i+1) + "-"
                    + to_string(neighbors[i].weight);
                    //cout<<"\t\t"<<message<<endl;
                    DR.destn = originalsrc;
                    DR.port = port_i;
                    packandsend(&DR, funcType, 0, message);     //Replying with my router's neighbor info
                }
            }
        }else if(type == 0){
            int w,index = -1;
            char x, y;
            for(int i = 0; i<6;i++) {         //Data after 6th : contains the message containing neighbor info
                index++;
                while(buf[index]!=':')
                    index++;
            }
            index+=4;
            y = buf[index];
            index+=5;
            w = buf[index] - '0';
            insertEdge(maingraph, originalsrc, y, w);
            BellmanFord(maingraph, (int)router1.src);
        }
        break;
        case 2:
        break;
        case 3:
        cout<<"\n\t\tMessage from " << originalsrc << ": \n\t\t"<<buf<<"\n"<<endl;  
        break;
    }
}

void BellmanFord(struct graph* g, int src)      //char src **
{
    int V = g->V;
    int E = g->E;
    int dist[MAX_ROUTERS];
    
    // Step 1: Initialize distances from src to all other vertices
    // as INFINITE
    for (int i = 0; i < MAX_ROUTERS; i++)
        dist[i] = 10000;
    dist[src-65] = 0;       //Assuming A is first vertex and so on...

    // Step 2: Relax all edges |V| - 1 times. A simple shortest 
    // path from src to any other vertex can have at-most |V| - 1 
    // edges
    for (int i = 1; i <= V-1; i++)
    {
        for (int j = 0; j < E; j++)
        {
            int u = g->edges[j]->v1;
            int v = g->edges[j]->v2;
            int weight = g->edges[j]->weight;
            if (dist[u-65] != 1000 && dist[u-65] + weight < dist[v-65])
                dist[v-65] = dist[u-65] + weight;
        }
    } 
    // Step 3: check for negative-weight cycles.  The above step 
    // guarantees shortest distances if graph doesn't contain 
    // negative weight cycle.  If we get a shorter path, then there
    // is a cycle.
    for (int i = 0; i < E; i++)
    {
        int u = g->edges[i]->v1;
        int v = g->edges[i]->v2;
        int weight = g->edges[i]->weight;
        if (dist[u] != 10000 && dist[u] + weight < dist[v])
            printf("Graph contains negative weight cycle");
    } 
    printArr(dist); 
    return;
}