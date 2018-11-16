/* The purpose of this program is to practice writing signal handling
 * functions and observing the behaviour of signals.
 */

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>

/* Message to print in the signal handling function. */
#define MESSAGE "%ld reads were done in %ld seconds.\n"

/* Global variables to store number of read operations and seconds elapsed. 
 */
long num_reads, seconds;

/* The first command-line argument is the number of seconds to set a timer to run.
 * The second argument is the name of a binary file containing 100 ints.
 * Assume both of these arguments are correct.
 */

void handler(int signo)
{
  printf(MESSAGE, num_reads, seconds);
  exit(0);
}

int main(int argc, char **argv)
{
  if (argc != 3)
  {
    fprintf(stderr, "Usage: time_reads s filename\n");
    exit(1);
  }
  seconds = strtol(argv[1], NULL, 10);

  FILE *fp;
  if ((fp = fopen(argv[2], "r")) == NULL)
  {
    perror("fopen");
    exit(1);
  }

  struct itimerval timer = {
      .it_interval = {.tv_sec = 1, .tv_usec = 0},
      .it_value = {.tv_sec = seconds, .tv_usec = 0}};

  struct sigaction sig = {
      .sa_handler = handler,
      .sa_flags = 0};

  sigemptyset(&sig.sa_mask);

  if (setitimer(ITIMER_PROF, &timer, NULL) != 0)
  {
    perror("setitimer");
  }

  sigaction(SIGPROF, &sig, NULL);

  /* In an infinite loop, read an int from a random location in the file,
     * and print it to stderr.
     */

  int val = 0;
  int loc = 0;
  for (;;)
  {
    loc = rand() % 100;
    fseek(fp, loc, SEEK_SET);
    fread(&val, 1, 1, fp);
    printf("%d\n", val);
    num_reads++;
  }
  return 1; // something is wrong if we ever get here!
}
