In this Project , I have implemnted peer - peer file sharing in the following manner

1) If a client wants to start file sharing , he needs to register himself with the server.
2)The server is just used for client discovery.
3)The server then sends a list of all registered clients to all the clients.
4)Whenever any new client registers or an old client deregisters/drops connection , the list of registered client list at every client gets updated.
5)The client can then establish a connection with any of the registered client. 
6)It can upload and download documents.
7)The download can also be done from multiple clients.

