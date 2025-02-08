/*
Author: Bantwal Vaibhav Mallya
Class: ECE 6122 A
Last Date Modified: 25th November 2023

Description:
This file contains the code for the server that handles multiple clients  
 
*/


#include <iostream>
#include <cstdlib>
#include <cstring>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <mutex>
#include <algorithm>
#include <csignal>
#include <vector>
#include <arpa/inet.h>
using namespace std;

//semaphores to prevent race conditions
sem_t x, y;

//thread id 
pthread_t tid;

//thread for handling user input on the server side 
pthread_t serverInputThread[100];

//thread for reading messages from client 
pthread_t readClientMsgsThread[100];
int readercount = 0;
int newSocket;

//mutex to send message from server to client that sent message type 201
std::mutex serverToClientMutex;

//mutex to send message from server to other clients that didn't send message type 77
std::mutex serverToOtherClientsMutex;

//mutex to take care of user input on the server side 
std::mutex userInputMutex;

//structure of message received 
struct tcpMessage{
  unsigned char nVersion;
  unsigned char nType;
  unsigned short nMsgLen;
  char chMsg[1000];
};


//struct to store client information
struct clientAttributes{
  int socketNumber;
  uint16_t portNumber;
  sockaddr_in clientAddress;
};


//vector of type clientAttributes to maintain a list of connected clients 
std::vector<clientAttributes> connectedClients;

//global variable to keep track of messages from client to server 
tcpMessage clientMessage;

//variable to store last message 
char latestMessage[1000];

//string input to server
std::string inputToServer;


void* readClientMessage(void* args){

  sockaddr_in clientAddress;
  socklen_t addressLen = sizeof(clientAddress);
  getpeername(newSocket, (struct sockaddr*)&clientAddress, &addressLen);
  uint16_t portNumber = ntohs(clientAddress.sin_port);
  {
    std::lock_guard<std::mutex>lock(serverToOtherClientsMutex);
    //maintaining a list of connected clients 
    connectedClients.push_back({newSocket, portNumber, clientAddress});
  }
  //sem_wait(&x);
  sem_wait(&x);
  while(recv(newSocket, &clientMessage, sizeof(clientMessage), 0)>0){
    
    if(inputToServer == "exit"){
      break;
    }
    
    std::lock_guard<std::mutex>lock(serverToClientMutex);
    int version = static_cast<int>(clientMessage.nVersion);
    int ntype = static_cast<int>(clientMessage.nType);
    int nmsglen = static_cast<int>(clientMessage.nMsgLen);
 

    //saving the latest message received by server
    strcpy(latestMessage, clientMessage.chMsg);
    
    sem_post(&x);

    sem_wait(&x);

    tcpMessage serverToClientMessage;
    serverToClientMessage.nType = clientMessage.nType;
    strcpy(serverToClientMessage.chMsg,clientMessage.chMsg);
    int length = strlen(serverToClientMessage.chMsg);


    //message type 201 is reversed and sent back to the same client 
    if(ntype == 201){
      //code for reversing the message 
      for(int i=0; i<length/2;i++){
        char temp = serverToClientMessage.chMsg[i];
        serverToClientMessage.chMsg[i] = serverToClientMessage.chMsg[length-i-1];
        serverToClientMessage.chMsg[length-i-1] = temp;
      }
      send(newSocket, &serverToClientMessage, sizeof(serverToClientMessage),0);
    }
   
    sem_post(&x);
    
    sem_wait(&y);

    //send messages of type 77 to other clients 
  
    if(ntype == 77){ 
      std::lock_guard<std::mutex>lock(serverToOtherClientsMutex);
       
      for(auto client: connectedClients){
        if(client.portNumber != portNumber){
        //
          send(client.socketNumber, &clientMessage, sizeof(clientMessage),0);
        }
      }
    }
    sem_post(&y);

    sem_wait(&x);
    if(version != 102){
      cout<<"\nMessage ignored";
      continue;
    }
    sem_post(&x);
    
   
  }
  pthread_exit(NULL);
  return nullptr;

}

//closing connected clients after request for exit
void closeConnectedClients(){
  std::lock_guard<std::mutex>lock(userInputMutex);
  for(const auto &connectedClient : connectedClients){
    close(connectedClient.socketNumber);
  }  
  connectedClients.clear();

}

void* userInput(void* args){
  
  while(true){
    cout<<"\nPlease enter command: ";
    std::getline(std::cin, inputToServer);
    
    if(inputToServer == "exit"){
      closeConnectedClients();
      cout<<"\nConnection closed";
      cout<<"\nProgram terminated";
      close(newSocket);
      break;
    }
    else if(inputToServer == "msg"){
      std::lock_guard<std::mutex>lock(userInputMutex);
      cout<<"\nLast message = "<<latestMessage;
    }
    else if(inputToServer == "clients"){
      std::lock_guard<std::mutex>lock(userInputMutex);
      cout<<"\nNumber of clients: "<<connectedClients.size();
      for(const auto &connectedClient : connectedClients){
        char clientIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(connectedClient.clientAddress.sin_addr),clientIP, INET_ADDRSTRLEN);
        cout<<"\nIP Address: "<<clientIP<<" | Port: "<<connectedClient.portNumber;
      }
    }
    
    
  }
  pthread_exit(NULL);
  return nullptr;
}

int main(int argc, char *argv[]) {

    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << "<serverPort>" << std::endl;
        return 1;
    }

    
    const int serverPort = std::atoi(argv[1]);
    
    struct sockaddr_in serverAddr;
    struct sockaddr_storage serverStorage;

    
    int serverSocket;


    socklen_t addr_size;
    sem_init(&x, 0, 1);
    sem_init(&y, 0, 1);

    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(serverPort);

    bind(serverSocket, reinterpret_cast<struct sockaddr*>(&serverAddr), sizeof(serverAddr));

    if (listen(serverSocket, 50) == 0)
        std::cout << "Listening" << std::endl;
    else
        std::cout << "Error" << std::endl;

    pthread_t tid[60];

    int i = 0;

    while (true) {
        addr_size = sizeof(serverStorage);

        //newSocket is the client socket
        newSocket = accept(serverSocket, reinterpret_cast<struct sockaddr*>(&serverStorage), &addr_size);
        
        //creating threads to read client messages and server inputs 
        if(pthread_create(&readClientMsgsThread[i++], nullptr, readClientMessage, &newSocket) != 0){
	  cerr<<"\nFailed to create read client messages threads";
	}
        
        if(pthread_create(&serverInputThread[i++], nullptr, userInput, &newSocket)!=0){
          cerr<<"\nFailed to create threads to read server inputs";
        }
        
        
        if (i >= 50) {
            i = 0;

            while (i < 50) {
                pthread_join(readClientMsgsThread[i++], nullptr);
                pthread_join(serverInputThread[i++], nullptr);
            }

            i = 0;
        }

        if (inputToServer == "exit"){
          close(serverSocket);
          closeConnectedClients();
          break;
        }
 
    }
    close(serverSocket);

    return 0;
}

