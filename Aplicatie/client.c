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

/* codul de eroare returnat de anumite apeluri */
extern int errno;

int sd;			// descriptorul de socket
static volatile int keepRunning=1;

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

void  INThandler(int sig)
{
  char  c;

  signal(sig, SIG_IGN);
  printf("OUCH, did you hit Ctrl-C?\nDo you really want to quit? [y/n]");
  c = getchar();
  if (c == 'y' || c == 'Y'){
    char exitmsg[10] = "/exit\n";
    write(sd, exitmsg, 10);
    keepRunning = 0;
    kill(0,SIGKILL);
    exit(0);
  }
  else
      signal(SIGINT, INThandler);
  getchar(); // Get new line character
}//https://stackoverflow.com/questions/4217037/catch-ctrl-c-in-c

/* portul de conectare la server*/
int port;

int main (int argc, char *argv[])
{
  struct sockaddr_in server;	// structura folosita pentru conectare 
  char msg[1024];		// mesajul trimis
  /* exista toate argumentele in linia de comanda? */
  if (argc != 3)
    {
      printf ("[client] Sintaxa: %s <adresa_server> <port>\n", argv[0]);
      return -1;
    }

  /* stabilim portul */
  port = atoi (argv[2]);

  /* cream socketul */
  if ((sd = socket (AF_INET, SOCK_STREAM, 0)) == -1)
    {
      perror ("[client] Eroare la socket().\n");
      return errno;
    }
  

  /* umplem structura folosita pentru realizarea conexiunii cu serverul */
  /* familia socket-ului */
  server.sin_family = AF_INET;
  /* adresa IP a serverului */
  server.sin_addr.s_addr = inet_addr(argv[1]);
  /* portul de conectare */
  server.sin_port = htons (port);
  
  /* ne conectam la server */
  if (connect (sd, (struct sockaddr *) &server,sizeof (struct sockaddr)) == -1)
  {
    perror ("[client]Eroare la connect().\n");
    return errno;
  }

  /* citirea mesajului */
  pid_t copil = fork();
  if(copil==-1){
    perror("Fork error!\n"); return errno;
  }
  else if(copil==0){
    signal(SIGINT, INThandler);
    while(keepRunning)
    {
      bzero (msg, 1024);
      fflush (stdout);
      read (0, msg, 1024);
      
      /* trimiterea mesajului la server */
      if(keepRunning)
        if (write (sd, msg, 1024) <= 0){
          perror ("[Client]Eroare la write() spre server.\n");
          return errno;
        }
      strcpy(msg," ");
    }
    return 0;
  }
  else{
    signal(SIGINT, INThandler);
    while(keepRunning)
    {
      if(keepRunning)
        if(read(sd,msg,sizeof(msg)) <=0){
          perror("[Client]Eroare la read() de la server!\n"); return errno;
        }
      printf("%s\n",msg);
      bzero(msg,sizeof(msg));
    }
    wait(0);
  }

  /* inchidem conexiunea, am terminat */
  close (sd);
}