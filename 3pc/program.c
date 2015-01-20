#include <mpi.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#define CAN_COMMIT 1
#define YES 2
#define NO 3
#define PRE_COMMIT 4
#define ABORT 5
#define ACK 6
#define DO_COMMIT 7

#define READY 1
#define WAITING 2
#define PREPARED 3
#define COMMITED 4
#define ABORTED 5

#define FAIL_PROB 10

int pidc;
int pid;
int nvm;
int state = READY;

int main(int argc, char ** argv)
{
  /* MPI init */
  MPI_Init(&argc, &argv);
  MPI_Comm_size(MPI_COMM_WORLD, &pidc);
  MPI_Comm_rank(MPI_COMM_WORLD, &pid);
  printf("#%d N:%d - started\n",pid,pidc);
  srand(time(0)*pid);

  MPI_Status status;
  MPI_Request request;
  int flag;
  int i;

  if (pid == 0)
    {
      /* cooridinator phase 1 */
      for (i = 1; i < pidc; i++)
	MPI_Send(&nvm,sizeof(nvm),MPI_INT,i,CAN_COMMIT,MPI_COMM_WORLD);
      state = WAITING;

      /* cooridinator phase 2 */
      if (state == WAITING)
	{
	  printf("#%d WAITING\n",pid);
	  sleep(2);
	  int cnt = 0;
	  for (i = 1; i < pidc; i++)
	    {
	      MPI_Irecv(&nvm,sizeof(nvm),MPI_INT,MPI_ANY_SOURCE,MPI_ANY_TAG,MPI_COMM_WORLD,&request);
	      MPI_Test(&request,&flag,&status);
	      if (flag && status.MPI_TAG == YES)
		cnt++;
	    }
	  printf("#%d %dxYES\n",pid,cnt);
	  if (cnt == pidc - 1)
	    {
	      for (i = 1; i < pidc; i++)
		if ((rand() % 100) >= FAIL_PROB)
		  MPI_Send(&nvm,sizeof(nvm),MPI_INT,i,PRE_COMMIT,MPI_COMM_WORLD);
	      state = PREPARED;
	    }
	  else
	    {
	      for (i = 1; i < pidc; i++)
		MPI_Send(&nvm,sizeof(nvm),MPI_INT,i,ABORT,MPI_COMM_WORLD);
	      state = ABORTED;
	    }
	}

      /* cooridinator phase 3 */
      if (state == PREPARED)
	{
	  printf("#%d PREPARED\n",pid);
	  sleep(2);
	  int cnt = 0;
	  for (i = 1; i < pidc; i++)
	    {
	      MPI_Irecv(&nvm,sizeof(nvm),MPI_INT,MPI_ANY_SOURCE,MPI_ANY_TAG,MPI_COMM_WORLD,&request);
	      MPI_Test(&request,&flag,&status);
	      if (flag && status.MPI_TAG == ACK)
		cnt++;
	    }
	  printf("#%d %dxACK\n",pid,cnt);
	  if ((rand() % 100) < FAIL_PROB)
	    {
	      printf("#%d FAILED\n",pid);
	      state = ABORTED;
	    }
	  if (cnt == pidc - 1)
	    {
	      for (i = 1; i < pidc; i++)
		if ((rand() % 100) >= FAIL_PROB)
		  MPI_Send(&nvm,sizeof(nvm),MPI_INT,i,DO_COMMIT,MPI_COMM_WORLD);
	      state = COMMITED;
	    }
	  else
	    {
	      for (i = 1; i < pidc; i++)
		MPI_Send(&nvm,sizeof(nvm),MPI_INT,i,ABORT,MPI_COMM_WORLD);
	      state = ABORTED;
	    }
	}

      /* cooridinator aborted */
      if (state == ABORTED)
	{
	  printf("#%d ABORTED\n",pid);
	}

      /* cooridinator commited */
      if (state == COMMITED)
	{
	  printf("#%d COMMITED\n",pid);
	}
    }
  else
    {
      /* node phase 1 */
      sleep(1);
      MPI_Irecv(&nvm,sizeof(nvm),MPI_INT,MPI_ANY_SOURCE,MPI_ANY_TAG,MPI_COMM_WORLD,&request);
      MPI_Test(&request,&flag,&status);
      if (flag)
	{
	  printf("#%d -> #%d CAN_COMMIT\n",status.MPI_SOURCE,pid);
	  if ((rand() % 100) < FAIL_PROB)
	    {
	      printf("#%d FAILED\n",pid);
	      state = ABORTED;
	    }
	  else
	    {
	      state = WAITING;
	      MPI_Send(&nvm,sizeof(nvm),MPI_INT,0,YES,MPI_COMM_WORLD);
	    }
	}

      /* node phase 2 */
      if (state == WAITING)
	{
	  printf("#%d WAITING\n",pid);
	  sleep(2);
	  MPI_Irecv(&nvm,sizeof(nvm),MPI_INT,MPI_ANY_SOURCE,MPI_ANY_TAG,MPI_COMM_WORLD,&request);
	  MPI_Test(&request,&flag,&status);
	  if (flag)
	    {
	      if (status.MPI_TAG == ABORT)
		{
		  printf("#%d -> #%d ABORT\n",status.MPI_SOURCE,pid);
		  state = ABORTED;
		}
	      else if ((rand() % 100) < FAIL_PROB)
		{
		  printf("#%d FAILED\n",pid);
		  state = ABORTED;
		}
	      else
		{
		  printf("#%d -> #%d PRE_COMMIT\n",status.MPI_SOURCE,pid);
		  MPI_Send(&nvm,sizeof(nvm),MPI_INT,0,ACK,MPI_COMM_WORLD);
		  state = PREPARED;
		}
	    }
	  else
	    {
	      printf("#%d TIMEOUT\n",pid);
	      state = ABORTED;
	    }
	}

      /* node phase 3 */
      if (state == PREPARED)
	{
	  printf("#%d PREPARED\n",pid);
	  sleep(2);
	  MPI_Irecv(&nvm,sizeof(nvm),MPI_INT,MPI_ANY_SOURCE,MPI_ANY_TAG,MPI_COMM_WORLD,&request);
	  MPI_Test(&request,&flag,&status);
	  if (flag)
	    {
	      if (status.MPI_TAG == ABORT)
		{
		  printf("#%d -> #%d ABORT\n",status.MPI_SOURCE,pid);
		  state = ABORTED;
		}
	      else
		{
		  state = COMMITED;
		}
	    }
	  else
	    {
	      printf("#%d TIMEOUT (COMMIT)\n",pid);
	      state = COMMITED;
	    }
	}

      /* node aborted */
      if (state == ABORTED)
	{
	  printf("#%d ABORTED\n",pid);
	}

      /* node commited */
      if (state == COMMITED)
	{
	  printf("#%d COMMITED\n",pid);
	}
    }

  MPI_Finalize();
  return 0;
}
