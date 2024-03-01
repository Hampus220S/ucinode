#include "debug.h"
#include "socket.h"
#include "fifo.h"
#include "thread.h"

#include <stdlib.h>
#include <signal.h>

// SIGUSR1 - interrupting threads

pthread_t stdinThread;  // Communication from engine to client
pthread_t stdoutThread; // Communication from client to engine

int serverfd = -1;
int sockfd = -1;

int stdinFIFO = -1;
int stdoutFIFO = -1;

bool acceptNewClient = true; // The UCI node should continue running

// Settings
bool debug = false;
bool reversed = false;

char address[64] = "127.0.0.1";
int port = 5555;

char stdinPathname[64] = "stdin";
char stdoutPathname[64] = "stdout";

/*
 * Communication from client to engine
 */
void* stdout_routine(void* arg)
{
  info_print("Redirecting socket -> fifo");

  char buffer[1024];
  memset(buffer, '\0', sizeof(buffer));

  int status;

  while((status = socket_read(sockfd, buffer, sizeof(buffer))) > 0)
  {
    debug_print(stdout, "socket -> fifo", "%s", buffer);

    // If the client wants to quit
    if(!strncmp(buffer, "quit", 4)) break;

    if(buffer_write(stdinFIFO, buffer, sizeof(buffer)) == -1) break;

    memset(buffer, '\0', sizeof(buffer));
  }
  info_print("Stopped socket -> fifo");

  // If stdin thread has interrupted stdout thread
  if(status == -1 && errno == EINTR)
  {
    info_print("stdout routine interrupted"); 
  }

  // Interrupt stdin thread
  pthread_kill(stdinThread, SIGUSR1);

  return NULL;
}

// Communication from engine to client
void* stdin_routine(void* arg)
{
  info_print("Redirecting fifo -> socket");

  char buffer[1024];
  memset(buffer, '\0', sizeof(buffer));

  int status;

  while((status = buffer_read(stdoutFIFO, buffer, sizeof(buffer))) > 0)
  {
    debug_print(stdout, "fifo -> socket", "%s", buffer);

    if(socket_write(sockfd, buffer, sizeof(buffer)) == -1) break;

    memset(buffer, '\0', sizeof(buffer));
  }
  info_print("Stopped fifo -> socket");

  // If stdout thread has interrupted stdin thread
  if(status == -1 && errno == EINTR)
  {
    info_print("stdin routine interrupted"); 
  }

  // Interrupt stdout thread
  pthread_kill(stdoutThread, SIGUSR1);

  return NULL;
}

/*
 * Keyboard interrupt - close the program (the threads)
 */
void sigint_handler(int signum)
{
  if(debug) info_print("Keyboard interrupt");

  acceptNewClient = false;

  pthread_kill(stdinThread, SIGUSR1);
  pthread_kill(stdoutThread, SIGUSR1);
}

void sigint_handler_setup(void)
{
  struct sigaction sigAction;

  sigAction.sa_handler = sigint_handler;
  sigAction.sa_flags = 0;
  sigemptyset(&sigAction.sa_mask);

  sigaction(SIGINT, &sigAction, NULL);
}

void sigusr1_handler(int signum) {}

void sigusr1_handler_setup(void)
{
  struct sigaction sigAction;

  sigAction.sa_handler = sigusr1_handler;
  sigAction.sa_flags = 0;
  sigemptyset(&sigAction.sa_mask);

  sigaction(SIGUSR1, &sigAction, NULL);
}

void signals_handler_setup(void)
{
  signal(SIGPIPE, SIG_IGN); // Ignores SIGPIPE
  
  sigint_handler_setup();

  sigusr1_handler_setup();
}

/*
 * Accept socket client
 *
 * RETURN
 * - 0 | Success!
 * - 1 | Failed to accept client socket
 * - 2 | Failed to start stdin and stdout threads
 */
int server_process_step3(void)
{
  while(acceptNewClient)
  {
    sockfd = socket_accept(serverfd, address, port, debug);

    if(sockfd == -1) return 1;

    int status = stdin_stdout_thread_start(&stdinThread, &stdin_routine, &stdoutThread, &stdout_routine, debug);

    socket_close(&sockfd, debug);

    if(status != 0) return 2;
  }
  return 0;
}

/*
 * Create server socket
 *
 * RETURN
 * - 0 | Success!
 * - 1 | Failed to create server socket
 * - 2 | Failed server_process_step3
 */
int server_process_step2(void)
{
  serverfd = server_socket_create(address, port, 1, debug);

  if(serverfd == -1) return 1;

  int status = server_process_step3();

  socket_close(&serverfd, debug);

  return (status != 0) ? 2 : 0;
}

/*
 * RETURN
 * - 0 | Success!
 * - 1 | Failed to open stdin and stdout FIFOs
 * - 2 | Failed to close stdin and stdout FIFOs
 * - 3 | Failed server_process_step2
 */
int server_process(void)
{
  if(stdin_stdout_fifo_open(&stdinFIFO, stdinPathname, &stdoutFIFO, stdoutPathname, reversed, debug) != 0) return 1;

  int status = server_process_step2();

  if(stdin_stdout_fifo_close(&stdinFIFO, &stdoutFIFO, debug) != 0) return 2;

  return (status != 0) ? 3 : 0;
}

/*
 * Parse the current passed flag
 *
 * FLAGS
 * --debug             | Output debug messages
 * --reversed          | Open stdout FIFO before stdin FIFO
 * --stdin=<name>      | The name of stdin FIFO
 * --stdout=<name>     | The name of stdout FIFO
 * --address=<address> | The server address
 * --port=<port>       | The server port
 */
void flag_parse(char flag[])
{
  if(!strcmp(flag, "--debug"))
  {
    debug = true;
  }
  else if(!strcmp(flag, "--reversed"))
  {
    reversed = true;
  }
  else if(!strncmp(flag, "--address=", 10))
  {
    strcpy(address, flag + 10);
  }
  else if(!strncmp(flag, "--port=", 7))
  {
    port = atoi(flag + 7);
  }
  else if(!strncmp(flag, "--stdin=", 8))
  {
    strcpy(stdinPathname, flag + 8);
  }
  else if(!strncmp(flag, "--stdout=", 9))
  {
    strcpy(stdoutPathname, flag + 9);
  }
}

/*
 * Parse every passed flag
 */
void flags_parse(int argc, char* argv[])
{
  for(int index = 1; index < argc; index += 1)
  {
    flag_parse(argv[index]);
  }
}

int main(int argc, char* argv[])
{
  flags_parse(argc, argv);

  signals_handler_setup();

  return server_process();
}
