#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/mman.h>

/* portul folosit */

#define PORT 2728

extern int errno;		/* eroarea returnata de unele apeluri */

static volatile int keepRunning = 1;

struct clientDetails{
  int descriptor;
  char nume[50];
  int logat;
  int folosit;
};
struct message{
  char content[1024];
  char sender[50];
  unsigned long id;
};
long global_id=0;

void printClientStatsDebug(int client_servit);
int fetchMessage(int fd);
void Register(char * username);
int Login(char * username, int client_servit);
void loginClient(char * username, int client_servit);
void disconnectClient(int client_servit);
int SendMessage(char * senderName, char * destination, char * message);
void saveChat(char *user1, char *user2, struct message messageStruct);
void ReplyMessage(char *user1, char *user2, char *id, char *repliedMessage);
void ShowHistory(char *user1, int U1Desc, char *user2);
void saveGlobalCount();
void readGlobalCount();
struct message getMessageData(char *user1, char *user2, char *id);
void revstr(char *str1);

/* functie de convertire a adresei IP a clientului in sir de caractere */
char * conv_addr (struct sockaddr_in address)
{
  static char str[25];
  char port[7];

  /* adresa IP a clientului */
  strcpy (str, inet_ntoa (address.sin_addr));	
  /* portul utilizat de client */
  bzero (port, 7);
  sprintf (port, ":%d", ntohs (address.sin_port));	
  strcat (str, port);
  return (str);
}
struct clientDetails clientDet[100];
int nrClienti = 0;
char parameter[50];

void  INThandler(int sig)
{
  char  c;

  signal(sig, SIG_IGN);
  printf("OUCH, did you hit Ctrl-C?\nDo you really want to quit? [y/n] ");
  c = getchar();
  if (c == 'y' || c == 'Y'){
    char msgrasp[1300];
    sprintf(msgrasp,"[Server]:Closing...\n");
    for(int i=0;i<100;i++){
      if(clientDet[i].folosit==1){
        if(write(clientDet[i].descriptor,msgrasp,sizeof(msgrasp)) <= 0){
          perror("write catre client error\n");
        }
      }
    }
    kill(0,SIGKILL);
    exit(0);
  }
  else
    signal(SIGINT, INThandler);
  getchar(); // Get new line character
}//https://stackoverflow.com/questions/4217037/catch-ctrl-c-in-c

void* create_shared_memory(size_t size) {
  // Our memory buffer will be readable and writable:
  int protection = PROT_READ | PROT_WRITE;

  // The buffer will be shared (meaning other processes can access it), but
  // anonymous (meaning third-party processes cannot obtain an address for it),
  // so only this process and its children will be able to use it:
  int visibility = MAP_SHARED | MAP_ANONYMOUS;

  // The remaining parameters to `mmap()` are not important for this use case,
  // but the manpage for `mmap` explains their purpose.
  return mmap(NULL, size, protection, visibility, -1, 0);
}//https://stackoverflow.com/questions/5656530/how-to-use-shared-memory-with-linux-in-c

struct clientDetails*  shmem;
fd_set readfds;		/* multimea descriptorilor de citire */
fd_set actfds;		/* multimea descriptorilor activi */

int sd, client;		/* descriptori de socket */
int optval=1; 			/* optiune folosita pentru setsockopt()*/ 
int fd;			/* descriptor folosit pentru 
          parcurgerea listelor de descriptori */
int nfds;			/* numarul maxim de descriptori */

/* programul */
int main ()
{
  global_id=(long)create_shared_memory(sizeof(long));
  shmem = (struct clientDetails*)create_shared_memory(100 * sizeof(struct clientDetails));
  memcpy(shmem, clientDet, 100*sizeof(struct clientDetails));

  readGlobalCount();
  /*Detalii despre clientii care o sa se conecteze*/
  
  bzero(clientDet,100);
  for(int i=0;i<100;i++){
    clientDet[i].logat=0;
    clientDet[i].folosit=0;
    
    clientDet[i].descriptor=-1;
  }
  struct sockaddr_in server;	/* structurile pentru server si clienti */
  struct sockaddr_in from;
 
  struct timeval tv;		/* structura de timp pentru select() */
  int len;			/* lungimea structurii sockaddr_in */

  /* creare socket */
  if ((sd = socket (AF_INET, SOCK_STREAM, 0)) == -1)
    {
      perror ("[server] Eroare la socket().\n");
      return errno;
    }

  /*setam pentru socket optiunea SO_REUSEADDR */ 
  setsockopt(sd, SOL_SOCKET, SO_REUSEADDR,&optval,sizeof(optval));

  /* pregatim structurile de date */
  bzero (&server, sizeof (server));

  /* umplem structura folosita de server */
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = htonl (INADDR_ANY);
  server.sin_port = htons (PORT);

  /* atasam socketul */
  if (bind (sd, (struct sockaddr *) &server, sizeof (struct sockaddr)) == -1)
    {
      perror ("[server] Eroare la bind().\n");
      return errno;
    }

  /* punem serverul sa asculte daca vin clienti sa se conecteze */
  if (listen (sd, 5) == -1)
    {
      perror ("[server] Eroare la listen().\n");
      return errno;
    }
  
  /* completam multimea de descriptori de citire */
  FD_ZERO (&actfds);		/* initial, multimea este vida */
  FD_SET (sd, &actfds);		/* includem in multime socketul creat */

  tv.tv_sec = 1;		/* se va astepta un timp de 1 sec. */
  tv.tv_usec = 0;
  
  /* valoarea maxima a descriptorilor folositi */
  nfds = sd;

  printf ("[server] Asteptam la portul %d...\n", PORT);
  fflush (stdout);
  
  /* servim in mod concurent clientii... */
  signal(SIGINT, INThandler);
  while (1)
  {
    /* ajustam multimea descriptorilor activi (efectiv utilizati) */
    bcopy ((char *) &actfds, (char *) &readfds, sizeof (readfds));

    /* apelul select() */
    if (select (nfds+1, &readfds, NULL, NULL, &tv) < 0)
    {
      perror ("[server] Eroare la select().\n");
      return errno;
    }
    /* vedem daca e pregatit socketul pentru a-i accepta pe clienti */
    if (FD_ISSET (sd, &readfds))
    {
      /* pregatirea structurii client */
      len = sizeof (from);
      bzero (&from, sizeof (from));

      /* a venit un client, acceptam conexiunea */
      client = accept (sd, (struct sockaddr *) &from, &len);

      /* eroare la acceptarea conexiunii de la un client */
      if (client < 0)
        {
          perror ("[server] Eroare la accept().\n");
          continue;
        }
        if (nfds < client) /* ajusteaza valoarea maximului */
          nfds = client;
              
      /* includem in lista de descriptori activi si acest socket */
      FD_SET (client, &actfds);
      for(int i=0;i<100;i++)
      {
        if(clientDet[i].folosit==0){
          clientDet[i].descriptor = client;
          strcpy(clientDet[i].nume,"Anonim");
          clientDet[i].logat = 0;
          clientDet[i].folosit = 1;
          nrClienti++;
          break;
        }
        if(i==99)
        {
          printf("Nu avem spatiu pentru clientul %d", client);
          close(client);
        } 
      }
      memcpy(shmem, clientDet, 100*sizeof(struct clientDetails));
      printf("[server] S-a conectat clientul cu descriptorul %d, de la adresa %s.\n",client, conv_addr (from));
      fflush (stdout);
    }
    /* vedem daca e pregatit vreun socket client pentru a trimite raspunsul */
    for (fd = 0; fd <= nfds; fd++)	/* parcurgem multimea de descriptori */
	  {
      /* este un socket de citire pregatit? */
      if (fd != sd && FD_ISSET (fd, &readfds))
      {	
        if (fetchMessage(fd)<=0)
        {
          printf ("[Server] S-a deconectat clientul cu descriptorul %d.\n",fd);
          fflush (stdout);
          nrClienti--;
          close (fd);		/* inchidem conexiunea cu clientul */
          FD_CLR (fd, &actfds);/* scoatem si din multime */
        }
      }
    }
	}
}

/* realizeaza primirea si retrimiterea unui mesaj unui client */
int fetchMessage(int fd)
{
  char buffer[1024];		/* mesajul */
  int bytes = 0;			/* numarul de octeti cititi/scrisi */
  char msg[1024];		//mesajul primit de la client 
  char msgrasp[1200]=" ";        //mesaj de raspuns pentru client
  int clientulServit;
  bzero(buffer,1024); bzero(msg,1024), bzero(msgrasp,1078);

  for(int i=0;i<100;i++){
    if(clientDet[i].descriptor==fd && clientDet[i].folosit==1)
      clientulServit=i;
  }

  bytes = read (clientDet[clientulServit].descriptor, msg, sizeof (buffer));
  if (bytes <= 0)
  {
    perror ("Eroare la read() de la client.\n");
    return 0;
  }
  msg[strlen(msg)-1]='\0';
  printf ("[server]Mesajul a fost receptionat de la %d...%s\n", fd, msg);
  /*Tratarea mesajului primit de la client comanda/mesaj*/
  if(msg[0]=='/'){
    printf("[Server]S-a dat o comanda catre server!\n");
    if(strstr(msg,"login")!= NULL){
      printf("Clientul %s incearca sa se logheze!\n",msg + strlen("/login "));
      if(Login(msg + strlen("/login "), clientulServit))
      {
        bzero(msgrasp,1200);
        sprintf(msgrasp,"Utilizatorul %s s-a logat cu succes!\n", clientDet[clientulServit].nume);
        if(write(clientDet[clientulServit].descriptor,msgrasp,sizeof(msgrasp)) <= 0){
          perror("write catre client error\n"); return 0;
        }
      }
      else{
        bzero(msgrasp,1200);
        sprintf(msgrasp,"Utilizatorul %s nu s-a logat corect!\n", clientDet[clientulServit].nume);
        if(write(clientDet[clientulServit].descriptor,msgrasp,sizeof(msgrasp)) <= 0){
          perror("write catre client error\n"); return 0;
        }
      }
      strcpy(parameter,"");
    }
    if(strstr(msg,"register")!=NULL){
      Register(msg + strlen("/register "));
    }
    if(strstr(msg,"send")!=NULL){
      char parametri[305] = " ";
      strcpy(parametri,msg+strlen("/send "));
      char destinatar[50] = " ";
      int j=0;
      for(int i=0;i<strlen(parametri);i++){
        if(parametri[i]==' ')  break;
        destinatar[j] = parametri[i];
        j++;
      }
      printf("parametri: %s\ndestinatar: %s\n", parametri, destinatar);
      SendMessage(clientDet[clientulServit].nume, destinatar, parametri + strlen(destinatar) + 1);
    } 
    if(strstr(msg,"reply")!=NULL){
      char parametri[1500] = " ";
      char gotId[10];
      strcpy(parametri,msg+strlen("/reply "));
      //printf("REPLY ACTIVAT!!!\nparams:%s\n",parametri);
      char destinatar[50] = " ";
      int j=0;
      for(int i=0;i<strlen(parametri);i++){
        if(parametri[i]==' ')  break;
        destinatar[j] = parametri[i];
        j++;
      }
      j=0;
      //printf("Dest:%s\n",destinatar);
      //printf("param after dest:%s\n",parametri+strlen(destinatar)+1);
      for(int i=strlen(destinatar)+1;i<strlen(parametri)+1;i++){
        if(parametri[i]==' ')  break;
        gotId[j] = parametri[i];
        j++;
      }
      //printf("Id:%s\n",gotId);
      //printf("params after id:%s\n",parametri+strlen(destinatar)+strlen(gotId)+2);
      ReplyMessage(clientDet[clientulServit].nume,destinatar,gotId,parametri+strlen(destinatar)+strlen(gotId)+2);
    }
    if(strstr(msg,"history")!=NULL){
      char parametri[1500] = " ";
      strcpy(parametri,msg+strlen("/history "));
      char destinatar[50] = " ";
      int j=0;
      for(int i=0;i<strlen(parametri);i++){
        if(parametri[i]==' ')  break;
        destinatar[j] = parametri[i];
        j++;
      }
      ShowHistory(clientDet[clientulServit].nume, clientDet[clientulServit].descriptor, destinatar);
    }
    if(strstr(msg,"exit")!=NULL){
      bzero(msgrasp,1200);
      sprintf(msgrasp,"Bye bye %s!\n", clientDet[clientulServit].nume);
      if(write(clientDet[clientulServit].descriptor,msgrasp,sizeof(msgrasp)) <= 0){
        perror("write catre client error\n"); return 0;
      }
      printf("Exiting..\n");
      return -1;
    }
  } 
  else{
    bzero(msgrasp,1200);
    //printClientStatsDebug(clientulServit);
    sprintf(msgrasp,"%s:%s\n", clientDet[clientulServit].nume,msg);
    for(int i=0;i<100;i++){
      if(clientDet[i].folosit==1){
        if(write(clientDet[i].descriptor,msgrasp,sizeof(msgrasp)) <= 0){
          perror("write catre client error\n"); return 0;
        }
      }
    }
  }
  return bytes;
}

void disconnectClient(int client_servit){
  clientDet[client_servit].logat=0;
  memcpy(shmem, clientDet, 100*sizeof(struct clientDetails));
}

void loginClient(char * username, int client_servit){
  clientDet[client_servit].logat=1;
  strcpy(clientDet[client_servit].nume,username);
  memcpy(shmem, clientDet, 100*sizeof(struct clientDetails));
}

void printClientStatsDebug(int client_servit){
  printf("Descriptor:%d,\n Nume:%s,\n logat?:%d,\n folosit?:%d\n",clientDet[client_servit].descriptor,clientDet[client_servit].nume,clientDet[client_servit].logat,clientDet[client_servit].folosit);
}

int Login(char * username, int client_servit){
  //printf("Logging in user: \"%s\"...\nSearching for username...\n", username);
  FILE *usr = fopen("users.cfg","r");
  char line[10];
  if(!usr) {
    perror("[Error]User file is NULL\n");
    exit(0);
  }
  //printf("Looking into the file...\n");
  while(fgets(line,sizeof(line),usr))
  {
    line[strlen(line)-1] = '\0';
    if(strcmp(username,line) == 0)
    {
      printf("Found the user...\nLoging in...\n");
      loginClient(username,client_servit);
      printClientStatsDebug(client_servit);
      fclose(usr);
      return 1;
    }
  }
  if(clientDet[client_servit].logat == 0)
    printf("User not found, not logging in.\n");
  fclose(usr);
  return 0;
}
void Register(char * username){
  //printf("Registering user: \"%s\"...\n", username);
  FILE *usr = fopen("users.cfg","r");
  char line[10];
  if(!usr) {
    perror("[Error]User file is NULL\n");
    exit(0);
  }
  //printf("Looking into the file...\n");
  while(fgets(line,sizeof(line),usr))
  {
    line[strlen(line)-1] = '\0';
    if(strcmp(username,line) == 0)
    {
      printf("user already exists!\n");
      return;
    }
  }
  fclose(usr);
  FILE *usrAppend = fopen("users.cfg","a");
  fprintf(usrAppend,"%s\n",username);
  fclose(usrAppend);
}

int SendMessage(char * senderName, char * destination, char * message){
  char messageRasp[1200] = " ";
  struct message messageStruct;
  messageStruct.id=++global_id; saveGlobalCount();
  strcpy(messageStruct.content,message);
  strcpy(messageStruct.sender,senderName);
  sprintf(messageRasp,"<<%s>>:%s",messageStruct.sender,messageStruct.content);
  saveChat(senderName, destination, messageStruct);
  int destinationDesc;
  pid_t asteapta=fork();
  if(asteapta==-1){perror("[Server] Eroare in fork()!\n"); return errno;}
  else if(asteapta==0){
    while(1){
      for(int i=0;i<100;i++){
        if(strcmp(shmem[i].nume,destination)==0){
          if(write(shmem[i].descriptor,messageRasp,strlen(messageRasp))<=0){
            perror("Write error catre client!\n");
            return 2;
            }//tell the client to look into history
          return 1;
        }
      }
      sleep(1);
    }
  }
}

void saveChat(char *user1, char *user2, struct message messageStruct){
  char filename[128];
  char message[1300];
  sprintf(message,"(%ld)<<%s>>%s",messageStruct.id,messageStruct.sender,messageStruct.content);
  sprintf(filename,"%s&%sChat",user1,user2);
  if(access(filename,F_OK) != 0){
    sprintf(filename,"%s&%sChat",user2,user1);
    if(access(filename,F_OK) != 0){
      printf("[Server]File does not exist!\n");
      sprintf(filename,"%s&%sChat",user1,user2);
    }
  }
  FILE *chat = fopen(filename,"a+");
  fprintf(chat,"%s\n",message);
  fclose(chat);
}

void ReplyMessage(char *user1, char *user2, char *id, char *repliedMessage){
  char messageReply[1300];
  messageReply[strlen(messageReply)-1]='\0';
  struct message replyTo = getMessageData(user1,user2,id);
  sprintf(messageReply,"Replied to <<%s>>%s: %s",replyTo.sender, replyTo.content, repliedMessage);
  SendMessage(user1,user2,messageReply);
}

void saveGlobalCount(){
  FILE *usr = fopen("ServerConfig.cfg","r");
  FILE *usrtmp = fopen("tmp","w");
  char line[30];
  if(!usr) {
    perror("[Error]User file is NULL\n");
    exit(0);
  }
  printf("\nLooking into the file...\n");
  while(fgets(line,sizeof(line),usr))
  {
    line[strlen(line)-1] = '\0';
    printf("line:%s strstr:%s\n",line, strstr(line,"global_id_counter"));
    if(strstr(line,"global_id_counter") != NULL)
    {
      printf("GLBCOUTNIS: %s and globalCount=%ld\n",line+strlen("global_id_counter = "),global_id);
      sprintf(line, "global_id_counter = %ld",global_id);
    }
    fprintf(usrtmp,"%s\n",line);
  }
  remove("ServerConfig.cfg");
  rename("tmp","ServerConfig.cfg");
  fclose(usr);
  fclose(usrtmp);
}
void readGlobalCount(){
  FILE *usr = fopen("ServerConfig.cfg","r");
  char line[30];

  if(!usr) {
      perror("[Error]User file is NULL\n");
      exit(0);
  }
  printf("Looking into the file...\n");
  while(fgets(line,sizeof(line),usr))
  {
    line[strlen(line)] = '\0';
    printf("%s",line);
    if(strstr(line,"global_id_counter") != NULL)
    {
      printf("DIN FISIER: %s\n",line+strlen("global_id_counter = "));
      global_id=atoi(line+strlen("global_id_counter = "));
      printf("DIN PROGRAM:%ld\n",global_id);
    }
  }
}

void revstr(char *str1)  
{  
  // declare variable  
  int i, len, temp;  
  len = strlen(str1); // use strlen() to get the length of str string  
    
  // use for loop to iterate the string   
  for (i = 0; i < len/2; i++)  
  {  
    // temp variable use to temporary hold the string  
    temp = str1[i];  
    str1[i] = str1[len - i - 1];  
    str1[len - i - 1] = temp;  
  }  
}  

struct message getMessageData(char *user1, char *user2, char *id){
  struct message replyTo;
  char filename[128], line[1300], message[1300], gotId[10], messageReply[1300];
  char gotUser[50];
  int ch, i;
  int count;

  sprintf(filename,"%s&%sChat",user1,user2);
  if(access(filename,F_OK) != 0){
    sprintf(filename,"%s&%sChat",user2,user1);
    if(access(filename,F_OK) != 0){
      printf("[Server]File does not exist!\n");
      sprintf(filename,"%s&%sChat",user1,user2);
    }
  }
  printf("filename:%s\n",filename);
  FILE *fd = fopen(filename,"a+");
  while(fgets(line,sizeof(line),fd)){
    line[strlen(line)-1]='\0';
    printf("%s\n",line);
    int k=0;
    for(int i=0;i<strlen(line);i++){
      if(line[i]=='('){
        i++;
        while(line[i]!=')') gotId[k++]=line[i++];
      }
    }
    printf("ID:%s GotID:%s; STRCMP:%d\n",id,gotId,strcmp(id,gotId));
    if(strcmp(id,gotId)==0) break;
  }
  strcpy(message,line);
  printf("%s\n",message);
  int k=0;
  i=0;
  while(message[i]!=')') i++;
  i+=3;
  while(message[i]!='>'){
    gotUser[k++]=message[i++];
  }
  i+=2;
  gotUser[strlen(gotUser)]='\0';
  printf("GotUser:%s\n",gotUser);
  strcpy(replyTo.sender,gotUser);
  printf("Message:%s\n",message+i);
  strcpy(replyTo.content,message+i);

  bzero(message,sizeof(message));
  bzero(gotUser,sizeof(gotUser));

  return replyTo;
  fclose(fd);
}

void ShowHistory(char *user1, int U1Desc, char *user2){
  char filename[128], line[1300];
  sprintf(filename,"%s&%sChat",user1,user2);
  if(access(filename,F_OK) != 0){
    sprintf(filename,"%s&%sChat",user2,user1);
    if(access(filename,F_OK) != 0){
      printf("[Server]File does not exist!\n");
      sprintf(filename,"%s&%sChat",user1,user2);
    }
  }
  printf("filename:%s\n",filename);
  FILE *fd = fopen(filename,"r");

  pid_t historyChild=fork();
  if(historyChild == -1) {perror("[Server]history child error!"); return;}
  else if(historyChild == 0){
    while(fgets(line,sizeof(line),fd)){
      line[strlen(line)-1] = '\0';
      //printf("%s -> %ld\n",line,strlen(line));
      write(U1Desc,line,strlen(line));
      sleep(0.2f);
      bzero(line,sizeof(line));
    }
    return;
  }
}