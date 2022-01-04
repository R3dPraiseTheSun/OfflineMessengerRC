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
static volatile int keepRunning = 1;

void  INThandler(int sig)
{
  char  c;

  signal(sig, SIG_IGN);
  printf("OUCH, did you hit Ctrl-C?\nDo you really want to quit? [y/n]");
  c = getchar();
  if (c == 'y' || c == 'Y'){
    keepRunning = 0;
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

  signal(SIGINT, INThandler);
  /* citirea mesajului */
  pid_t copil = fork();
  if(copil==-1){
    perror("Fork error!\n"); return errno;
  }
  else if(copil==0){
    while(keepRunning)
    {
      bzero (msg, 1024);
      fflush (stdout);
      read (0, msg, 1024);
      
      /* trimiterea mesajului la server */
      if (write (sd, msg, 1024) <= 0)
      {
        perror ("[Client]Eroare la write() spre server.\n");
        return errno;
      }
      strcpy(msg," ");
    }
    char exitmsg[10] = "Bye bye!";
    write(sd, exitmsg, 10);
    return 0;
  }
  else{
    while(keepRunning)
    {
      if(read(sd,msg,1024) <=0){
        perror("[Client]Eroare la read() de la server!\n"); return errno;
      }
      printf("%s\n",msg);
      strcpy(msg," ");
    }
    wait(0);
  }

  /* inchidem conexiunea, am terminat */
  close (sd);
}