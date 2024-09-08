/*
 * Written by Hampus Fridholm
 *
 * Last updated: 2024-09-08
 */

#include "fifo.h"

/*
 * Open fifo for reading (input/stdin fifo)
 * 
 * Optionally print debug messages
 *
 * RETURN (int status)
 * - 0 | Success
 * - 1 | Missing address for stdin fifo
 * - 2 | Missing path to stdin fifo
 * - 3 | Failed to open stdin fifo
 */
static int stdin_fifo_open(int* fifo, const char* path, bool debug)
{
  if(!fifo)
  {
    if(debug) error_print("Missing address for stdin fifo");

    return 1;
  }

  if(!path)
  {
    if(debug) error_print("Missing path to stdin fifo");

    return 2;
  }

  if(debug) info_print("Opening stdin fifo (%s)", path);

  if((*fifo = open(path, O_RDONLY)) == -1)
  {
    if(debug) error_print("Failed to open stdin fifo (%s)", path);
    
    return 3;
  }

  if(debug) info_print("Opened stdin fifo (%s): (%d)", path, *fifo);

  return 0;
}

/*
 * Open fifo for writing (output/stdout fifo)
 * 
 * Optionally print debug messages
 *
 * RETURN (int status)
 * - 0 | Success
 * - 1 | Missing address for stdout fifo
 * - 2 | Missing path to stdout fifo
 * - 3 | Failed to open stdout fifo
 */
static int stdout_fifo_open(int* fifo, const char* path, bool debug)
{
  if(!fifo)
  {
    if(debug) error_print("Missing address for stdout fifo");

    return 1;
  }

  if(!path)
  {
    if(debug) error_print("Missing path to stdout fifo");

    return 2;
  }

  if(debug) info_print("Opening stdout fifo (%s)", path);

  if((*fifo = open(path, O_WRONLY)) == -1)
  {
    if(debug) error_print("Failed to open stdout fifo (%s)", path);

    return 3;
  }
  
  if(debug) info_print("Opened stdout fifo (%s): (%d)", path, *fifo);

  return 0;
}

/*
 * Close fifo and mark it as closed for future potential use
 *
 * Optionally print debug messages
 *
 * Note: If no open fifo is supplied, nothing is done
 *
 * RETURN (int status)
 * - 0 | Success
 * - 1 | Failed to close fifo
 */
int fifo_close(int* fifo, bool debug)
{
  if(!fifo || *fifo == -1) return 0;

  if(debug) info_print("Closing fifo (%d)", *fifo);

  if(close(*fifo) == -1)
  {
    if(debug) error_print("Failed to close fifo: %s", strerror(errno));

    return 1;
  }

  if(debug) info_print("Closed fifo");

  *fifo = -1;

  return 0;
}

/*
 * PARAMS
 * - bool reverse
 *   - true  | First open stdin then stdout
 *   - false | First open stdout then stdin
 *
 * RETURN (int status)
 * [IMPORTANT] Same as stdin_stdout_fifo_open
 *
 * Note: This is a very nice programming concept
 *       I have never seen it being used before
 */
int stdout_stdin_fifo_open(int* stdout_fifo, const char* stdout_path, int* stdin_fifo, const char* stdin_path, bool reverse, bool debug)
{
  if(reverse) return stdin_stdout_fifo_open(stdin_fifo, stdin_path, stdout_fifo, stdout_path, !reverse, debug);

  if(stdout_fifo_open(stdout_fifo, stdout_path, debug) != 0)
  {
    return 2;
  }

  if(stdin_fifo_open(stdin_fifo, stdin_path, debug) != 0)
  {
    return 1;
  }

  return 0;
}

/*
 * Special:
 * The function expects both fifos to be opened.
 * It will fail if not both stdin and stdout pahts are supplied
 *
 * PARAMS
 * - bool reverse
 *   - true  | First open stdout then stdin
 *   - false | First open stdin then stdout
 *
 * RETURN (int status)
 * - 0 | Success
 * - 1 | Failed to open stdin fifo
 * - 2 | Failed to open stdout fifo
 */
int stdin_stdout_fifo_open(int* stdin_fifo, const char* stdin_path, int* stdout_fifo, const char* stdout_path, bool reverse, bool debug)
{
  if(reverse) return stdout_stdin_fifo_open(stdout_fifo, stdout_path, stdin_fifo, stdin_path, !reverse, debug);

  if(stdin_fifo_open(stdin_fifo, stdin_path, debug) != 0)
  {
    return 1;
  }

  if(stdout_fifo_open(stdout_fifo, stdout_path, debug) != 0)
  {
    return 2;
  }

  return 0;
}

/*
 * RETURN (ssize_t size)
 * - >0 | The number of read characters
 * -  0 | End of File
 * - -1 | Failed to read buffer
 */
ssize_t buffer_read(int fd, char* buffer, size_t size)
{
  if(errno != 0) return -1;

  if(!buffer) return 0;

  char symbol = '\0';
  ssize_t index;

  for(index = 0; index < size && symbol != '\n'; index++)
  {
    ssize_t status = read(fd, &symbol, 1);

    if(status == -1 || errno != 0) return -1; // ERROR

    buffer[index] = symbol;

    if(status == 0) return 0; // End Of File
  }

  return index;
}

/*
 * RETURN (ssize_t size)
 * - >0 | The number of written characters
 * -  0 | End of File? (I think)
 * - -1 | Failed to write to buffer
 */
ssize_t buffer_write(int fd, const char* buffer, size_t size)
{
  if(errno != 0) return -1;

  if(!buffer) return 0;

  ssize_t index;
  char symbol = '\0';

  for(index = 0; index < size && symbol != '\n'; index++)
  {
    symbol = buffer[index];

    ssize_t status = write(fd, &symbol, 1);

    if(status == -1 || errno != 0) return -1;

    if(status == 0) return 0; // End Of File
    
    if(symbol == '\0') break;
  }

  return index;
}

/*
 * buffer_write, but with just a string instead of an allocated buffer
 */
ssize_t message_write(int fd, const char* message)
{
  return buffer_write(fd, message, strlen(message));
}
