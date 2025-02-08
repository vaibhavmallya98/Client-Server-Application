/*
Author: Bantwal Vaibhav Mallya
Class: ECE 6122 A
Last Date Modified: 25th November 2023

Description:
This file contains the code for multiple clients that send messages to a server
 
*/

#include <iostream>
#include <cstdlib>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include<string>
#include<sstream>
#include<mutex>
#include<atomic>
using namespace std;

//flag to receive the reversed message from server 
std::atomic<bool>receiveServerMsgs(false);

//flag to receive message from server to other clients 
std::atomic<bool>deliverServerMsgsToOtherClients(false);

//mutexes for preventing race conditions 

std::mutex serverToClientMutex;
std::mutex serverToClientFlagMutex;

std::mutex serverToOtherClientsMutex;
std::mutex serverToOtherClientsFlagMutex;

//message structure 

struct tcpMessage{
  unsigned char nVersion;
  unsigned char nType;
  unsigned short nMsgLen;
  char chMsg[1000];
};

tcpMessage clientMessage;

int networkSocket;

char *serverIp;
int serverPort;

//to keep track of receive message from server flag 
void receiveMsgsFromServer(tcpMessage &message){
  //std::lock_guard<std::mutex>lock(serverToClientFlagMutex);
  int ntype = static_cast<int>(message.nType);
  int version = static_cast<int>(message.nVersion);
  if(ntype == 201){
    std::lock_guard<std::mutex>lock(serverToClientFlagMutex);
    receiveServerMsgs.store(true, std::memory_order_relaxed);
    cout<<"\nReceive messages flag set to true ";
  }
  else{
    receiveServerMsgs.store(false, std::memory_order_relaxed);
  }

}

//to keep track of flag to send messages to other clients 
void sendMsgsFromServerToOtherClients(tcpMessage &message){
  int ntype = static_cast<int>(message.nType);
  if(ntype == 77){
    std::lock_guard<std::mutex>lock(serverToOtherClientsFlagMutex);
    deliverServerMsgsToOtherClients.store(true, std::memory_order_relaxed);
  }
  else{
    deliverServerMsgsToOtherClients.store(false, std::memory_order_relaxed);
  }

}

//function to send messages from client to server 
void* clientToServerThread(void* args) {

    struct sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(serverPort);
    inet_pton(AF_INET, serverIp, &(serverAddress.sin_addr));

    int connectionStatus = connect(networkSocket, reinterpret_cast<struct sockaddr*>(&serverAddress), sizeof(serverAddress));

    if (connectionStatus < 0) {
        cerr << "Error\n";
        return nullptr;
    }

    cout << "\nConnection established";

    send(networkSocket, &clientMessage, sizeof(clientMessage), 0);
    receiveMsgsFromServer(clientMessage);
    sendMsgsFromServerToOtherClients(clientMessage);

    //close(networkSocket);
    pthread_exit(nullptr);

    return nullptr;
}

//this thread receives reversed  message from server if ntype = 201
void* serverToClientThread(void* args){
  tcpMessage serverMessage;
 
  std::lock_guard<std::mutex> lock(serverToClientMutex);
  if(receiveServerMsgs){
    cout<<"\nServer is reversing the message!!";
    recv(networkSocket, &serverMessage, sizeof(serverMessage),0);
    std::cout<<"\nReceived Msg Type: "<<(int) serverMessage.nType
	       <<"; Msg: "<<serverMessage.chMsg;
  }
  return nullptr;
}


//this thread sends messages to other clients 
void* serverToOtherClientsThread(void* args){
  tcpMessage serverToOtherClientsMessage;
  cout<<"\nServer is sending messages to other connected clients";
  if(deliverServerMsgsToOtherClients){
    //std::lock_guard<std::mutex> lock(serverToOtherClientsMutex);
    recv(networkSocket, &serverToOtherClientsMessage, sizeof(serverToOtherClientsMessage),0);
        std::cout<<"\nReceived Msg Type: "<<(int) serverToOtherClientsMessage.nType
             <<"; Msg: "<<serverToOtherClientsMessage.chMsg;
    
  }

  return nullptr;
 
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <serverIp> <serverPort>" << std::endl;
        return 1;
    }

    serverIp = argv[1];
    serverPort = std::atoi(argv[2]);

    
    if (strcmp(serverIp, "localhost") == 0) {
        serverIp = "127.0.0.1";
    }

    while(true){
      cout << "\nPlease enter command: ";

      string clientInput;
      getline(cin,clientInput);

      pthread_t tidClientToServer;
      pthread_t tidServerToClient;
      pthread_t tidServerToOtherClients;

      istringstream clientStringStream(clientInput);
    
      string firstElement; //element before space
      string restElements; //elements after space

      networkSocket = socket(AF_INET, SOCK_STREAM, 0);

      //splitting the input string 
      getline(clientStringStream,firstElement,' ');
      getline(clientStringStream,restElements);


      //default version number 
      int version = 102;
      clientMessage.nVersion = static_cast<unsigned char>(version);

      
      if(firstElement == "v"){
        int restInteger = stoi(restElements);
        clientMessage.nVersion = static_cast<unsigned char>(restInteger); 
      }

      //to handle string after input is specified as t 
      else if(firstElement == "t"){
        istringstream stringStreamSecondSpace(restElements);
        string firstElementBeforeSecondSpace;
        string elementsAfterSecondSpace;

        getline(stringStreamSecondSpace, firstElementBeforeSecondSpace, ' ');
        getline(stringStreamSecondSpace, elementsAfterSecondSpace);

        int firstInt = stoi(firstElementBeforeSecondSpace);
        clientMessage.nType = static_cast<unsigned char>(firstInt);
        strcpy(clientMessage.chMsg,elementsAfterSecondSpace.c_str());
        clientMessage.nMsgLen = strlen(clientMessage.chMsg);
      }
      else if(firstElement == "q"){
      
        close(networkSocket);
        cout<<"\n Socket closed";
        cout<<"\n Program terminated";
	break;
      }
      else{
        cout<<"\nInvalid input";
      }
    

      pthread_create(&tidClientToServer, nullptr, clientToServerThread, &networkSocket);
      pthread_join(tidClientToServer, nullptr);

      //if nType = 201 create separate thread
      if(receiveServerMsgs){
	pthread_create(&tidServerToClient, nullptr, serverToClientThread, &networkSocket);
        pthread_join(tidServerToClient, nullptr);
      }
     
      //if nType = 77 create separate thread
      if(deliverServerMsgsToOtherClients){
        pthread_create(&tidServerToOtherClients, nullptr, serverToOtherClientsThread, &networkSocket);
        pthread_join(tidServerToOtherClients, nullptr);
      }
      
      
    }
   
    return 0;
}

