#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>

#define N 8
#define K 8

int states[N];
sem_t sems[N];

void * node(void * data)
{
  int i;
  int c;
  long id = (long)data;
  long prev = id - 1 >= 0 ? id - 1 : N - 1;
  long next = (id + 1) % N;
  int lstates[N];

  printf("#%d started\n",id);
  while (1)
    {
      sem_wait(&sems[id]);
      sleep(1);

      /* count privileges */
      for (i = 0; i < N; i++)
	lstates[i] = states[i];
      c = 0;
      if (lstates[0] == lstates[N-1])
	c++;
      for (i = 1; i < N; i++)
	if (lstates[i] != lstates[i-1])
	  c++;
      printf("#%d see %d privileges\n",id,c);

      /* update state */
      if (id && lstates[prev] != lstates[id])
	{
	  states[id] = lstates[prev];
	  sem_post(&sems[next]);
	  printf("#%d %d->%d\n",id,lstates[id],states[id]);
	}
      if (!id && lstates[prev] == lstates[id])
	{
	  states[id] = (lstates[id] + 1) % K;
	  sem_post(&sems[next]);
	  printf("#%d %d->%d\n",id,lstates[id],states[id]);
	}
    }
}

int main()
{
  pthread_t threads[N];
  int rc;
  long i;

  srand(time(0));

  /* create threads */
  for (i = 0; i < N; i++)
    {
      sem_init(&sems[i],0,0);
      rc = pthread_create(&threads[i],NULL,node,(void *)i); 
      if (rc)
	{
	  printf("ERROR; return code from pthread_create() is %d\n",rc);
	  return -1;
	}
    }

  /* generate random states and signal threads */
  for (i = 0; i < N; i++)
    states[i] = rand() % K;
  for (i = 0; i < N; i++)
    sem_post(&sems[i]);

  /* generate random error */
  sleep(10);
  for (i = 0; i < N; i++)
    states[i] = rand() % K;
  for (i = 0; i < N; i++)
    sem_post(&sems[i]);

  /* join threads */
  for (i = 0; i < N; i++)
    {
      rc = pthread_join(threads[i],NULL);
      sem_destroy(&sems[i]);
      if (rc)
	{
	  printf("ERROR; return code from pthread_join() is %d\n",rc);
	  return -1;
	}
    }
 
  printf("Main: program completed. Exiting.\n");

  pthread_exit(NULL);
  return 0;
}
