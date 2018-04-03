//
//  main.cpp
//  myRouter
//
//  Created by Peter Rymsza on 3/23/18.
//  Copyright © 2018 Peter Rymsza. All rights reserved.
//

#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <fstream>
#include <string.h>

using namespace std;

int main(int argc, char * argv[]) {
    char routerNames[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J'};
    int routerPortNums[10] = {10000, 10001, 10002, 10003, 10004, 10005, 10006, 10007, 10008, 10009};
    int myRtrNum;
    char myRtrName;
    int myRtrPort;
    int neighborRouters[10] = {0};
    int neighborCosts[10];
    int dvTable[10][10];
    int sourceRouter;
    int routingTable[10][3];
    //int liveRouters[10] = {0};
    int i = 0;
    int j = 0;

    //INITIALIZATION
    //Set router name (string)
    myRtrName = *argv[1];
    //Find way to make it uppercase
    //Set router number (int) (good for array/table usage)
    while (routerNames[i] != myRtrName) {
        i++;
    }
    myRtrNum = i;
    //Initialize neighbor path costs. 100 ~ infinity
    for (i=0; i<10; i++) {
        neighborCosts[i] = 100;
    }
    neighborCosts[myRtrNum] = 0;
    //Read in neighbor routers from file. Update neighbor array and link cost array.
    string port;
    string cost;
    string linkInfo;
    ifstream rtrTopFile ("routerTopology.txt");
    if (rtrTopFile.is_open()) {
        while (getline(rtrTopFile, linkInfo)) {
            if (linkInfo[0] == myRtrName) {
                port = "";
                cost = "";
                i = 0;
                j = 0;
                while (linkInfo[i] != ',') {
                    i++;
                }
                i++;
                while (linkInfo[i] != routerNames[j]) {
                    j++;
                }
                neighborRouters[j] = 1;
                while (linkInfo[i] != ',') {
                    i++;
                }
                i++;
                while (linkInfo[i] != ',') {
                    port = port + linkInfo[i];
                    i++;
                }
                routerPortNums[j] = stoi(port);
                i++;
                while (linkInfo[i] != '\n') {
                    cost = cost + linkInfo[i];
                    i++;
                }
                neighborCosts[j] = stoi(cost);
            }
        }
        rtrTopFile.close();
    }
    //Set up dvTable and routing table
    for (i=0; i<10; i++) {
        for (j=0; j<10; j++) {
            if (i == myRtrNum) {
                dvTable[i][j] = neighborCosts[j];
            }
            else {
                dvTable[i][j] = 100;
            }
        }
        if (neighborRouters[i] == 1 || i == myRtrNum) {
            routingTable[i][0] = neighborCosts[i];
            routingTable[i][1] = i;
            routingTable[i][2] = routerPortNums[i];
        }
        else {
            routingTable[i][0] = 100;
            routingTable[i][1] = 100;
            routingTable[i][2] = 100;
        }
    }
    
    //RECEIVE packet and interpret
    int pos;
    int srcNum;
    //Example string that would be extracted from received packet
    string packet = "GROUP1ROUTING/1.0\nSRC: A\nDST: F\nTYP: C\n-A5,B21,C8,D4,E27,F6,G100,H100,I100,";
    if (packet.find("GROUP1ROUTING/1.0") != std::string::npos) {
        pos = packet.find("TYP: ") + 5;
        //Is control type
        if (packet[pos] == 'C') {
            pos = packet.find("DST: ") + 5;
            //Sent to this router
            if (packet[pos] == myRtrName) {
                //Find source number
                pos = packet.find("SRC: ") + 5;
                i = 0;
                while (routerNames[i] != packet[pos]) {
                    i++;
                }
                srcNum = i;
                //Update dv table
                pos = packet.find("-") + 1;
                while (packet[pos] != '\0') {
                    j = 0;
                    while (packet[pos] != routerNames[j]) {
                        j++;
                    }
                    pos++;
                    while (!strncmp(packet[pos], ",")) {
                        cost = cost + packet[pos];
                        pos++;
                    }
                    if (cost == "100") {
                        for (i=0; i<10; i++) {
                            dvTable[i][j] = stoi(cost);
                        }
                    }
                    else {
                        dvTable[srcNum][j] = stoi(cost);
                    }
                    pos++;
                }
                //Then run Bellman-Ford
            }
        }
        //Is data type
        else if (packet[pos] == 'D') {
            //For data packets, print timestamp, source router, destination router, port received from,
            //port forwarded to,
            //string printout;
            //printout = "timestamp\n"
            //pos = packet.find("SRC: ") + 5;
            //printout = printout + "Source Router: " + packet[pos];
            //pos = packet.find("DST: ") + 5;
            //printout = ptintout + "\nDestination Router: " + packet[pos] + "\nPort Received From: "
            //+ itos(receivedPortNum)[take note when packet is received];
            //j = 0;
            //while (packet[pos] != routerNames[j]) {
            //    j++;
            //}
            //printout = printout + "\nPort Forwarded To: " + itos(routingTable[j][2]);
            //if (packet[pos] == routerName) {
            //  pos = packet.find("-") + 1;
            //  while (packet[pos] != '\0') {
            //      print(packet[pos]);
            //  }
            //}
        }
    }
    else {
        //print "Timestamp. Received unintelligible packet"
    }
    
    //Run Bellman-Ford
    int lowestCost;
    int nextHopNum;
    bool sendDV = false;
    bool printNewRoutingTable = false;
    bool printedOldRoutingTable = false;
    for (i=0; i<10; i++) {
        lowestCost = 100;
        nextHopNum = 10;
        for (j=9; j>=0; j--) {
            if (neighborCosts[j] + dvTable[j][i] <= lowestCost) {
                lowestCost = neighborCosts[j] + dvTable[j][i];
                nextHopNum = j;
            }
        }
        if (lowestCost != dvTable[myRtrNum][i]) {
            sendDV = true;
        }
        dvTable[myRtrNum][i] = lowestCost;
        if (!printedOldRoutingTable && (lowestCost != routingTable[i][0] || nextHopNum != routingTable[i][1])) {
            //print timestamp, old routing table, DV that caused change
            //string routingTableText = "Destination    Cost    Outgoing UDP Port    Destination UDP Port\n";
            for (i=0; i<10; i++) {
                if (dvTable[myRtrName][i] < 100 && i != myRtrNum) {
                    //routingTableText = routingTableText + routerNames[i] + "              "
                    //+ itos(routingTable[i][0]) + "       " + itos(myRtrPort) + " (Router "
                    //+ routerName + ")     " + itos(routingTable[i][2]) + " (Router "
                    //+ routerNames[routingTable[i][1]] + ")    \n"
                }
            }
            //string receivedDV = "\nDV from Router " + routerNames[srcNum] + ":\nDestination: "
            //pos = packet.find("-") + 1;
            //while(packet[pos] != "\0") {
            //  receivedDV = receivedDV + packet[pos] + "   ";
            //  while (!strncmp(packet[pos], ",")) {
            //      pos++;
            //  }
            //  pos++;
            //}
            //receivedDV = receivedDV + "\nCost:        ";
            //pos = packet.find("-") + 1;
            //while(packet[pos] != "\0") {
            //  pos++;
            //  while (!strncmp(packet(pos), ",")) {
            //      receivedDV = receivedDV + packet(pos);
            //      pos++;
            //  }
            //  receivedDV = reveivedDV + "   ";
            //  pos++;
            //}
            //Print receivedDV + "\n\n"
            printedOldRoutingTable = true;
            printNewRoutingTable = true;
        }
        routingTable[i][0] = lowestCost;
        routingTable[i][1] = nextHopNum;
        routingTable[i][2] = routerPortNums[nextHopNum];
    }
    if (printNewRoutingTable) {
        //string routingTableText = "Destination    Cost    Outgoing UDP Port    Destination UDP Port\n";
        for (i=0; i<10; i++) {
            if (dvTable[myRtrName][i] < 100 && 1 != myRtrNum) {
                //routingTableText = routingTableText + routerNames[i] + "              "
                //+ itos(routingTable[i][0]) + "       " + itos(myRtrPort) + " (Router "
                //+ routerName + ")     " + itos(routingTable[i][1]) + " (Router "
                //+ routerNames[routingTable[i][1]] + ")    \n"
            }
        }
        //Print routingTableNext
    }
    //Send new DV
    if (sendDV) {
        for (i=0; i<10; i++) {
            if (neighborRouters[i] == 1) {
                string DV;
                //DV = "GROUP1ROUTING/1.0\nSRC: " + routerName + "\nDST: " + routerNames[i] + "TYP: C\n-";
                for (j=0; j<10; j++) {
                    if (dvTable[myRtrNum][j] < 100) { // || j = justFailed. We'll handle this later
                        //DV = DV + routerNames[j] + itos(dvTable[routerNum][j]) + ",";
                    }
                }
                //sendPacket(DV, routerPortNums[i]);
            }
        }
    }
    //Routing output file. ("routing-output" + routerName + ".txt")
    
}
