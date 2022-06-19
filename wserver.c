#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>    /* Internet domain header */

#include "wrapsock.h"
#include "ws_helpers.h"

#define MAXCLIENTS 10

int handleClient(struct clientstate *cs, char *line);

// You may want to use this function for initial testing
//void write_page(int fd);

int
main(int argc, char **argv) {

    if(argc != 2) {
        fprintf(stderr, "Usage: wserver <port>\n");
        exit(1);
    }
    unsigned short port = (unsigned short)atoi(argv[1]);
    int listenfd;
    struct clientstate client[MAXCLIENTS];


    // Set up the socket to which the clients will connect
    listenfd = setupServerSocket(port);

    initClients(client, MAXCLIENTS);

    fd_set sockets, sockets_to_accept;
    FD_ZERO(&sockets);  // starts empty
    FD_SET(listenfd, &sockets);  // add the server socket
    
    // loop until server is shut down
    while (1) {

        sockets_to_accept = sockets;

        // select the socket that are ready to be accepted
        // the limit is set by MAXCLIENTS
        if (select(MAXCLIENTS, &sockets_to_accept, NULL, NULL, NULL) < 0) {
            perror("select error");
            exit(EXIT_FAILURE);
        }

        // loop through all the clients
        for (int i=0; i < MAXCLIENTS; i++) {
            // if it is in the set to be accepted, then accept it!
            if (FD_ISSET(i, &sockets_to_accept)) {
                if (i == listenfd) {

                    struct sockaddr *client_addr = NULL;
                    unsigned int size = sizeof(struct sockaddr);
                    int client_socket = Accept(listenfd, client_addr, &size);

                    FD_SET(client_socket, &sockets);  // add it to the general sockets set

                } else {

                    char buffer[500];
                    read(i, buffer, 500);  // read to http request from the client socket
                    
                    struct clientstate current_client;  // construct a clienstate using the buffer
                    current_client = current_client;
                    current_client.sock = i;
                    current_client.request = buffer;
                    current_client.path = getPath(buffer);
                    current_client.query_string = getQuery(buffer);

                    handleClient(&current_client, buffer);  // now that we have the clientstate setup, we can handle it

                    FD_CLR(i, &sockets);  // remoce the socket from the set

                }
            }
        }
    }


    return 0;
}

/* Update the client state cs with the request input in line.
 * Intializes cs->request if this is the first read call from the socket.
 * Note that line must be null-terminated string.
 *
 * Return 0 if the get request message is not complete and we need to wait for
 *     more data
 * Return -1 if there is an error and the socket should be closed
 *     - Request is not a GET request
 *     - The first line of the GET request is poorly formatted (getPath, getQuery)
 * 
 * Return 1 if the get request message is complete and ready for processing
 *     cs->request will hold the complete request
 *     cs->path will hold the executable path for the CGI program
 *     cs->query will hold the query string
 *     cs->output will be allocated to hold the output of the CGI program
 *     cs->optr will point to the beginning of cs->output
 */
int handleClient(struct clientstate *cs, char *line) {

    
    // If the resource is favicon.ico we will ignore the request
    if(strcmp("favicon.ico", cs->path) == 0){
        // A suggestion for debugging output
        fprintf(stderr, "Client: sock = %d\n", cs->sock);
        fprintf(stderr, "        path = %s (ignoring)\n", cs->path);
		printNotFound(cs->sock);
        return -1;
    }


    // A suggestion for printing some information about each client. 
    // You are welcome to modify or remove these print statements
    fprintf(stderr, "Client: sock = %d\n", cs->sock);
    fprintf(stderr, "        path = %s\n", cs->path);
    fprintf(stderr, "        query_string = %s\n", cs->query_string);


    // process the clients request and store the output in a pipe descriptor
    int pd = processRequest(cs);

    int cgi_size = cs->optr - cs->output;
    char buffer[cgi_size];

    // read the pipe descriptor and store its contents in 'buffer'
    read(pd, buffer, cgi_size);

    // send ok message
    char ok[] = "HTTP/1.1 200 OK\r\n";
    write(cs->sock, "HTTP/1.1 200 OK\r\n", sizeof(ok));

    // write the output of the cgi program to the client
    write(cs->sock, buffer, cgi_size/2);


    return 1;
}

