/*
 * Name:        Josh Levy
 * Case ID:     jml312
 * Filename:    proj3.c
 * Created:     10/20/2022
 * Description: This program takes a port, documentary directory, and an authentication token and creates a server that can handle either GET or TERMINATE requests from the client.
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define ERROR 1
#define PROTOCOL "tcp"
#define BUFLEN 1024
#define MIN_PORT 1025
#define QLEN 1
#define NO_SPACE ""
#define SPACE " "
#define SLASH "/"
#define GET "GET"
#define TERMINATE "TERMINATE"

/* exit program with error message */
int errexit(char *format, char *arg)
{
  fprintf(stderr, format, arg);
  fprintf(stderr, "\n");
  exit(ERROR);
}

/* respond to client request with message */
void respond(char *response, int nbytes, int client_socket_descriptor)
{
  nbytes = write(client_socket_descriptor, response, strlen(response));
  if (nbytes < 0)
  {
    errexit("error writing to socket", NULL);
  }
}

int main(int argc, char *argv[])
{
  int opt;
  char *port, *document_directory, *auth_token;
  struct sockaddr_in sin;
  struct sockaddr addr;
  struct protoent *protoinfo;
  unsigned int addrlen;
  int server_socket_descriptor, client_socket_descriptor;
  char buffer[BUFLEN];
  int nbytes;
  char *method, *argument, *http_version;
  char *file_path;
  FILE *file_pointer;

  /* read command line arguments */
  while ((opt = getopt(argc, argv, ":p:r:t:")) != -1)
  {
    switch (opt)
    {
    case 'p':
      port = optarg;
      if (atoi(port) < MIN_PORT)
      {
        errexit("port number must be greater than 1024", NULL);
      }
      break;
    case 'r':
      document_directory = optarg;
      break;
    case 't':
      auth_token = optarg;
      break;
    case ':':
    {
      char optopt_char[2];
      sprintf(optopt_char, "%c", optopt);
      errexit("ERROR: option -%s requires an argument", optopt_char);
      break;
    }
    case '?':
    {
      char optopt_char[2];
      sprintf(optopt_char, "%c", optopt);
      errexit("ERROR: unknown option -%s", optopt_char);
      break;
    }
    }
  }

  /* check for required arguments */
  if (strcmp(port, NO_SPACE) == 0)
  {
    errexit("ERROR: port is required (-p)", NULL);
  }
  if (strcmp(document_directory, NO_SPACE) == 0)
  {
    errexit("ERROR: document directory is required (-r)", NULL);
  }
  if (strcmp(auth_token, NO_SPACE) == 0)
  {
    errexit("ERROR: auth token is required (-t)", NULL);
  }

  /* determine protocol */
  if ((protoinfo = getprotobyname(PROTOCOL)) == NULL)
  {
    errexit("cannot find protocol information for %s", PROTOCOL);
  }

  /* setup endpoint info */
  memset((char *)&sin, 0x0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = INADDR_ANY;
  sin.sin_port = htons(atoi(port));

  /* allocate a socket */
  server_socket_descriptor = socket(PF_INET, SOCK_STREAM, protoinfo->p_proto);
  if (server_socket_descriptor < 0)
  {
    errexit("cannot create socket", NULL);
  }

  /* bind the socket */
  if (bind(server_socket_descriptor, (struct sockaddr *)&sin, sizeof(sin)) < 0)
  {
    errexit("cannot listen on port %s", port);
  }

  /* listen for incoming connections */
  if (listen(server_socket_descriptor, QLEN) < 0)
  {
    errexit("cannot listen on port %s", port);
  }

  while (1)
  {
    /* accept a connection */
    addrlen = sizeof(addr);
    client_socket_descriptor = accept(server_socket_descriptor, &addr, &addrlen);
    if (client_socket_descriptor < 0)
    {
      errexit("error accepting connection", NULL);
    }

    /* read from socket */
    nbytes = read(client_socket_descriptor, buffer, BUFLEN);
    if (nbytes < 0)
    {
      errexit("error reading from socket", NULL);
    }

    /* parse method, argument, and http version from buffer */
    if ((method = strtok(buffer, SPACE)) == NULL)
    {
      respond("HTTP/1.1 400 Malformed Request\r\n\r\n", nbytes, client_socket_descriptor);
      close(client_socket_descriptor);
      continue;
    }

    if ((argument = strtok(NULL, SPACE)) == NULL)
    {
      respond("HTTP/1.1 400 Malformed Request\r\n\r\n", nbytes, client_socket_descriptor);
      close(client_socket_descriptor);
      continue;
    }

    if ((http_version = strtok(NULL, "\r")) == NULL)
    {
      respond("HTTP/1.1 400 Malformed Request\r\n\r\n", nbytes, client_socket_descriptor);
      close(client_socket_descriptor);
      continue;
    }

    char *rest = strtok(NULL, NO_SPACE);
    if (rest == NULL)
    {
      respond("HTTP/1.1 400 Malformed Request\r\n\r\n", nbytes, client_socket_descriptor);
      close(client_socket_descriptor);
      continue;
    }
    /* remove last trailing \n for comparison */
    rest[strlen(rest) - 1] = '\0';
    if (strncmp(rest, "\n\r\n", 3) != 0)
    {
      respond("HTTP/1.1 400 Malformed Request\r\n\r\n", nbytes, client_socket_descriptor);
      close(client_socket_descriptor);
      continue;
    }

    if (strncmp(http_version, "HTTP/", 5) != 0)
    {
      respond("HTTP/1.1 501 Protocol Not Implemented\r\n\r\n", nbytes, client_socket_descriptor);
      close(client_socket_descriptor);
      continue;
    }

    if (strcmp(method, GET) != 0 && strcmp(method, TERMINATE) != 0)
    {
      respond("HTTP/1.1 405 Unsupported Method\r\n\r\n", nbytes, client_socket_descriptor);
      close(client_socket_descriptor);
      continue;
    }

    if (strcmp(method, GET) == 0)
    {
      /* check if argument doesn't start with a slash or ends with a slash */
      if ((strncmp(argument, SLASH, 1) != 0) || (strlen(argument) > 1 && strcmp(argument + strlen(argument) - 1, SLASH) == 0))
      {
        respond("HTTP/1.1 406 Invalid Filename\r\n\r\n", nbytes, client_socket_descriptor);
        close(client_socket_descriptor);
        continue;
      }
      else
      {
        if (strcmp(argument, SLASH) == 0)
        {
          argument = "/homepage.html";
        }
        file_path = malloc(strlen(document_directory) + strlen(argument) + 1);
        strcpy(file_path, document_directory);
        strcat(file_path, argument);

        file_pointer = fopen(file_path, "r");
        if (file_pointer == NULL)
        {
          respond("HTTP/1.1 404 File Not Found\r\n\r\n", nbytes, client_socket_descriptor);
          close(client_socket_descriptor);
          continue;
        }

        respond("HTTP/1.1 200 OK\r\n\r\n", nbytes, client_socket_descriptor);
        while (fgets(buffer, BUFLEN, file_pointer) != NULL)
        {
          respond(buffer, nbytes, client_socket_descriptor);
        }
        fclose(file_pointer);
        close(client_socket_descriptor);
        continue;
      }
    }
    else if (strcmp(method, TERMINATE) == 0)
    {
      if (strcmp(argument, auth_token) == 0)
      {
        respond("HTTP/1.1 200 Server Shutting Down\r\n\r\n", nbytes, client_socket_descriptor);
        close(client_socket_descriptor);
        close(server_socket_descriptor);
        exit(ERROR);
      }
      else
      {
        respond("HTTP/1.1 403 Operation Forbidden\r\n\r\n", nbytes, client_socket_descriptor);
        close(client_socket_descriptor);
        continue;
      }
    }
  }
  return 0;
}