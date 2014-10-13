/*
 * FileSharing.cpp
 *
 *  Created on: Feb 13, 2014
 *      Author: abhishek
 */

   #include <iostream>
   #include <stdio.h>
   #include <sstream>
   #include <string>
   #include <cstring>
   #include <stdlib.h>
   #include <unistd.h>
   #include <sys/types.h>
   #include <sys/socket.h>
   #include <netinet/in.h>
   #include <arpa/inet.h>
   #include <netdb.h>
   #include <fcntl.h>
   #include <errno.h>
   #include <vector>
   #include <netdb.h>
   #include <sys/time.h>
   #include <fstream>
   #include <ctime>
   using namespace std;
   #define BUFFER_SIZE 1000
   int listenerSocket;
   int status,fdmax;
   string myPortNumberString;
   int myPortNumber;
   struct sockaddr_in mySocketAddress;
   char* myIP = new char[INET6_ADDRSTRLEN];
   char myHostName[100];
   ssize_t nbytes;
   string processType;
   string addToServerListCommand ="ADD-TO-SERVER-LIST";
   string newServerListCommand  ="NEW-SERVER-LIST";
   string saveCommand = "SAVE";
   string fetchCommand= "FETCH";
   string spaceString = " ";
   fd_set master;     // master file descriptor list
   struct clientDetail{
	   char   hostName[100];
	   char   ipaddress[INET6_ADDRSTRLEN];
	   char   portNumber[10];
	   int    socketDescriptor;
	   suseconds_t    receivingStartTimeMicroseconds;
	   suseconds_t    receivingEndTimeMicroseconds;
	   time_t         receivingStartTimeSeconds;
	   time_t         receivingEndTimeSeconds;
	   unsigned long long int    receivingFileSize;
   };
   vector<clientDetail>  serverList;
   vector<clientDetail>  connectionList;
   int stringLength(char*);

   // get port, IPv4 or IPv6:
   in_port_t get_in_port(struct sockaddr *sa)
   {
       if (sa->sa_family == AF_INET) {
           return (((struct sockaddr_in*)sa)->sin_port);
       }

       return (((struct sockaddr_in6*)sa)->sin6_port);
   }

   // get sockaddr, IPv4 or IPv6:
      void *get_in_addr(struct sockaddr *sa){
         if (sa->sa_family == AF_INET) {
            return &(((struct sockaddr_in*)sa)->sin_addr);
         }

         return &(((struct sockaddr_in6*)sa)->sin6_addr);
      }


   //Converts String to Integers
   bool stringToInt(string str, int* result){
         stringstream ss(str);
         ss.imbue(locale::classic());
         ss >> *result;
         return !ss.fail() && ss.eof();
  }

   int getPortFromSocket(int socketDescriptor){

	   struct sockaddr_in socketAddress;
	   socklen_t len;
	   len = sizeof socketAddress;
	   if(getsockname(socketDescriptor,(struct sockaddr*)&socketAddress,&len) == -1){
	   	   fprintf(stdout,"couldnot get socket details\n");
	   	   fflush(stdout);
	   	   return -1;
	   }

	   return(ntohs(socketAddress.sin_port));
   }

   int getHostName(char* ipAddress, const char* portNumberString,char* hostName){

	   struct sockaddr_in socketAddress;
	   int portNumber;
	   //struct hostent* host=NULL;
	   memset(&socketAddress, 0, sizeof(socketAddress));
	   socketAddress.sin_family = AF_INET;
	   socketAddress.sin_port = htons(stringToInt(portNumberString,&portNumber));
	   socketAddress.sin_addr.s_addr = inet_addr(ipAddress);
	   if (getnameinfo((struct sockaddr*) &socketAddress, sizeof(struct sockaddr),hostName,100,NULL,0,0)!=0){
		   fprintf(stdout,"couldnot get the host name\n");
		   fflush(stdout);
		   return -1;
	   }
	   //host = gethostbyaddr((struct sockaddr*)&socketAddress,sizeof(socketAddress),AF_INET);
	   //strcpy(hostName,host->h_nlame);
	   return 0;
   }

   int getMySocketAddress(){

         struct sockaddr_in dnsSocketAddress;
	     socklen_t len;
	     int s;
	     if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1){
	       fprintf(stdout,"Unable to create a socket\n");
	       fflush(stdout);
	       return -1;
	     }

	     memset(&mySocketAddress, 0, sizeof(mySocketAddress));
	     memset(&dnsSocketAddress, 0, sizeof(dnsSocketAddress));

	     //"connect" google's DNS server at 8.8.8.8 , port 4567
	     dnsSocketAddress.sin_family = AF_INET;
	     dnsSocketAddress.sin_port = htons(4567);
	     dnsSocketAddress.sin_addr.s_addr = inet_addr("8.8.8.8");
	     if(connect(s,(struct sockaddr*)&dnsSocketAddress,sizeof dnsSocketAddress) == -1){
	    	 fprintf(stdout,"couldnot connect to the Google DNS server\n");
	    	 fflush(stdout);
	    	 return -1;
	     }
         len = sizeof mySocketAddress;
	     if(getsockname(s,(struct sockaddr*)&mySocketAddress,&len) == -1){
	    	 fprintf(stdout,"couldnot get socket details\n");
	    	 fflush(stdout);
	    	 return -1;
	     }

	     //getting myIP
	     myIP=inet_ntoa(mySocketAddress.sin_addr);
	    // printf("local addr %s:%u\n",myIP,ntohs(mySocketAddress.sin_port));

	     //getting myPort
	     mySocketAddress.sin_port = htons(myPortNumber);
	    // printf("local addr %s:%u\n",inet_ntoa(mySocketAddress.sin_addr),ntohs(mySocketAddress.sin_port));

	     //getting myHost
	    // myHost = gethostbyaddr((struct sockaddr*)&mySocketAddress,sizeof(mySocketAddress),AF_INET);
	     //gethostname(myHostName,100);
	     getHostName(myIP,myPortNumberString.c_str(),myHostName);

	     close(s);
	     return 0;
   }

   int writeToSocket(int socket,char* packet, int length){

       int writefdmax;
       struct timeval tv;
       fd_set write_fds;
   	   //Clearing the sets
   	   FD_ZERO(&write_fds);

   	   // adding the socket to the writing set
   	   FD_SET(socket, &write_fds);
   	   //Adding the time delay
   	   tv.tv_sec = 60;
   	   tv.tv_usec = 0;

   	   writefdmax = socket;
   	   if(select(writefdmax+1,NULL,&write_fds, NULL, &tv)==-1){
   		   fprintf(stdout,"[error] : select API failure\n");
   		   fflush(stdout);
   		   return -1;
   	   }

   	   if(FD_ISSET(socket, &write_fds)) {
   		   if (send(socket,packet,length,0)==-1){
   		   	   fprintf(stdout, "Unable to send data. Send command error\n");
   		   	   fprintf(stdout, "[error] %s  \n" ,strerror(errno));
   		   	   fflush(stdout);
   		   	   return -1;
   		   }
   	   }
   	   else{
   		   fprintf(stdout,"[error] : Time out .cannot write to the socket. Waited for 3 seconds\n");
   		   fflush(stdout);
   		   return -1;
   	   }

   	   //fprintf(stdout,"write select call is completed.packet sent to socket %d \n",socket);
   	   return 0;
      }


   int displayConnectionList(){

      int i;
      fprintf(stdout,"Connection List Begins.....\n");
      fprintf(stdout,"Sr.NO.    HostName                           IPAddress                     Port\n");
      for(i=0;i<connectionList.size();i++){
         fprintf(stdout,"%-10d%-35s%-30s%-8s\n",i+ 1,connectionList[i].hostName,connectionList[i].ipaddress,connectionList[i].portNumber);
      }
      fflush(stdout);
      return 0;
   }

   void displayServerList(){

   	   int i;
   	   fprintf(stdout,"Server List\n");
   	   fprintf(stdout,"Sr.NO.    HostName                           IPAddress                     Port\n");
   	   for(i=0;i<serverList.size();i++){
   		   fprintf(stdout,"%-10d%-35s%-30s%-8s\n",i + 1,serverList[i].hostName,serverList[i].ipaddress,serverList[i].portNumber);
   	   }
   	   fflush(stdout);
      }

   int parseCommand(string data,vector<string>* userInput){

   	   int counter=0;
   	   char *temp=NULL;
   	   int tempCount=0;
   	   while(counter<data.size()){
   		  temp = new char[100];
          while(data[counter]!=' ' && data[counter]!='\0'){
             temp[tempCount++]=data[counter++];
          }
          temp[tempCount]='\0';
          counter++;
          tempCount=0;
          userInput->push_back(temp);
   	   }
   	   return 0;
      }


   int saveFile(char* data , int dataSize,int SocketDescriptor){

      vector<string> userInput;
      int counter=0,fieldCounter=0;
      char fileName[100];
      char chunkAttribute[2];
      char *buffer=NULL;
      int   size;
      int i;
      ofstream outputFileStream;
      int connectionNumber;
      float receivingRate;
      float receivingTime=0;
      struct timeval receivingTimeStruct;


      //Step 1 : Get the connection Number
      for(i=0;i<connectionList.size();i++){
    	  if(connectionList[i].socketDescriptor==SocketDescriptor){
    		  connectionNumber=i+1;
    	  }
      }
      //Step 1 : By pass the save command and space
      while(data[counter]!=' '){
    	  counter++;
      }
      counter++;
      //fprintf(stdout,"Step 1 : By passed the command successful\n");

      //Step 2  : Get the filename and by pass the space
      while(data[counter]!=' '){
         fileName[fieldCounter++] = data[counter++];
      }
      fileName[fieldCounter]='\0';
      counter++;
      fieldCounter=0;
      //fprintf(stdout,"Step 2 : Received the file Name:%s successful\n",fileName);

      //Step 3 : Get the chunk Attribute
      while(data[counter]!=' '){
         chunkAttribute[fieldCounter++] = data[counter++];
      }
      chunkAttribute[fieldCounter]='\0';
      counter++;
      fieldCounter=0;
      //fprintf(stdout,"Step 3 : Received the chunk attribute:%s successfully\n",chunkAttribute);

      //Step 4 : Start the timer if chunk attribute is S
      if(chunkAttribute[0]=='S' || chunkAttribute[0]=='N'){
    	  gettimeofday(&receivingTimeStruct,NULL);
    	  connectionList[connectionNumber-1].receivingStartTimeMicroseconds=receivingTimeStruct.tv_usec;
    	  connectionList[connectionNumber-1].receivingStartTimeSeconds=receivingTimeStruct.tv_sec;
    	  connectionList[connectionNumber-1].receivingEndTimeMicroseconds=0;
    	  connectionList[connectionNumber-1].receivingEndTimeSeconds=0;
    	  connectionList[connectionNumber-1].receivingFileSize=0;
    	  fprintf(stdout,"The download process for file: %s begins\n",fileName);
    	  fflush(stdout);
      }

      //Step 5 :Get the file Content and update the connectionList
      size=dataSize-counter;
      buffer = new char[size];
      while(counter<dataSize){
         buffer[fieldCounter++] = data[counter++];
      }
      connectionList[connectionNumber-1].receivingFileSize+=size;
      //fprintf(stdout,"Step 5 : Get the file content successfully\n");


      //Step 6 :Open the file in write mode and write the buffer to it and update the timers
      outputFileStream.open(fileName,ios::out  |  ios::binary | ios::app);
      if(!outputFileStream.is_open()){
         fprintf(stdout,"Unable to write into the file\n");
      	 return -1;
      }
      outputFileStream.write(buffer,size);
      flush(outputFileStream);
      //fprintf(stdout,"Step 6 : Open the file in write mode and write the buffer to it successfully\n");

      //Step 7 : close the file and delete the buffers
      if(chunkAttribute[0]=='T' || chunkAttribute[0]=='S'){
    	  gettimeofday(&receivingTimeStruct,NULL);
    	  connectionList[connectionNumber-1].receivingEndTimeMicroseconds=receivingTimeStruct.tv_usec;
    	  connectionList[connectionNumber-1].receivingEndTimeSeconds=receivingTimeStruct.tv_sec;
    	  receivingTime= (connectionList[connectionNumber-1].receivingEndTimeSeconds-connectionList[connectionNumber-1].receivingStartTimeSeconds) + ((connectionList[connectionNumber-1].receivingEndTimeMicroseconds - connectionList[connectionNumber-1].receivingStartTimeMicroseconds)/1000000.0);
    	  receivingRate= (connectionList[connectionNumber-1].receivingFileSize*8.0)/receivingTime;
    	  fprintf(stdout,"Rx %s -> %s , File Size :%lld Bytes Time Taken %.6f seconds Rx Rate %.2f bits/second \n\n",connectionList[connectionNumber-1].hostName,myHostName,connectionList[connectionNumber-1].receivingFileSize,receivingTime,receivingRate);
    	  fprintf(stdout,"The complete file :%s has been downloaded successfully\n",fileName);
    	  fflush(stdout);
      }
      outputFileStream.close();
      delete[] buffer;
      buffer=NULL;
      //fprintf(stdout,"Step 7 : close the file and delete the buffer success\n");

	  return 0;
   }

   void sendServerList(){
   	   char serverListPacket[600];
   	   strcpy(serverListPacket,"");
   	   //Creating the server List Packet
   	   //Adding the header
   	   snprintf(serverListPacket,600,"%s%s%d",newServerListCommand.c_str(),spaceString.c_str(),serverList.size());
   	   //Adding the client details
          for(int i=0;i<serverList.size();i++){
       	  strcat(serverListPacket,spaceString.c_str());
             strcat(serverListPacket,serverList[i].ipaddress);
             strcat(serverListPacket,spaceString.c_str());
             strcat(serverListPacket,serverList[i].portNumber);
         }

          strcat(serverListPacket,spaceString.c_str());     // Adding space towards the end

          //Sending server list to each client
          for(int i=0;i<serverList.size();i++){
       	   if(writeToSocket(serverList[i].socketDescriptor,serverListPacket,strlen(serverListPacket))==-1){
       		   fprintf(stdout,"Unable to send server list to the client %s .Problem in send()",serverList[i].ipaddress);
       	   }
          }
          fflush(stdout);

          //fprintf(stdout,"Server List successfully sent to all the clients");
      }

   int exitAllConnections(){

	   for(int i =connectionList.size()-1; i>=0;i--){

		  //Step 1 : close all sockets
	      close(connectionList[i].socketDescriptor);

	      //Step 2 : clear fdmax
	      if(connectionList[i].socketDescriptor==fdmax){
	    	  fdmax--;
	      }

	      //Step 3 : update the master set
	      FD_CLR(connectionList[i].socketDescriptor, &master);
	   }

	   //Step 4 : clear the connection list
	   connectionList.clear();

	   //Step 5 : clear the server list
	   serverList.clear();

	   return 0;
   }

   int terminateConnection(int socketDescriptor){

	   int i,j;
	   //Step 1 : Remove the connection from the connection list
	   for(i =0; i<connectionList.size();i++){
		   if(connectionList[i].socketDescriptor==socketDescriptor){
			  if(i==0 && processType=="c"){ //Server Connection is being asked to remove
				  exitAllConnections();
				  fprintf(stdout, "All the connections are closed\n");
				  fflush(stdout);
				  return 0;
			  }
	          connectionList.erase(connectionList.begin()+ i);
	          break;
		   }
	   }

	   //Step 2 : Display the connection List
	   displayConnectionList();

	   //Step 3 : Close the socket
	   close(socketDescriptor);

	   //Step 4 : update fdmax
	   if(fdmax==socketDescriptor){
		   fdmax--;
	   }

	   //Step 5 : remove socket from master set
	   FD_CLR(socketDescriptor, &master);

	   //Step 6 :check if it is a server
	   if(processType=="s"){

		   //Step 7 :remove from the server list
		   for(j =0; j<serverList.size();j++){
		      if(serverList[j].socketDescriptor==socketDescriptor){
		   	     serverList.erase(serverList.begin()+ j);
		   	     break;
		   	  }
		   }

		   //Step 8 : send the server list to all clients
		   sendServerList();

		   //Step 9 : display server list
		   displayServerList();
	   }
	   return -1;
   }

   int processTerminateCommand(string data){

       int connectionNumber;
       int connectionSocket;
       vector<string> userInput;

       // step 1 : parse the command
       parseCommand(data,&userInput);

       //Step 2 : check the command
       if(strcasecmp(userInput[0].c_str(),"TERMINATE")!=0){
    	   fprintf(stdout, "Invalid Command\n");
    	   return -1;
       }

       //Step 3 : check the length of the command
       if(userInput.size()!=2){
    	   fprintf(stdout, "Invalid Command\n");
    	   return -1;
       }

       //Step 3 : Convert connection from string to int
       if(stringToInt(userInput[1],&connectionNumber)==false){
    	   fprintf(stdout,"Connection Number is Invalid\n");
    	   return -1;
       }
       //fprintf(stdout,"Step 4 : Convert connection from string to int Successful . Connection no. %d\n",connectionNumber);

       //Step 5 : validate the connection
       if(connectionNumber>connectionList.size() || connectionNumber<1){
    	   fprintf(stdout,"Invalid connection\n");
    	   fflush(stdout);
    	   return -1;
       }
       //fprintf(stdout,"Step 5 :The connection no. is valid Successful.\n");

       //Step 6 :remove the connection
       connectionSocket=connectionList[connectionNumber-1].socketDescriptor;
       terminateConnection(connectionSocket);
       fprintf(stdout,"Step 6 :Remove the connection Successful.\n");
       fflush(stdout);

	   return 0;
   }


   int isUploadCommandValid(vector<string>* userInput){

	   int connectionNumber;
	   //Step 1 : check the size of userInput . If not equal to 3 declare error
	   if(userInput->size()!=3){
		   fprintf(stdout, "Invalid command syntax\n");
		   fflush(stdout);
		   return -1;
	   }
	   //fprintf(stdout, "Step 1 : The userInput size =3 Successful\n");

	   //Step 2 :Check the command
	   if(strcasecmp(userInput->at(0).c_str(),"UPLOAD")!=0){
		   fprintf(stdout, "Invalid command syntax\n");
		   fflush(stdout);
		   return -1;
	   }
	   //fprintf(stdout, "Step 2 : Upload Command Given\n");

	   //Step 3 :Check the connection
	   if(stringToInt(userInput->at(1),&connectionNumber)==-1){
		   fprintf(stdout, "Connection No. Is invalid\n");
		   fflush(stdout);
		   return -1;
	   }

	   //Step 4 : check if connection number is of server or negative or greater than no. of connection
	   if(connectionNumber <=1 || connectionNumber>connectionList.size()){
		   fprintf(stdout, "Connection No. Is invalid\n");
		   fflush(stdout);
		   return -1;
	   }
   	   return 0;
   }

   int isDownloadCommandValid(vector<string>* userInput){

   	   int connectionNumber,i;
   	   //Step 1 : check the size of userInput . If not equal to 3 declare error
   	   if(userInput->size()!=3 && userInput->size()!=5 && userInput->size()!=7){
   		   fprintf(stdout, "Invalid command syntax\n");
   		   fflush(stdout);
   		   return -1;
   	   }
   	   //fprintf(stdout, "Step 1 : The userInput size =3,5,7 Successful\n");


   	   //Step 2 :Check the command
   	   if(strcasecmp(userInput->at(0).c_str(),"DOWNLOAD")!=0){
   		   fprintf(stdout, "Invalid command syntax\n");
   		   fflush(stdout);
   		   return -1;
   	   }
   	   //fprintf(stdout, "Step 2 : Download Command Given\n");

   	   for(i=userInput->size();i>=2;i-=2){
   	      //Step 3 :Check the connection
   	      if(stringToInt(userInput->at(i-2),&connectionNumber)==-1){
   		     fprintf(stdout, "Connection No. Is invalid\n");
   		     fflush(stdout);
   		     return -1;
   	      }

   	      //Step 4 : check if connection number is of server or negative or greater than no. of connection
   	      if(connectionNumber <=1 || connectionNumber>connectionList.size()){
   		     fprintf(stdout, "Connection No. Is invalid\n");
   		     fflush(stdout);
   		     return -1;
   	      }
   	   }
      	   return 0;
      }


   int processDownloadCommand(string data){

   	   int connectionNumber;
   	   int i;
   	   char *packet=NULL;
   	   int packetSize;
   	   int uploadSocket;
   	   vector<string> userInput;
   	   string filePath;

   	   // Step 1 :Verify if it is a client
   	   if(strcasecmp(processType.c_str(),"s")==0){
   	      fprintf(stdout,"DOWNLOAD command cannot be given to a server\n");
   	      fflush(stdout);
   	   	  return -1;
   	   }
   	   //fprintf(stdout,"Step 1 :Verify if it is a client Successful\n");

   	   //Step 2 :Parse the command and get connection numbers and file name
   	   if(parseCommand(data,&userInput)==-1){
   	      fprintf(stdout,"UPLOAD Command Syntax is invalid\n");
   	      fflush(stdout);
   	   	  return -1;
   	   }
   	   //fprintf(stdout,"Step 2 :Parse the command and get the connection no. and file name Successful\n");

   	   //Step 3  :Validate the command
   	   if(isDownloadCommandValid(&userInput)==-1){
   		   fprintf(stdout,"UPLOAD Command Syntax is invalid\n");
   		   fflush(stdout);
   		   return -1;
   	   }
   	   //fprintf(stdout,"Step 3 :Validate the command successful\n");

   	   for(i=userInput.size();i>=2;i-=2){

   		  //Step 4 : Get the File Name
   		  filePath = userInput[i-1];
   		  //fprintf(stdout,"Step 4 :Getting the file name successful\n");

   		  //Step 5 : Fetch the connection numbers
   		  stringToInt(userInput[i-2],&connectionNumber);
   		  //fprintf(stdout,"Step 5 :Getting the connection number successful\n");

   		  //Step 6 : Get the socket descriptor
   		  uploadSocket=connectionList[connectionNumber-1].socketDescriptor;
   		  //fprintf(stdout,"Step 6 :Getting the socket descriptor successful\n");

   		  //Step 7 :Make the packet
   		  packetSize = fetchCommand.length() + spaceString.length() + filePath.length();
   		  packet = new char[packetSize+ 1];
   		  strcpy(packet,fetchCommand.c_str());
   		  strcat(packet,spaceString.c_str());
   		  strcat(packet,filePath.c_str());
   		  packet[packetSize]='\0';
   		  //fprintf(stdout,"Step 7 :Making the packet Successful\n");
   		  /*for(int count=0;count<packetSize;count++){
   		     fprintf(stdout,"Download packet : x[%d]=%c\n",count,packet[count]);
   		  }*/

   		  //Step 8 : Send the packet
   		  if(writeToSocket(uploadSocket,packet,packetSize)==-1){
   		     fprintf(stdout, "Unable to upload file. Send command error\n");
   		     fflush(stdout);
   		     return -1;
   		  }
   		  //fprintf(stdout,"Step 8 :Send the packet Successful\n");

   		  //Step 9 : Clear the buffers
   		  delete[] packet;
   		  packet=NULL;
   		  //fprintf(stdout,"Step 9 :Clear the buffers Successful\n");
   	   }

       userInput.clear();
   	   return 0;
      }

   int processUploadCommand(string data){

	   ifstream inputFileStream;
	   int connectionNumber;
	   string fileName;
	   string filePath;
	   unsigned long long int fileSize;
	   char fileBuffer[BUFFER_SIZE+ 1];
	   int charactersToRead,charactersRead;
	   char packet[BUFFER_SIZE+ 1];
	   int fileBufferSize;
	   int packetSize,i;
	   int uploadSocket;
	   string chunkAttribute;
	   int chunkNumber=0;
	   int saveHeaderLength=0;
	   vector<string> userInput;
	   int chunkAttributeSize=1;
	   int fileBufferCount;
	   float transmissionRate;
	   struct timeval transmissionStart;
	   struct timeval transmissionEnd;
	   float transmissionTime;

	   /*for(int j=0;j<data.length();j++){
		   fprintf(stdout," process upload command input data[%d]=%c\n",j,data[j]);
	   }*/

	   // Step 1 :Verify if it is a client
	   if(strcasecmp(processType.c_str(),"s")==0){
	      fprintf(stdout,"UPLOAD command cannot be given to a server\n");
	      fflush(stdout);
	   	  return -1;
	   }
	   //fprintf(stdout,"Step 1 :Verify if it is a client Successful\n");

	   //Step 2 :Parse the command and get connection no. and file name
	   if(parseCommand(data,&userInput)==-1){
	      fprintf(stdout,"UPLOAD Command Syntax is invalid\n");
	      fflush(stdout);
	   	  return -1;
	   }
	   //fprintf(stdout,"Step 2 :Parse the command and get the connection no. and file name Successful\n");

	   //Step 3  :Validate the command
	   if(isUploadCommandValid(&userInput)==-1){
		   fprintf(stdout,"UPLOAD Command Syntax is invalid\n");
		   fflush(stdout);
		   return -1;
	   }
	   //fprintf(stdout,"Step 3 :Validate the command successful\n");

	   //Step 4 : Fetch the connection no. and file name
	   stringToInt(userInput[1],&connectionNumber);
	   filePath= userInput[2];
	   /*for(i=0;i<filePath.length();i++){
		   fprintf(stdout,"filePath[%d] =%c\n",i,filePath[i]);
	   }*/
	   //fprintf(stdout,"Step 4 :Fetching the connection and file name successful\n");

	   //Step 5 : Get the socket descriptor
	   uploadSocket=connectionList[connectionNumber-1].socketDescriptor;
	   //fprintf(stdout,"Step 5 :Getting the socket descriptor successful\n");
	   //Step 6 : open the file
	   inputFileStream.open(filePath.c_str(),ios::in | ios::binary | ios::ate);
	   if(!inputFileStream.is_open()){
		   fprintf(stdout,"The provided file cannot be opened\n");
		   fflush(stdout);
		   return -1;
	   }
	   //fprintf(stdout,"Step 6 :check if the file can be opened successful\n");

	   //Step 7 : Derive the file Name
	   if(filePath.find("/",0)==-1){
		   fileName=filePath;
	   }
	   else{
		   fileName=filePath.substr(filePath.find_last_of("/") + 1);
	   }
	   //fprintf(stdout,"Step 7 :Deriving File Name successful filepath:%s and fileName:%s\n",filePath.c_str(),fileName.c_str());

       //Step 8 : Get the length of the file
	   inputFileStream.seekg (0, inputFileStream.end);
	   fileSize= inputFileStream.tellg();
	   //fprintf(stdout,"fileSize=%d\n",fileSize);
	   inputFileStream.seekg (0, inputFileStream.beg);
	   //fprintf(stdout,"Step 8 :Get the length of the file successful\n");

       //Step 9  : Initialize buffers
       charactersToRead=fileSize;
       //fprintf(stdout,"Step 9 :Initialize buffers Successful\n");

       //Step 10 :Read into the buffer
       saveHeaderLength=saveCommand.size() + spaceString.size() + fileName.size() + spaceString.size() + chunkAttributeSize + spaceString.size();
       /*fprintf(stdout,"saveHeaderLength=%d\n",saveHeaderLength);
       fprintf(stdout,"saveCommand=%d\n",saveCommand.size());
       fprintf(stdout,"spaceString=%d\n",spaceString.size());
       fprintf(stdout,"fileName=%d\n",fileName.size());
       fprintf(stdout,"chunkAttribute=%d\n",chunkAttribute.size());*/
       fileBufferSize=BUFFER_SIZE-saveHeaderLength;
       gettimeofday(&transmissionStart,NULL);
       fprintf(stdout,"The upload process begins\n");
       fflush(stdout);
       while(charactersToRead!=0){
       memset(fileBuffer,0,BUFFER_SIZE);
       memset(packet,0,BUFFER_SIZE);
       if(charactersToRead>fileBufferSize){
    	   //fileBuffer = new char[fileBufferSize];
    	   inputFileStream.read (fileBuffer,fileBufferSize);
    	   charactersToRead-=fileBufferSize;
    	   charactersRead=fileBufferSize;
       }
       else{
    	   //fileBuffer = new char[charactersToRead];
    	   //fprintf(stdout,"charactersToRead=%d\n",charactersToRead);
    	   inputFileStream.read (fileBuffer,charactersToRead);
    	   charactersRead=charactersToRead;
    	   //fprintf(stdout,"charactersRead=%d\n",charactersRead);
    	   charactersToRead=0;
       }
       //fprintf(stdout,"Step 10 :Read into the buffer Successful\n");

       //Step 11 : Find the chunk attribute
       if (charactersToRead==0 && chunkNumber==0){
    	   chunkAttribute="S";
       }
       else if(charactersToRead==0){
           	   chunkAttribute="T";
       }
       else if(chunkNumber==0){
    	   chunkAttribute="N";
    	   chunkNumber++;
       }
       else{
    	   chunkAttribute="A";
       }

       //Step 12 :Make the packet
       packetSize = saveHeaderLength + charactersRead ;
       //fprintf(stdout,"packetSize=%d\n",packetSize);
       //packet = new char[packetSize + 1];
       strcpy(packet,saveCommand.c_str());
       strcat(packet,spaceString.c_str());
       strcat(packet,fileName.c_str());
       strcat(packet,spaceString.c_str());
       strcat(packet,chunkAttribute.c_str());
       strcat(packet,spaceString.c_str());
       for(fileBufferCount=0;fileBufferCount<charactersRead;fileBufferCount++){
    	   packet[saveHeaderLength + fileBufferCount]=fileBuffer[fileBufferCount];
       }
       //strncat(packet,fileBuffer,charactersRead);
       //packet[packetSize]='\0';
       //fprintf(stdout,"Step 11 :Make the packet Successful\n");
       /*for(int count=0;count<packetSize;count++){
    	    fprintf(stdout,"packet : x[%d]=%c\n",count,packet[count]);
       }*/


       //Step 13 : Send the packet
       if(writeToSocket(uploadSocket,packet,packetSize)==-1){
       	     fprintf(stdout, "Unable to upload file. Send command error\n");
       	     fflush(stdout);
       	     return -1;
       }
       //fprintf(stdout,"Step 12 :Send the packet Successful\n");


       //Step 14 : Clear the buffers
       /*delete[] fileBuffer;
       fileBuffer=NULL;
       fprintf(stdout,"Step 13 :file buffer cleared Successful\n");
       delete[] packet;
       packet=NULL;
       fprintf(stdout,"Step 13 :Clear the buffers Successful\n");*/
       }
       gettimeofday(&transmissionEnd,NULL);
       transmissionTime = (transmissionEnd.tv_sec-transmissionStart.tv_sec) + ((transmissionEnd.tv_usec-transmissionStart.tv_usec)/ 1000000.0);
       transmissionRate = (fileSize * 8.0)/transmissionTime;
       fprintf(stdout,"Tx %s -> %s   file size %lld Bytes Time taken %.6f seconds Transmission Rate %.2f bits/second\n\n",myHostName,connectionList[connectionNumber-1].hostName,fileSize,transmissionTime,transmissionRate);
       fprintf(stdout,"file successfully sent\n");
       fflush(stdout);
       userInput.clear();
       inputFileStream.close();

	   return 0;
   }

   int fetchFile(char* data,int dataSize ,int socket){

	   int count=0;
	   int filePathCount=0,connectionCount=0,connectionNumber,packetLength,filePathLength;
	   char* filePath=NULL;
	   char *packet=NULL;
	   char connectionNumberString[2];

       //Step 1 : By  pass the command and space
	   while(data[count]!=' '){
		   count++;
	   }
	   count++;
	   //fprintf(stdout, "Step 1 : By  pass the command and space successful\n");

	   //Step 2 : Get the file Path
	   filePathLength=dataSize-count;
	   filePath = new char[filePathLength];
	   while(count < dataSize){
		   filePath[filePathCount++] = data[count++];
	   }
	   //fprintf(stdout,"Step 2 : Get the file Path successful\n");

	   //Step 3 : Find the connection Number
	   for(connectionCount=0;connectionCount<connectionList.size();connectionCount++){
           if(connectionList[connectionCount].socketDescriptor==socket){
        	   connectionNumber=connectionCount+1;
        	   break;
           }
	   }
	   snprintf(connectionNumberString,2,"%d",connectionNumber);
	   //fprintf(stdout,"Step 3 : Find the connection Number successful\n");

	   //Step 4 :Make the packet
	   packetLength = filePathLength + 9; //6(upload) + 1(space) + 1(connection) + 1(space)
	   packet = new char[packetLength+ 1];
	   strcpy(packet,"UPLOAD");
	   strcat(packet,spaceString.c_str());
	   strcat(packet,connectionNumberString);
	   strcat(packet,spaceString.c_str());
	   strcat(packet,filePath);
	   packet[packetLength]='\0';
	   /*for(int i=0;i<packetLength;i++){
		   fprintf(stdout,"upload packet[%d]=[%c]\n",i,packet[i]);
	   }*/
	   //fprintf(stdout,"Step 4 : Make the packet successful\n");


	   //Step 5 :Invoke processUploadCommand()
	   processUploadCommand(string(packet));
	   //fprintf(stdout,"Step 5 : Invoke processUploadCommand() successful\n");

	   //Step 6 : clear buffers
	   delete[] filePath;
	   delete[] packet;
	   filePath=NULL;
	   packet=NULL;
   	   return 0;
   }


   int  updateConnectionList(char* ipAddress, char* portNumber,int socketDescriptor){

	   struct clientDetail client;
	   char hostName[100];

	   //Step 1 : Fill the client
	   strcpy(client.ipaddress,ipAddress);
	   strcpy(client.portNumber,portNumber);
	   client.socketDescriptor=socketDescriptor;
	   getHostName(ipAddress,portNumber,hostName);
	   //gethostname(client.hostName,100);
	   strcpy(client.hostName,hostName);

	   //Step 2  : Add the new client to the connection list
	   connectionList.push_back(client);
	   return 0;
   }

   int ifExistsInServerList(char* ipAddress, char* portNumber){

	   int i=0;
	   for(i=0;i<serverList.size();i++){
          if(strcasecmp(serverList[i].ipaddress,ipAddress)==0 || strcasecmp(serverList[i].hostName,ipAddress)==0){
        	  strcpy(ipAddress,serverList[i].ipaddress);
        	  if(portNumber==NULL){
        		  return 0;
        	  }
        	  if(strcasecmp(serverList[i].portNumber,portNumber)==0){
        		  //fprintf(stdout, "The existing connection exists in the server list\n");
        		  return 0;
        	  }
          }
	   }
	   return -1;
   }

   int ifExistsInConnectionList(char* ipAddress, char* portNumber){

   	   int i=0;
   	   for(i=0;i<connectionList.size();i++){
             if(strcasecmp(connectionList[i].ipaddress,ipAddress)==0 || strcasecmp(connectionList[i].hostName,ipAddress)==0){
            	 if(portNumber==NULL){
            	    return 0;
            	 }
           	    if(strcasecmp(connectionList[i].portNumber,portNumber)==0){
           		   fprintf(stdout, "The existing connection already exists in the conection list\n");
           		   fflush(stdout);
           		   return 0;
           	  }
             }
   	   }
   	   return -1;
      }

   int storeNewConnection(struct sockaddr_storage* peerSocketStorageAddress, int peerSocketDescriptor){

	   struct sockaddr_in* peerSocketAddress=NULL;
	   char peerIPAddress[INET6_ADDRSTRLEN];
	   peerSocketAddress=(struct sockaddr_in*)peerSocketStorageAddress;
	   int i;

	   //fprintf(stdout, "Inside store new connection\n");

	   //Step 1 : get the IP address
	   strcpy(peerIPAddress,inet_ntoa(peerSocketAddress->sin_addr));
	   //fprintf(stdout, "Step 1 : get IP address successful\n");

	   //Step 2 : check if the IP address is in the server list
	   if(ifExistsInServerList(peerIPAddress,NULL)==-1){
		   fprintf(stdout, "The requested client doesnot exist in the server list. Connection cannot be completed\n");
		   fflush(stdout);
		   return -1;
	   }
	   //fprintf(stdout, "Step 2 : check if the IP address is in the server list successful\n");

	   //Step 3 : Check if the IP address already exists in the connection list
	   if(ifExistsInConnectionList(peerIPAddress,NULL)==0){
		   fprintf(stdout, "The requested Connection already exists. Connection cannot be completed\n");
		   fflush(stdout);
		   //return -1;
	   }
	   //fprintf(stdout, "Step 3 : Check if the IP address already exists in the connection list successful\n");

	   //Step 4 : check the maximum no. of connections
	   if(connectionList.size()>=4){
		   fprintf(stdout,"Maximum no. of connections already reached\n");
		   fflush(stdout);
		   terminateConnection(peerSocketDescriptor);
		   return -1;
	   }

	   //Step 4 : Fetch the port Number from the server list, update the connection List and display it.
	   for(i=0;i<serverList.size();i++){
		   if(strcasecmp(serverList[i].ipaddress,peerIPAddress)==0){
			   updateConnectionList(serverList[i].ipaddress,serverList[i].portNumber,peerSocketDescriptor);
			   fprintf(stdout, "Step 4 : The Connection has been completed. Connection list has been updated. Job Successful\n");
			   fflush(stdout);
			   displayConnectionList();
               return 0;
		   }
	   }
	   return -1;
   }


   void storeNewServerList(char* data){

	   struct clientDetail *client=NULL;
	   char* iterator = data;
	   int count=0,portNumber;
	   struct hostent* host;
	   char serverListSize[5];
	   int  serverListSizeInt,i;
	   char hostName[100];
	   struct sockaddr_in clientSocketAddress;
	   //fprintf(stdout, "Inside store new server list\n");
	   //Clear the existing server List
	   serverList.clear();

	   //By Pass the command
	   while(*iterator!=' ') {
	   		iterator++;
	   }
	   //fprintf(stdout, "By passed the new server list command\n");

	   //By Pass the space
	   iterator++;

	   //Get the size
	   while(*iterator!=' ') {
		        serverListSize[count++] = *iterator;
	   	   		iterator++;
	   	   }
	   serverListSize[count] = '\0';
	   stringToInt(serverListSize,&serverListSizeInt);
	   count=0;
	   //fprintf(stdout, "The size of new server list is %d\n",serverListSizeInt);

	   //By Pass the space
	   iterator++;

       //Get the client details
	   for(i=0; i<serverListSizeInt;i++){

		    //creating a new client
		    client = (struct clientDetail*)malloc(sizeof(struct clientDetail));
		    memset(client,0,sizeof(struct clientDetail));

		    //creating client Socket address
		    memset(&clientSocketAddress, 0, sizeof(clientSocketAddress));
		    clientSocketAddress.sin_family = AF_INET;
		    //fprintf(stdout, "inside server list loop : %d\n",i);

	        //Store the ip address
	        while(*iterator!=' ') {
	           client->ipaddress[count++]=*iterator;
	           iterator++;
	        }
	        client->ipaddress[count++]='\0';
	        count=0;
	        //fprintf(stdout, "stored the ip address:%s for loop : %d\n",client->ipaddress,i);

	        //By Pass the space
	        iterator++;

	        //Storing the port Number
	        while(*iterator!=' ') {
	        	client->portNumber[count++]=*iterator;
	        	iterator++;
	        }
	        client->portNumber[count++]='\0';
	        count=0;
	        //fprintf(stdout, "stored the port number :%s for loop : %d\n",client->portNumber,i);

	        //By Pass the space
	        iterator++;

	        //Socket Descriptor is kept -1 at client side
	        client->socketDescriptor=-1;

	        //Storing the hostName
	        //stringToInt(client->portNumber,&portNumber);
	        //clientSocketAddress.sin_port = htons(portNumber);
	        //clientSocketAddress.sin_addr.s_addr = inet_addr(client->ipaddress);
	        //host = gethostbyaddr((struct sockaddr*)&clientSocketAddress,sizeof(clientSocketAddress),AF_INET);
	        getHostName(client->ipaddress,client->portNumber,hostName);
	        //strcpy(client->hostName,host->h_name);
	        strcpy(client->hostName,hostName);
	        //fprintf(stdout, "stored the host name:%s for loop : %d\n",client->hostName,i);

	        //Adding new client detail to the server list
	        serverList.push_back(*client);
	   }

	   //fprintf(stdout, "Exiting store new server list\n");
   }


   int stringLength(char* data){
	   int length=0;
	   while(*data!='\0'){
		   length++;
		   data++;
	   }
	   return length;
   }

   void addToServerList(int socketDescriptor,char* data){
	   struct clientDetail client;
	   char* iterator = data;
	   int count=0;
	   struct sockaddr_in clientSocketAddress;
	   int portNumber;
	   char hostName[100];

	   //By Pass the command
	   while(*iterator!=' ') {
		   iterator++;
	   }
	   //fprintf(stdout,"By passed the command\n");

	   iterator++; //By Pass the space
	   //fprintf(stdout,"By passed the first space\n");

	   //Store the ip address
	   while(*iterator!=' ') {
		   client.ipaddress[count++]=*iterator;
	   	   iterator++;
	   	   }
	   client.ipaddress[count++]='\0';
	   count=0;
	   //fprintf(stdout,"stored the ip address\n");

	   iterator++; //By Pass the space
	   //fprintf(stdout,"By passed the second space\n");

	   //Storing the port Number
	   while(*iterator!=' ') {
	   		client.portNumber[count++]=*iterator;
	   	   	iterator++;
	   	    }
	   client.portNumber[count++]='\0';
	   count=0;
	   //fprintf(stdout,"stored the port number\n");

	   //Storing the socket descriptor
	   client.socketDescriptor=socketDescriptor;

       //creating client Socket address
	   stringToInt(client.portNumber,&portNumber);
	   memset(&clientSocketAddress, 0, sizeof(clientSocketAddress));
	   clientSocketAddress.sin_family = AF_INET;
	   clientSocketAddress.sin_port = htons(portNumber);
	   clientSocketAddress.sin_addr.s_addr = inet_addr(client.ipaddress);

	   //Storing the hostName
	   //fprintf(stdout,"storing the host name\n");
	   //host = gethostbyaddr((struct sockaddr*)&clientSocketAddress,sizeof(struct sockaddr),AF_INET);
	   getHostName(client.ipaddress,client.portNumber,hostName);
	   //fprintf(stdout,"fetched the host name\n");
	   strcpy(client.hostName,hostName);

       //Adding new client detail to the server list
	   //fprintf(stdout,"Adding new client detail to the server list\n");
	   serverList.push_back(client);
	   connectionList.push_back(client);
	   //fprintf(stdout,"Added new client detail to the server list\n");

   }

   int validatePortNumber(string portNumberString){

	   int portNumber;
	   if(stringToInt(portNumberString,&myPortNumber)==false){
	      fprintf(stdout , "Port No. Invalid\n");
	      fflush(stdout);
	      return -1;
	   }
	   if (myPortNumber < 1024) {
	      fprintf(stdout,"[error] Port number must be greater than or equal to 1024.\n");
	      fflush(stdout);
	      return -1;
	   }
	   return 0;
   }


   int parseRegisterConnectCommand(string data,char* ipaddress,char* portNumber){

	   vector<string> userInput;

	   //Step 1 : Parse the input
       parseCommand(data,&userInput);

       //Step 2 : validation of size
       if(userInput.size()!=3){
    	   fprintf(stdout,"Invalid Command\n");
    	   fflush(stdout);
    	   return -1;
       }

       //Step 3 : validation of command
       if(strcasecmp(userInput[0].c_str(),"REGISTER")!=0 && strcasecmp(userInput[0].c_str(),"CONNECT")!=0){
    	   fprintf(stdout,"Invalid Command\n");
    	   fflush(stdout);
    	   return -1;
       }

       //Step 4 : validate the port Number
       if(validatePortNumber(userInput[2])==-1){
    	   fprintf(stdout,"Invalid Port Number\n");
    	   fflush(stdout);
    	   return -1;
       }

       strcpy(ipaddress,userInput[1].c_str());
       strcpy(portNumber,userInput[2].c_str());

	   return 0;
   }


      int processConnectCommand(string data){

	   char* peerIpaddress = new char[INET6_ADDRSTRLEN];
	   char* peerPortNumberString =(char*) malloc(sizeof(int));
	   int connectionSocket,peerPortNumber;
	   int yes=1;
	   struct sockaddr_in peerSocketAddress;

	   // Step 1 :Verify if it is a client and if max no. of connections have already reached
	   if(strcasecmp(processType.c_str(),"s")==0){
	      fprintf(stdout,"This command can only be given to a client\n");
	      fflush(stdout);
	      return -1;
	   }
	   //fprintf(stdout,"Step 1 :Verify if it is a client Successful\n");
	   if(connectionList.size()==4){
	   	      fprintf(stdout,"Maimum number of connections already reached .max no. of possible connection =4 including 1 with server\n");
	   	      fflush(stdout);
	   	      return -1;
	   }

	   //Step 2 :Parse the command and get the peer ip address and port
	   if(parseRegisterConnectCommand(data,peerIpaddress,peerPortNumberString)==-1){
	   	 fprintf(stdout,"Connect Command Syntax is invalid\n");
	   	 fflush(stdout);
	   	 return -1;
	    }
	   //fprintf(stdout,"Step 2 :Parse the command and get the peer ip address and port Successful\n");

	   //Step 3 : Verify if the ipaddress/host name exists in the server list
	   if(ifExistsInServerList(peerIpaddress,peerPortNumberString)==-1){
	     fprintf(stdout,"The specified details donot exist in the server list\n");
	     fflush(stdout);
	     return -1;
	   }
	   //fprintf(stdout,"Step 3 :Verify if the ipaddress/host name exists in the server list Successful\n");

	   //Step 4 : Verify if the ipaddress/host name exists in the connection list
	   if(ifExistsInConnectionList(peerIpaddress,peerPortNumberString)==0){
	   	  fprintf(stdout,"The specified Connection already exists.\n");
	   	  fflush(stdout);
	   	  return -1;
	   }
	   //fprintf(stdout,"Step 4 :Verify if the ipaddress/host name exists in the connection list Successful\n");

       //Step 5 : If the IP address or host name is self reject it
	   if (strcasecmp(peerIpaddress,myIP)==0){
		   fprintf(stdout,"Connection cannot be established with self.\n");
		   fflush(stdout);
		   return -1;
	   }
	   if (strcasecmp(peerIpaddress,myHostName)==0){
	   	   fprintf(stdout,"Connection cannot be established with self.\n");
	   	   fflush(stdout);
	   	   return -1;
	   }
	   //fprintf(stdout,"Step 5 :If the IP address or host name is self reject it Successful\n");

	   //Step 6 : Create a new socket
	   if((connectionSocket = socket(AF_INET,SOCK_STREAM,0))<0){
	   	   fprintf(stdout, "Couldnot create a connection socket\n");
	   	   fflush(stdout);
	   	   return -1;
	   }
	   fcntl(connectionSocket, F_SETFL, O_NONBLOCK);
	   if (setsockopt(connectionSocket,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)) == -1) {
	   	   fprintf(stdout, "Unable to reuse socket\n");
	   	   fflush(stdout);
	   }
	   //fprintf(stdout,"Step 6 :Creating a new socket Successful\n");

	   //Step 7 :Create peer socket address
	   stringToInt(peerPortNumberString,&peerPortNumber);
	   memset(&peerSocketAddress, 0, sizeof(peerSocketAddress));
	   peerSocketAddress.sin_family = AF_INET;
	   peerSocketAddress.sin_port = htons(peerPortNumber);
	   peerSocketAddress.sin_addr.s_addr = inet_addr(peerIpaddress);
	   //fprintf(stdout,"Step 7 :Create peer socket address Successful\n");

	   //Step 8 : Connecting the socket to the peer client address
	   if(connect(connectionSocket,(struct sockaddr*)&peerSocketAddress,sizeof(struct sockaddr))==-1){
	   	   if(errno!=EINPROGRESS){
	   	      fprintf(stdout, "Couldnot connect to the given address.Error while connecting\n");
	   	      fprintf(stdout, "[error] %s  \n" ,strerror(errno));
	   	      fflush(stdout);
	   	      return -1;
	   	   }
	   }
	   //fprintf(stdout,"Step 8 :Connecting the socket to the peer client address Successful\n");

	   //Step 9 : update fdmax and master set
	   if(connectionSocket > fdmax){
	   	   fdmax=connectionSocket;
	   }
	   FD_SET(connectionSocket, &master);


	   //Step 10 : Update the connection list
	   updateConnectionList(peerIpaddress,peerPortNumberString,connectionSocket);
	   //fprintf(stdout,"Step 9 :Update the connection list Successful\n");
	   displayConnectionList();

       free(peerPortNumberString);
	   return 0;
   }

   int processRegisterCommand(string data){
	  char* serverIpaddress = new char[INET6_ADDRSTRLEN];
	  char* serverPortNumberString =(char*) malloc(sizeof(int));
	  int serverPortNumber;
	  int fileTransferSocket;
	  int yes =1;
	  char addServerListPacket[100];
	  struct sockaddr_in serverSocketAddress;

	  //fprintf(stdout,"process type : %s\n",processType.c_str());
      if(strcasecmp(processType.c_str(),"s")==0){
         fprintf(stdout,"Register Command can be given only to a client. You are a server\n");
         fflush(stdout);
         return -1;
      }

      //Check if already registered
      if(connectionList.size()!=0){
    	  fprintf(stdout,"You are already registered\n");
    	  fflush(stdout);
    	  return -1;
      }

      if(parseRegisterConnectCommand(data,serverIpaddress,serverPortNumberString)==-1){
		 fprintf(stdout,"Register Command Syntax is invalid\n");
		 fflush(stdout);
		 return -1;
      }
	  //fprintf(stdout,"Registering you to IPaddrLess %s and Port NO. %s  process begins \n",serverIpaddress,serverPortNumberString);

	  stringToInt(serverPortNumberString,&serverPortNumber);
	  memset(&serverSocketAddress, 0, sizeof(serverSocketAddress));
	  serverSocketAddress.sin_family = AF_INET;
	  serverSocketAddress.sin_port = htons(serverPortNumber);
	  serverSocketAddress.sin_addr.s_addr = inet_addr(serverIpaddress);

	  //Creating a new socket
	  if((fileTransferSocket = socket(serverSocketAddress.sin_family,SOCK_STREAM,0))<0){
	     fprintf(stdout, "Couldnot create a file Transfer Socket\n");
	     fflush(stdout);
	     return -1;
	  }

	  fcntl(fileTransferSocket, F_SETFL, O_NONBLOCK);
	  //Reusing a socket
	   if (setsockopt(fileTransferSocket,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)) == -1) {
	      fprintf(stdout, "Unable to reuse socket\n");
	      fflush(stdout);
	   }

	   //Connecting the socket to the server IP address
	   if(connect(fileTransferSocket,(struct sockaddr*)&serverSocketAddress,sizeof(struct sockaddr))==-1){
	      if(errno!=EINPROGRESS){
	      	  fprintf(stdout, "Couldnot connect to the given address.Error while connecting\n");
	    	  fprintf(stdout, "[error] %s  \n" ,strerror(errno));
	    	  fflush(stdout);
	    	  return -1;
	      }
	   }

	   //updating the fdmax
	   if(fileTransferSocket > fdmax){
	      fdmax=fileTransferSocket;
	   }

	   //Adding the fileTransferSocket to the master set
	   FD_SET(fileTransferSocket, &master);

	   //Updating the connection list of the client
	   updateConnectionList(serverIpaddress,serverPortNumberString,fileTransferSocket);
	   //displayConnectionList();

	   //Creating and sending the register packet to the server
	   //addServerListPacket=addToServerListCommand + " " + string(myIP)  +  " " + myPortNumberString + " ";
	   strcpy(addServerListPacket,addToServerListCommand.c_str());
	   strcat(addServerListPacket,spaceString.c_str());
	   strcat(addServerListPacket,string(myIP).c_str());
	   strcat(addServerListPacket,spaceString.c_str());
	   strcat(addServerListPacket,myPortNumberString.c_str());
	   strcat(addServerListPacket,spaceString.c_str());
	   //fprintf(stdout,"The add-server-list packet being sent is : %s  from socket:%d and port:%d\n",addServerListPacket,fileTransferSocket,getPortFromSocket(fileTransferSocket));

	   if(writeToSocket(fileTransferSocket,addServerListPacket,strlen(addServerListPacket))==-1){
	     fprintf(stdout, "Unable to send to server. Send command error\n");
	     fflush(stdout);
	   }

       delete[] serverIpaddress;
       serverIpaddress=NULL;
	   free(serverPortNumberString);
	   serverPortNumberString=NULL;
       return 0;
   }


   void processHelpCommand(){
      fprintf(stdout , "The followings commands are available\n");
      fprintf(stdout , "1.MYIP\n");
      fprintf(stdout , "2.MYPORT\n");
      fprintf(stdout , "3.REGISTER <SERVER_IP_ADDRESS> <SERVER_PORT>\n");
      fprintf(stdout , "4.CONNECT  <PEER_IP_ADDRESS OR PERR_HOST_NAME> <PEER_LISTENING_PORT>\n");
      fprintf(stdout , "5.UPLOAD   <CONNECTION_NO> <FILE_NAME>\n");
      fprintf(stdout , "6.DOWNLOAD <CONNECTION_NO> <FILE_NAME> ----(upto 3 connections can be specified) \n");
      fprintf(stdout , "7.TERMINATE <CONNECTION_NO> \n");
      fprintf(stdout , "8.EXIT \n");
      fprintf(stdout , "9.HELP \n");
      fprintf(stdout , "8.CREATE \n");
      fflush(stdout);
   }


   void processStandardInput(string data){
      int startIndex,endIndex,length;
      /*Trimming the string for leading and trailing spaces*/
      startIndex=data.find_first_not_of(' ');
      endIndex=data.find_last_not_of(' ');
      length=endIndex-startIndex + 1;
      data=data.substr(data.find_first_not_of(' '),length );

      if(strcasecmp(data.c_str(),"HELP")==0){
	     processHelpCommand();
      }
      else if(strcasecmp(data.c_str(),"MYIP")==0){
	      fprintf(stdout , "Your IP address is %s\n",myIP);
	      fflush(stdout);
      }
      else if(strcasecmp(data.c_str(),"MYPORT")==0){
	     fprintf(stdout , "Your PORT NO is %s\n",myPortNumberString.c_str());
	     fflush(stdout);
      }
      else if(strcasecmp(data.substr(0,8).c_str(),"REGISTER")==0){
	      if(processRegisterCommand(data)==-1){
		      fprintf(stdout, "Register Command Cannot Be Completed\n");
		      fflush(stdout);
	       }
      }
      else if(strcasecmp(data.substr(0,7).c_str(),"CONNECT")==0){
      	   if(processConnectCommand(data)==-1){
      		  fprintf(stdout, "Connect Command Cannot Be Completed\n");
      		  fflush(stdout);
      	   }
      }
      else if(strcasecmp(data.c_str(),"LIST")==0){
            if(processType=="c"){
      	       displayConnectionList();
            }
            else{
               displayServerList();
            }
      	  }
      else if(strcasecmp(data.substr(0,9).c_str(),"TERMINATE")==0){
            if(processTerminateCommand(data)==-1){
               fprintf(stdout, "Terminate Command Cannot Be Completed\n");
               fflush(stdout);
            }
      }
      else if(strcasecmp(data.c_str(),"EXIT")==0){
    	    exitAllConnections();
    	    fprintf(stdout, "All the connections have been closed\n");
    	    fflush(stdout);
    	    close(listenerSocket);
    	    exit(0);
      }
      else if(strcasecmp(data.substr(0,6).c_str(),"UPLOAD")==0){
    	    FD_CLR(0,&master); //Disabling stdin
          	if(processUploadCommand(data)==-1){
          	   fprintf(stdout, "UPLOAD Command Could not be Completed\n");
          	   fflush(stdout);
          	}
          	FD_SET(0,&master); //Enabling stdin
      }
      else if(strcasecmp(data.substr(0,8).c_str(),"DOWNLOAD")==0){
          	FD_CLR(0,&master); //Disabling stdin
            if(processDownloadCommand(data)==-1){
               fprintf(stdout, "DOWNLOAD Command Could not be Completed\n");
               fflush(stdout);
            }
            FD_SET(0,&master); //Enabling stdin
      }
      else if(strcasecmp(data.c_str(),"CREATE")==0){
                 fprintf(stdout, "Name           : Abhishek Kapoor\n");
                 fprintf(stdout, "Person Number  : 5009 8058\n");
                 fprintf(stdout, "UBIT Name      : akapoor\n");
                 fprintf(stdout, "emailID        : akapoor@buffalo.edu\n");
                 fflush(stdout);
      }
      else{
    	  fprintf(stdout, "Invalid Command\n");
    	  fflush(stdout);
      }
   }

    void processData(int socketDescriptor,char* data , int dataSize){
	   char standardInputData[600];
	   if(socketDescriptor==0){
		  //fwrite(data,nbytes-1,1,stdout);
		   //fprintf(stdout,"%s \n",data);
		   strcpy(standardInputData,data);
		   //fwrite(data,nbytes,1,stdout);
		   processStandardInput(standardInputData);
	   } else  if(strcasecmp(string(data,18).c_str(),addToServerListCommand.c_str())==0){
		          if(processType=="s"){
		             //fprintf(stdout, "Add to server list is being called\n");
			         addToServerList(socketDescriptor,data);
			         //fprintf(stdout, "send server list is being called\n");
			         displayServerList();
			         //displayConnectionList();
			         sendServerList();
		          }
		          else{
		        	  fprintf(stdout, "Not a server.Cannot register\n");
		        	  terminateConnection(socketDescriptor);
		          }
	   }
	   else if(strcasecmp(string(data,15).c_str(),newServerListCommand.c_str())==0){
		      storeNewServerList(data);
		      displayServerList();
	   }
	   else if(strcasecmp(string(data,4).c_str(),saveCommand.c_str())==0){
		      saveFile(data,dataSize,socketDescriptor);
	   }
	   else if(strcasecmp(string(data,5).c_str(),fetchCommand.c_str())==0){
	   		  fetchFile(data,dataSize,socketDescriptor);
	   }

    }


   void serviceConnections(){

      fd_set read_fds;   // temp file descriptor list for select()
      int i,newfd;
      struct sockaddr_storage remoteaddr; // client address
      socklen_t addrlen;
      char *buf=NULL ; // buffer for client data
      char remoteIP[INET6_ADDRSTRLEN];
      char* data=NULL;
      //fprintf(stdout,"serving connections Initialized\n");
      //Clearing the sets
      FD_ZERO(&master);
      FD_ZERO(&read_fds);
      int bufferCount;
      //FILE *fd =NULL;
      //fd = fopen("log.txt","a");

      // adding the listening sfd to the master set
      FD_SET(listenerSocket, &master);

      //Adding STDIN to the master set
      FD_SET(0, &master);

      //fprintf(stdout,"Listener and Standard Input are listening\n");
      // keeping track of the biggest file descriptor
      fdmax = listenerSocket;

      while(1){
      read_fds=master;
      if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
         fprintf(stdout,"[error] : select API failure\n");
         fflush(stdout);
         exit(1);
      }
      //fprintf(stdout,"read select call has been done\n");
      //cout << "select call has been done" << endl;
      // run through the existing connections looking for data to read
      for(i = 0; i <= fdmax; i++) {
	     //fprintf(stdout,"for loop increment socket no. :%d \n", i);
         if (FD_ISSET(i, &read_fds)) {
        	 //fprintf(stdout,"We have a new connection at socket :%d. port no. %d\n",i,getPortFromSocket(i));// we got one!!
            if (i == listenerSocket) {
                // handle new connections
            	//fprintf(stdout,"The new connection is listener socket:%d. port no. %d\n",i,getPortFromSocket(i));// we got one!!
        	    addrlen=sizeof(remoteaddr);
                newfd = accept(listenerSocket,(struct sockaddr *)&remoteaddr,&addrlen);
                if(strcasecmp(processType.c_str(),"c")==0){
                	storeNewConnection(&remoteaddr,newfd);
                }
                //fprintf(stdout,"The listener socket has accepted connection. new socket is :%d. port no. %d\n",newfd,getPortFromSocket(newfd));// we got one!!l
                if (newfd == -1) {
                   fprintf(stdout,"[error] Accept call Failure\n");
                   fflush(stdout);
                } else {
                     FD_SET(newfd, &master); // add to master set
                     if (newfd > fdmax) {    // keep track of the max
                        fdmax = newfd;
                     }
                     //fprintf(stdout, "selectserver: new connection from %s on socket %d \n",inet_ntop(remoteaddr.ss_family,get_in_addr((struct sockaddr*)&remoteaddr),remoteIP,INET6_ADDRSTRLEN),newfd);
                  }
            } else {
            	buf = new char[BUFFER_SIZE + 1];
            	//fprintf(stdout,"handling data from a client at socket: %d and port: %d\n",i,getPortFromSocket(i));
                if ((nbytes = read(i, buf, BUFFER_SIZE)) <= 0) {
                if (nbytes == 0) { // got error or connection closed by client
                	 terminateConnection(i);
                     fprintf(stdout,"%d Socket has been closed\n",i); // connection closed
                     fflush(stdout);
                } else {
                     fprintf(stdout,"[error] Receive call failure %d \n", nbytes);
                     fprintf(stdout, "[error] %s  \n" ,strerror(errno));
                     fflush(stdout);
                     close(i);
                     FD_CLR(i, &master); // remove from master set
                }
               }
               buf[nbytes+1]='\0';
               data =(char*) malloc(nbytes);
               if(i==0){
            	   bufferCount=0;
            	   //fprintf(stdout,"no. of bytes is: %d \n",nbytes);
            	   while(buf[bufferCount]!='\n'){
                       data[bufferCount]=buf[bufferCount];
                       bufferCount++;
            	   }
                   data[bufferCount]='\0';
                   bufferCount=0;
               }
               else{
            	   memcpy (data,buf,nbytes);
               }

               //Testing Code Begins
               /*char ch;
               int cnt = 0;
               while((ch = buf[cnt]) != '\0')
               {
            	   fprintf(stdout, "[%d]=%c  [%d] = %c\n", cnt, ch,cnt, data[cnt]);
            	   cnt++;
               }*/
               //Testing Code Ends

               //fprintf(stdout,"process data is being called \n bufer : %s , data :%s\n",buf,data);
               //fprintf(fd,"hi \n");
               processData(i,data,nbytes);   //transferring all the nbytes
               FD_CLR(i, &read_fds);
               delete[] buf;
               free(data);
               data=NULL;
               buf=NULL;
               fflush(stdin);
                } // END handle data from clientL
            } // END got new incoming connection
        } // END looping through file descriptors
       //cout <<"Inside Servicing Connections .....1 loop over";
       //fprintf(stdout,"Inside Servicing Connections .....1 loop over\n");
    } // END While
   }

   int socketSetup(){

	    int yes =1;
        //fprintf(stdout,"socketSetup Initialization\n");

        //Populating mySocketAddress
        if(getMySocketAddress() ==-1){
        	fprintf(stdout, "Cannot get Ip address\n");
        	fflush(stdout);
        	return -1;
        }

        //Creating a socket
        if((listenerSocket = socket(mySocketAddress.sin_family,SOCK_STREAM,0))<0){
        	fprintf(stdout, "Cannot create a socket\n");
        	fflush(stdout);
        	return -1;
        }

        fcntl(listenerSocket, F_SETFL, O_NONBLOCK);

        //Reusing a socket
        if (setsockopt(listenerSocket,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)) == -1) {
   	        fprintf(stdout, "Unable to resuse socket\n");
   	        fflush(stdout);
            return -1;
        }

        //Binding it
        if (bind(listenerSocket,(sockaddr*)&mySocketAddress,sizeof(mySocketAddress)) == -1){
   	       fprintf(stdout, "server: failed to bind\n");
   	       fflush(stdout);
   	       close(listenerSocket);
   	       return -1;
          }

        //Listening to it
        //fprintf(stdout,"Listening socket is %d  port is %d\n",listenerSocket,ntohs(get_in_port((struct sockaddr *)&mySocketAddress)));
        if (listen(listenerSocket, 20) == -1) {
   	       fprintf(stdout, "server: Unable to listen\n");
   	       fflush(stdout);
   	       close(listenerSocket);
           return -1;
          }

        fprintf(stdout,"socketSetup Successful\n");
        fflush(stdout);
        return 0;
      }


   int main(int argc , char **argv){

      if(argc!=3){
         fprintf(stdout , "Example Usage : ./filesharing c 2040\n");
         fflush(stdout);
         exit(1);
      }

      processType = argv[1];
      if(processType!="c" && processType!="s" ){
         fprintf(stdout , "Example Usage : ./filesharing c 2040\n");
         fflush(stdout);
         exit(1);
      }
      if(stringToInt(argv[2],&myPortNumber)==false){
         fprintf(stdout , "Port No. Invalid\n");
         fflush(stdout);
         exit(1);
      }
      if (myPortNumber < 1024) {
         fprintf(stdout,"[error] Port number must be greater than or equal to 1024.\n");
         fflush(stdout);
         exit(1);
      }
      myPortNumberString =argv[2];    //it is a null terminated string

      //Create a socket and Bind to the given Port
      if(socketSetup()==-1){
    	  fprintf(stdout,"[error] Cannot setup the socket\n");
    	  fflush(stdout);
    	  exit(1);
      }

      //The function will call select() and service the connections
      serviceConnections();
      return 0;
   }
