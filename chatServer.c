#include "chatServer.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#define MAX_PORT 65535
static int end_server = 0;
int get_positive_num(char*);
int get_max_fd(conn_pool_t*);
void destroy_pool(conn_pool_t*);
void set_all_fd(conn_pool_t*);
int max(int, int);
void intHandler(int SIG_INT) {
	/* use a flag to end_server to break the main loop */
    end_server = 1;
}
int main (int argc, char *argv[])
{
	if(argc != 2 ) {
        printf("Usage:");
        for(int i=0;i<argc;i++)
            printf(" %s",argv[i]);
        printf("\n");
        exit (0);
    }
    int port=0;
    port = get_positive_num(argv[1]);
	if(port < 1 || port > MAX_PORT){
        printf("Usage:");
        for(int i=0;i<argc;i++)
            printf(" %s",argv[i]);
        printf("\n");
        exit (0);
    }
    signal(SIGINT, intHandler);
    signal(SIGPIPE, SIG_IGN);
    char buffer[BUFFER_SIZE]={};
    char* message=(char*)malloc(sizeof(char)*(1+BUFFER_SIZE));
    if(!message){
        perror("malloc");
        exit(1);
    }
	conn_pool_t* pool = malloc(sizeof(conn_pool_t));
	init_pool(pool);

	/*************************************************************/
	/* Create an AF_INET stream socket to receive incoming      */
	/* connections on                                            */
	/*************************************************************/
	int welcome;
    if((welcome = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");

        exit(1);
    }
    struct sockaddr_in srv;
    srv.sin_family = AF_INET;
    srv.sin_port = htons(port);
    srv.sin_addr.s_addr = htonl(INADDR_ANY);
    socklen_t srv_len = sizeof(srv);


    /*************************************************************/
	/* Set socket to be nonblocking. All of the sockets for      */
	/* the incoming connections will also be nonblocking since   */
	/* they will inherit that state from the listening socket.   */
	/*************************************************************/
    int on = 1;
    int rc = ioctl(welcome, (int)FIONBIO, (char *)&on);
	/*************************************************************/
	/* Bind the socket                                           */
	/*************************************************************/
	if(bind(welcome, (struct sockaddr*) &srv, sizeof(srv)) < 0) {
        perror("bind");
        destroy_pool(pool);
        free(pool);
        free(message);
        exit(1);
    }

	/*************************************************************/
	/* Set the listen back log                                   */
	/*************************************************************/
    if(listen(welcome, 15)<0){
        perror("listen");
        destroy_pool(pool);
        free(pool);
        free(message);
        exit(1);
    }
    /*------------debug------------------------------
    while(1){
        int test = accept(welcome,(struct sockaddr*) &srv, &srv_len);
        write(test,"work\n",5);
        close(test);
    }
    /*------------------------------------------------*/
    FD_SET(welcome, &(pool->ready_read_set));
	/*************************************************************/
	/* Initialize fd_sets: in init_pool()  			                             */
	/*************************************************************/
	
	/*************************************************************/
	/* Loop waiting for incoming connects, for incoming data or  */
	/* to write data, on any of the connected sockets.           */
	/*************************************************************/
    do{
		/**********************************************************/
		/* Copy the master fd_set over to the working fd_set.     */
		/**********************************************************/
        pool->read_set = pool->ready_read_set;
        pool->write_set = pool->ready_write_set;
        //set_all_fd(pool);
        //FD_SET(welcome,&(pool->read_set));
		/**********************************************************/
		/* Call select() 										  */
		/**********************************************************/
        if(select(max(welcome,pool->maxfd)+1, &(pool->read_set), &(pool->write_set), NULL, NULL)<0){
            perror("select");
            destroy_pool(pool);
            free(pool);
            free(message);
            exit(1);
        }
        //printf("select passed!\n");
        /**********************************************************/
		/* One or more descriptors are readable or writable.      */
		/* Need to determine which ones they are.                 */
		/**********************************************************/

        /* check all descriptors, stop when checked all valid fds */
		for (int i=1;i<=max(pool->maxfd, welcome);i++){
			/* Each time a ready descriptor is found, one less has  */
			/* to be looked for.  This is being done so that we     */
			/* can stop looking at the working set once we have     */
			/* found all of the descriptors that were ready         */
				
			/*******************************************************/
			/* Check to see if this descriptor is ready for read   */
			/*******************************************************/
			if (FD_ISSET(i,&(pool->read_set))){
				/***************************************************/
				/* A descriptor was found that was readable		   */
				/* if this is the listening socket, accept one      */
				/* incoming connection that is queued up on the     */
				/*  listening socket before we loop back and call   */
				/* select again. 						            */
				/****************************************************/
                if(i==welcome) {
                    int fd = accept(welcome,(struct sockaddr*) &srv, &srv_len);
                    if(fd<0)
                        continue;
                    printf("accept fd: %d\n", fd);
                    add_conn(fd, pool);
                    FD_SET(fd, &(pool->ready_read_set));
                    ioctl(fd, (int)FIONBIO, (char *)&on);
                }
				/****************************************************/
				/* If this is not the listening socket, an 			*/
				/* existing connection must be readable				*/
				/* Receive incoming data his socket             */
				/****************************************************/
                else {
                    int nbytes=0, readed=0;
                    strcpy(buffer,"");
                    strcpy(message,"");
                    while (1) {
                        nbytes = (int)read(i, buffer, sizeof(buffer));
                        if(nbytes<=0)
                            break;
                        strcat(message, buffer);
                        readed+=nbytes;
                        message = realloc(message,sizeof(char)*(readed+BUFFER_SIZE+1));
                        if(strstr(message,"\r\n"))
                            break;
                    }
                    if(readed) {
                        //FD_CLR(i, &(pool->ready_read_set));
                        for (int j = 1; j < strlen(message); j++)
                            if (message[j - 1] == '\r' && message[j] == '\n')
                                message[j + 1] = '\0';
                        printf("read from %d\n",i);
                        printf("message recived: %s\n", message);
                        add_msg(i,message, (int)strlen(message), pool);
                        strcpy(message,"");
                    }
                    //else{
                        //FD_CLR(i, &(pool->ready_read_set));
                        //remove_conn(i, pool);
                        //printf("%d removed from connections\n",i);
                    //}

    
                }
				/* If the connection has been closed by client 		*/
                /* remove the connection (remove_conn(...))    		*/
                /*
                if(i!=welcome && write(i,NULL,0)==-1){
                    printf("%d detected to be romved\n",i);
                    int e = errno;
                    if(errno == EPIPE)
                        remove_conn(i, pool);
                }
                */
                on = 1;
                ioctl(i, (int)FIONREAD, (char*)&on);
                if (on == 0)
                    remove_conn(i, pool);
				/**********************************************/
				/* Data was received, add msg to all other    */
				/* connectios					  			  */
				/**********************************************/

			} /* End of if (FD_ISSET()) */
			/*******************************************************/
			/* Check to see if this descriptor is ready for write  */
			/*******************************************************/
			if (FD_ISSET(i, &(pool->write_set))) {
				/* try to write all msgs in queue to sd */
				for(conn_t* cur = pool->conn_head; cur; cur = cur->next)
                    write_to_client(cur->fd,pool);
		 	}
		 /*******************************************************/
		 
		 
        } /* End of loop through selectable descriptors */
        //pool->ready_read_set = pool->read_set;
        //pool->ready_write_set = pool->write_set;
    } while (end_server == 0);

	/*************************************************************/
	/* If we are here, Control-C was typed,						 */
	/* clean up all open connections					         */
	/*************************************************************/
    printf("before destroy pool active connection: %d\n",pool->nr_conns);
    destroy_pool(pool);
    free(pool);
    free(message);
	return 0;
}


int init_pool(conn_pool_t* pool) {
    /* Largest file descriptor in this pool. */
    pool->maxfd=0;
    /* Number of ready descriptors returned by select. */
    pool->nready=0;
    /* Set of all active descriptors for reading. */
    FD_ZERO(&(pool->read_set));
    /* Subset of descriptors ready for reading. */
    FD_ZERO(&(pool->ready_read_set));
    /* Set of all active descriptors for writing. */
    FD_ZERO(&(pool->write_set));
    /* Subset of descriptors ready for writing.  */
    FD_ZERO(&(pool->ready_write_set));
    /* Doubly-linked list of active client connection objects. */
    pool->conn_head = NULL;
    /* Number of active client connections. */
    pool->nr_conns=0;
    return 0;
}

int add_conn(int sd, conn_pool_t* pool) {
	/*
	 * 1. allocate connection and init fields
	 * 2. add connection to pool
	 * */
    conn_t* new_con = (conn_t*) malloc(sizeof (conn_t));
    if(!new_con)
        return  -1;
    new_con->prev = NULL;
    new_con->next = pool->conn_head;
    pool->conn_head = new_con;
    new_con->fd = sd;
    new_con->write_msg_head = new_con->write_msg_tail = NULL;
    if(sd>pool->maxfd)
        pool->maxfd = sd;
    pool->nready++;
    pool->nr_conns++;
	return 0;
}


int remove_conn(int sd, conn_pool_t* pool) {
	/*
	* 1. remove connection from pool 
	* 2. deallocate connection 
	* 3. remove from sets 
	* 4. update max_fd if needed 
	*/
    conn_t* to_remove=NULL;
    for(conn_t* cur = pool->conn_head;cur;cur = cur->next)
        if(cur->fd==sd) {
            to_remove = cur;
            break;
        }
    if(!to_remove)                                  //not found
        return -1;
    // remove the conn from the pool list
    printf("remove %d from the pool\n",sd);
    conn_t* before_to_remove = to_remove->prev;
    conn_t* after_to_remove = to_remove->next;
    if(before_to_remove)                            //not first of the list
        before_to_remove->next = after_to_remove;
    else                                            //first of the list
        pool->conn_head = after_to_remove;
    if(after_to_remove)                             //not the last of the list
        after_to_remove->prev = before_to_remove;
	//free all msgs
    msg_t* next=NULL;
    msg_t* cur=to_remove->write_msg_head;
    while(cur){
        next = cur->next;
        free(cur->message);
        free(cur);
        cur = next;
    }
    //remove the fd from all sets
    FD_CLR(to_remove->fd, &(pool->ready_write_set));
    FD_CLR(to_remove->fd, &(pool->ready_read_set));
    //FD_CLR(to_remove->fd, &(pool->write_set));
    //FD_CLR(to_remove->fd, &(pool->read_set));
    //update max fd
    if(to_remove->fd == pool->maxfd)
        pool->maxfd = get_max_fd(pool);
    free(to_remove);
    pool->nr_conns--;
    pool->nready--;
	return 0;
}

int add_msg(int sd,char* buffer,int len,conn_pool_t* pool) {
	
	/*
	 * 1. add msg_t to write queue of all other connections 
	 * 2. set each fd to check if ready to write 
	 */
    for(conn_t* cur = pool->conn_head;cur;cur = cur->next){
        if(sd == cur->fd)//the sender, no need to append the message
            continue;
        //add the message to the conn
        msg_t* new_msg = (msg_t*)malloc(sizeof(msg_t));
        if(!new_msg)
            return -1;
        new_msg->message = (char*) malloc(sizeof(char)*(len+1));
        if(!new_msg->message) {
            free(new_msg);
            return -1;
        }
        new_msg->next = new_msg->prev = NULL;
        strcpy(new_msg->message, buffer);
        new_msg->size = len;
        if(!cur->write_msg_tail)                //empty list
            cur->write_msg_head = cur->write_msg_tail = new_msg;
        else{
            cur->write_msg_tail->next = new_msg;
            new_msg->prev = cur->write_msg_tail;
            cur->write_msg_tail = new_msg;
        }
        //configure the sets to check if ready to write
        FD_SET(cur->fd,&(pool->ready_write_set));
    }
	
	return 0;
}

int write_to_client(int sd,conn_pool_t* pool) {
	
	/*
	 * 1. write all msgs in queue 
	 * 2. deallocate each writen msg 
	 * 3. if all msgs were writen successfully, there is nothing else to write to this fd... */
	conn_t* client = NULL;
    for(conn_t* cur = pool->conn_head; cur; cur = cur->next)
        if(cur->fd == sd){
            client = cur;
            break;
        }
    if(!client)
        return -1;
    msg_t* next = NULL;
    msg_t* cur = client->write_msg_head;
    while(cur){
        next = cur->next;
        if(write(sd, cur->message, strlen(cur->message))<0)
            return -1;
        client->write_msg_head = next;
        free(cur->message);
        free(cur);
        cur = next;
    }
    if(!client->write_msg_head)//all message wrote successfully
        client->write_msg_tail = NULL;
	return 0;
}
int get_positive_num(char* str){
    for(int i=0;i<strlen(str);i++) {
        if (!isdigit(str[i]))
            return -1;
    }
    int val;
    sscanf(str,"%d",&val);
    return val;
}
int get_max_fd(conn_pool_t* pool){
    int max = 0;
    for(conn_t* cur = pool->conn_head;cur;cur = cur->next)
        if(cur->fd > max)
            max = cur->fd;
    return max;
}
void destroy_pool(conn_pool_t* pool){
    conn_t* cur_con = pool->conn_head;
    conn_t* next_con = NULL;
    while(cur_con){
        next_con = cur_con->next;
        msg_t* cur_msg = cur_con->write_msg_head;
        msg_t* next_msg = NULL;
        while(cur_msg){
            next_msg = cur_msg->next;
            free(cur_msg->message);
            free(cur_msg);
            cur_msg = next_msg;
        }
        printf("removing connection with sd %d \n", cur_con->fd);
        close(cur_con->fd);
        free(cur_con);
        cur_con = next_con;
    }
    init_pool(pool);
}
int max(int a, int b){
    if(a>b)
        return a;
    return b;
}
void set_all_fd(conn_pool_t* pool){
    FD_ZERO(&(pool->read_set));
    FD_ZERO(&(pool->write_set));
    for(conn_t* cur = pool->conn_head; cur; cur = cur->next){
        FD_SET(cur->fd,&(pool->read_set));
        FD_SET(cur->fd,&(pool->write_set));
    }
}