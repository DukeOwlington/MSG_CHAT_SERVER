#ifndef CHATROOM_UTILS_H_INCLUDED
#define CHATROOM_UTILS_H_INCLUDED

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#define MSGSZ     150
#define MAX_CLIENTS 4

/* enum of different messages possible */
typedef enum {
  CONNECT = 1,
  DISCONNECT,
  GET_USERS,
  SET_USERNAME,
  PUBLIC_MESSAGE,
  PRIVATE_MESSAGE,
  TOO_FULL,
  USERNAME_ERROR,
  SUCCESS,
  ERROR,
  ALIVE
} MessageType;


/* message structure */
typedef struct message {
    long   mtype;
    char    mtext[MSGSZ];
} MessageBuf;

/* structure to hold client connection information */
typedef struct Clients {
  int usr_id;
  char username[20];
} Clients;

int InitializeServer(void);
bool NameExist(void);
int RegisterClient(void);
int HandlePublicMessage(void);
int SendUserList(void);
int HandlePrivateMessage(void);
int DisconnectUser(void);
#endif
