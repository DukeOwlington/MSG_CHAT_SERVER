#include "chatroom_utils.h"

/* global variables for work with threads */
int usr_count = 0;
int msqid_in;
int anon_msqid;
int stat_msqid_in;
int msqid_out[MAX_CLIENTS];
int stat_msqid_out[MAX_CLIENTS];

bool turn_off = false;
MessageBuf message_buf;
Clients clients[MAX_CLIENTS];
key_t key_in = 5;
key_t stat_key_in = 9;
key_t anon_key = 10;
key_t key_out [MAX_CLIENTS];
key_t stat_key_out;
pthread_mutex_t buffer_lock;

/* create message queues */
int InitializeServer(void) {
  int i;
  int count = anon_key;
  int msgflg = IPC_CREAT | 0666;

  if ((msqid_in = msgget(key_in, msgflg)) < 0) {
    perror("msgget");
    return -1;
  }
  if ((stat_msqid_in = msgget(stat_key_in, msgflg)) < 0) {
    perror("msgget");
    return -1;
  }
  if ((anon_msqid = msgget(anon_key, msgflg)) < 0) {
    perror("msgget");
    return -1;
  }
  for (i = 0; i < MAX_CLIENTS; i++) {
    count++;
    key_out[i] = count;
    if ((msqid_out[i] = msgget(key_out[i], msgflg)) < 0) {
      perror("msgget");
      return -1;
    }
  }
  return 0;
}

/* check whether the given user with such name already exists */
bool NameExist(void) {
  int i;
  for (i = 0; i < MAX_CLIENTS; i++) {
    if (strcmp(clients[i].username, message_buf.mtext) == 0) {
      return true;
    }
  }
  return false;
}

/* add client to clients array */
int RegisterClient(void) {
  size_t buf_len;
  int index = 0;

  if (usr_count == MAX_CLIENTS) {
    message_buf.mtype = TOO_FULL;
    buf_len = strlen(message_buf.mtext) + 1;
    msgsnd(anon_msqid, &message_buf, buf_len, 0);
    return 0;
  } else if (NameExist()) {
    message_buf.mtype = USERNAME_ERROR;
    buf_len = strlen(message_buf.mtext) + 1;
    msgsnd(anon_msqid, &message_buf, buf_len, 0);
    return 0;
  }

  while (clients[index].usr_id != 0) {
    index++;
  }
  strcpy(clients[index].username, message_buf.mtext);
  clients[index].usr_id = msqid_out[index];
  message_buf.mtype = SUCCESS;
  sprintf(message_buf.mtext, "%d", key_out[index]);
  buf_len = strlen(message_buf.mtext) + 1;
  msgsnd(anon_msqid, &message_buf, buf_len, 0);
  printf("%s has connected\n", clients[index].username);
  usr_count++;

  return 0;
}

/* if the message is public - send it to all clients */
int HandlePublicMessage(void) {
  int i;
  int num_id;
  char usr_id[2];
  char message[128];
  size_t buf_len;

  strncpy(usr_id, message_buf.mtext, 2);
  num_id = strtol(message_buf.mtext, NULL, 10);
  for (i = 0; i < MAX_CLIENTS; i++) {
    if (key_out[i] == num_id) {
      strcpy(message, clients[i].username);
      strcat(message, ": ");
      break;
    }
  }
  strcat(message, message_buf.mtext + 2);
  memset(message_buf.mtext, 0, MSGSZ);
  strcpy(message_buf.mtext, message);
  buf_len = strlen(message_buf.mtext) + 1;
  for (i = 0; i < usr_count; i++) {
    if (clients[i].usr_id != 0)
      msgsnd(msqid_out[i], &message_buf, buf_len, 0);
  }
  return 0;
}

/* send user list to all clients */
int SendUserList(void) {
  int i;
  size_t buf_len;
  char usr_list[128] = {0};

  for (i = 0; i < usr_count; i++) {
    if (clients[i].usr_id != 0) {
      strcat(usr_list, clients[i].username);
      strcat(usr_list, "\n");
    }
  }
  message_buf.mtype = GET_USERS;
  strcpy(message_buf.mtext, usr_list);
  buf_len = strlen(message_buf.mtext) + 1;

  for (i = 0; i < usr_count; i++) {
    if (clients[i].usr_id != 0)
      msgsnd(msqid_out[i], &message_buf, buf_len, 0);
  }
  return 0;
}

/* if the message is private - send it to certain user */
int HandlePrivateMessage(void) {
  int i;
  int num_id;
  size_t buf_len;
  char usr_id[2];
  char usr_dest[20];
  char message[128];

  strncpy(usr_id, message_buf.mtext, 2);
  num_id = strtol(message_buf.mtext, NULL, 10);
  for (i = 0; i < MAX_CLIENTS; i++) {
    if (key_out[i] == num_id) {
      strcpy(message, clients[i].username);
      strcat(message, ": ");
      break;
    }
  }
  buf_len = strlen(message_buf.mtext) + 1;

  i = 3;
  while (message_buf.mtext[i] != '/') {
    usr_dest[i - 3] = message_buf.mtext[i];
    i++;
  }
  i++;
  strcat(message, message_buf.mtext + i);
  memset(message_buf.mtext, 0, MSGSZ);
  strcpy(message_buf.mtext, message);
  buf_len = strlen(message_buf.mtext) + 1;
  for (i = 0; i < usr_count; i++) {
    if (strcmp(usr_dest, clients[i].username) == 0) {
      msgsnd(msqid_out[i], &message_buf, buf_len, 0);
      break;
    }
  }
  return 0;
}

/* disconnect client */
int DisconnectUser(void) {
  int num_id;
  int i;
  char usr_id[2];

  strncpy(usr_id, message_buf.mtext, 2);
  num_id = strtol(message_buf.mtext, NULL, 10);
  for (i = 0; i < MAX_CLIENTS; i++) {
    if (key_out[i] == num_id) {
      clients[i].usr_id = 0;
      printf("User %s has disconnected\n", clients[i].username);
      memset(clients[i].username, 0, 20);
    }
  }
  usr_count--;
  SendUserList();
  return 0;
}

/* thread responsible for incoming message handling */
void *HandleClientMsg(void *args) {
  int status;
  while (!turn_off) {
    pthread_mutex_lock(&buffer_lock);
    status = msgrcv(msqid_in, &message_buf, MSGSZ, ALIVE, IPC_NOWAIT | MSG_EXCEPT);
    if (status < 0) {
      pthread_mutex_unlock(&buffer_lock);
      continue;
    }
    switch (message_buf.mtype) {
      case CONNECT:
        RegisterClient();
        SendUserList();
        break;
      case PUBLIC_MESSAGE:
        HandlePublicMessage();
        break;
      case PRIVATE_MESSAGE:
        HandlePrivateMessage();
        break;
      case GET_USERS:
        SendUserList();
        break;
      case DISCONNECT:
        DisconnectUser();
        break;
      default:
        puts("Uknown message");
    }
    pthread_mutex_unlock(&buffer_lock);
  }
  pthread_exit(NULL);
}

/* thread responsible for sending messages
 * to clients that server is alive */
void *SendServerStatus(void *args) {
  size_t buf_len;
  struct msqid_ds buf;
  while (!turn_off) {
    pthread_mutex_lock(&buffer_lock);
    if (msgctl(stat_msqid_in, IPC_STAT, &buf) < 2) {
      message_buf.mtype = ALIVE;
      buf_len = strlen(message_buf.mtext) + 1;
      msgsnd(stat_msqid_in, &message_buf, buf_len, 0);
    }
    pthread_mutex_unlock(&buffer_lock);
    sleep(1);
  }
  pthread_exit(NULL);
}

/* thread responsible for checking
 * whether the client is alive */
void *CheckClientStatus(void *args) {
  int status;
  int i;
  bool usr_lst = false;
  while (!turn_off) {
    sleep(5);
    pthread_mutex_lock(&buffer_lock);
    for (i = 0; i < MAX_CLIENTS; i++) {
      status = msgrcv(msqid_out[i], &message_buf, MSGSZ, ALIVE, IPC_NOWAIT);
      if (status < 0 && clients[i].usr_id != 0) {
        clients[i].usr_id = 0;
        printf("User %s has disconnected\n", clients[i].username);
        memset(clients[i].username, 0, 20);
        usr_count--;
        usr_lst = true;
      }
      if (usr_lst) {
        SendUserList();
        usr_lst = false;
      }
    }
    pthread_mutex_unlock(&buffer_lock);
  }
  pthread_exit(NULL);
}

int main(void) {
  int i;
  char input[2];
  pthread_t server_func[2];
  pthread_attr_t attr;

  for (i = 0; i < MAX_CLIENTS; i++) {
    clients[i].usr_id = 0;
  }

  if (InitializeServer() < 0) {
    return EXIT_FAILURE;
  }

  if (pthread_mutex_init(&buffer_lock, NULL) < 0) {
    perror("mutex failed: ");
    return EXIT_FAILURE;
  }

  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

  pthread_create(&server_func[0], NULL, HandleClientMsg, NULL);
  pthread_create(&server_func[1], NULL, SendServerStatus, NULL);
  pthread_create(&server_func[2], NULL, CheckClientStatus, NULL);

  pthread_attr_destroy(&attr);

  printf("Type /q to turn off the server\n");

  while (strcmp(input, "/q") != 0)
    scanf("%2s", input);

  turn_off = true;
  for (i = 0; i < 3; i++) {
    pthread_join(server_func[i], NULL);
  }

  pthread_mutex_destroy(&buffer_lock);

  return EXIT_SUCCESS;
}
