#include <mpi.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#define MSG_PING 1
#define MSG_PONG 2

#define MOD 100

#define P_LOST 10
#define P_CONSUME 20

int pid;
int pidc;

int ping = 0;
int pong = 0;
int m = 0;

pthread_mutex_t mx;
pthread_t t;

void sendPing();

void * worker(void * whatever)
{
  printf("#%d m:%d ping:%d pong:%d - ...worker started\n",pid,m,ping,pong);
  sleep(rand() % 7 + 1);
  pthread_mutex_lock(&mx);
  sendPing();
  pthread_mutex_unlock(&mx);
  printf("#%d m:%d ping:%d pong:%d - ...worker done\n",pid,m,ping,pong);
  pthread_exit(NULL);
}

void sendPing()
{
  if (ping > 0)
    {
      MPI_Send(&ping,sizeof(ping),MPI_INT,(pid+1)%pidc,MSG_PING,MPI_COMM_WORLD);
      m = ping;
      ping = 0;
      printf("#%d m:%d ping:%d pong:%d - ping sent\n",pid,m,ping,pong);
    }
}

void sendPong()
{
  if (pong < 0)
    {
      MPI_Send(&pong,sizeof(pong),MPI_INT,(pid+1)%pidc,MSG_PONG,MPI_COMM_WORLD);
      m = pong;
      pong = 0;
      printf("#%d m:%d ping:%d pong:%d - pong sent\n",pid,m,ping,pong);
    }
}

void regenerate(int x)
{
  ping = x > 0 ? x : -x;
  pong = -ping;
}

void incarnate()
{
  ping = ping % MOD + 1;
  pong = - ping;
}

void recvPing(int val)
{
  int sp = 0;
  ping = val;
  printf("#%d m:%d ping:%d pong:%d - ping recv...\n",pid,m,ping,pong);
  sleep(1);
  if (m == ping)
    {
      regenerate(ping);
      pthread_mutex_lock(&mx);
      sendPong();
      pthread_mutex_unlock(&mx);
      printf("#%d m:%d ping:%d pong:%d - ...pong regenerated...\n",pid,m,ping,pong);
    }
  int rnd = rand() % 100;
  if (rnd < P_LOST)
    {
      ping = 0;
      printf("#%d m:%d ping:%d pong:%d - ...OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOPS :(\n",pid,m,ping,pong);
    }
  else if (rnd < P_LOST + P_CONSUME)
    {
      int rc = pthread_create(&t,NULL,worker,NULL);
      if (rc)
	printf("ERROR; return code from pthread_create() is %d\n", rc);
      printf("#%d m:%d ping:%d pong:%d - ...ping consumed\n",pid,m,ping,pong);
    }
  else
    sendPing();
  if (sp)
    {
      pthread_mutex_lock(&mx);
      sendPong();
      pthread_mutex_unlock(&mx);
    }
}

void recvPong(int val)
{
  pong = val;
  printf("#%d m:%d ping:%d pong:%d - pong recv...\n",pid,m,ping,pong);
  pthread_mutex_lock(&mx);
  if (ping > 0)
    {
      incarnate();
      printf("#%d m:%d ping:%d pong:%d - ...incarnation...\n",pid,m,ping,pong);
    }
  if (m == pong)
    {
      regenerate(pong);
      sendPing();
      printf("#%d m:%d ping:%d pong:%d - ...ping regenerated\n",pid,m,ping,pong);
    }
  sleep(1);
  sendPong();
  pthread_mutex_unlock(&mx);
}

int main(int argc, char ** argv)
{
  /* MPI & pthread init */
  int lvl;
  MPI_Init_thread(&argc, &argv,MPI_THREAD_SERIALIZED,&lvl);
  if (lvl != MPI_THREAD_SERIALIZED)
    {
      printf("Lvl not reached\n");
      return 0;
    }
  pthread_mutex_init(&mx,NULL);
  MPI_Comm_size(MPI_COMM_WORLD, &pidc);
  MPI_Comm_rank(MPI_COMM_WORLD, &pid);
  printf("#%d N:%d - started\n",pid,pidc);
  srand(time(0)*pid);

  /* start the party */
  if (pid == 0)
    {
      ping = 1;
      /* pong = -1; */
      sendPing();
      /* sendPong(); */
    }

  /* recv messages */
  while (1)
    {
      MPI_Status status;
      int val;
      MPI_Recv(&val,sizeof(val),MPI_INT,MPI_ANY_SOURCE,MPI_ANY_TAG,MPI_COMM_WORLD,&status);
      if (status.MPI_TAG == MSG_PING)
	recvPing(val);
      else
	recvPong(val);
    }

  MPI_Finalize();
  pthread_mutex_destroy(&mx);
  pthread_exit(NULL);

  return 0;
}
