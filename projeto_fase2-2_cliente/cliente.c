#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "my_protocol.h"

#define SERVER_PORT 10007		/* arbitrary, but client and server must agree */
#define BUF_SIZE 4096			/* block transfer size */

void create_json_string(char *json, char *type, char *origin, char *destination, char *message);

struct sockaddr_in init_client_info(char* name)
{
    struct hostent *h; /* info about server */
    h = gethostbyname(name);		/* look up host's IP address */
    if (!h)
    {
        perror("gethostbyname");
        exit(-1);
    }
    struct sockaddr_in channel;
    memset(&channel, 0, sizeof(channel));
    channel.sin_family= AF_INET;
    memcpy(&channel.sin_addr.s_addr, h->h_addr, h->h_length);
    channel.sin_port= htons(SERVER_PORT);
    return channel;
}

int main(int argc, char **argv)
{
    char options[] = "\n2 - Broadcast\n3 - Private\n4 - WhoIsOnline\n5 - Create\n6 - Join\n7 - Leave\n8 - ShowRooms\n9 - Ban\n\n";
    char json[200] = "";
    char type[2] = "";
    char origin[10];
    char group_id[2] = "";
    char destination[10];
    char message[50];

    int c, s, bytes;
    char buf[BUF_SIZE];
    struct sockaddr_in channel;

    if (argc != 2)
    {
        printf("Usage: is-client server-name\n");
        exit(-1);
    }
    s = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s < 0)
    {
        perror("socket");
        exit(-1);
    }
    channel=init_client_info(argv[1]);
    c = connect(s, (struct sockaddr *) &channel, sizeof(channel));
    if (c < 0)
    {
        perror("connect");
        exit(-1);
    }
    else
    {
        write(1, "Username: ", 10);
        bytes=read(0,origin,BUF_SIZE);
        origin[bytes-1]='\0';

        /** type = 1 -> registration */
        create_json_string(json, "1", origin, "", "");
        write(s, json, strlen(json) + 1);
        write(1, options, sizeof(options));
    }

    fd_set rset;
    for(;;)
    {
        FD_ZERO(&rset);
        FD_SET(s,&rset);
        FD_SET(0,&rset);
        int maxfd=s;
        select(maxfd+1,&rset,NULL,NULL,NULL);

        /** Recebe mensagem do socket */
        if(FD_ISSET(s,&rset))
        {
            bytes = read(s, buf, BUF_SIZE);
            /** Se bytes = 0 -> desconectou */
            if(bytes == 0)
            {
                close(s);
                break;
            }
            buf[bytes]='\0';
            /** Converte mensagem em estrutura com os campos da mensagem */
            PROTOCOL* protocol=parse_message(buf);

            /** type = 8 -> ShowRooms */
            if(protocol->type == 8)
            {
                /** Escolha da sala */
                write(1, "Sala disponivel: ", 16);
                write(1, protocol->message, strlen(protocol->message));
                write(1, "\n", 1);
            }
            /** type = 2 -> Broadcast */
            else if(protocol->type == 2)
            {
                /** Imprime origem e mensagem recebida */
                write(1, protocol->origin, strlen(protocol->origin));
                write(1, ": ", 2);
                write(1, protocol->message, strlen(protocol->message));
                write(1, "\n", 1);
            }
            /** type = 1 -> Registration */
            else if(protocol->type == 1)
            {
                /** Indica que clientes se conectam */
                write(1, protocol->origin, strlen(protocol->origin));
                write(1, " conectado.\n", 12);
            }
            /** type = 3 -> Private */
            else if(protocol->type == 3)
            {
                /** Imprime origem e mensagem recebida */
                write(1, "[PRIVATE]", 9);
                write(1, protocol->origin, strlen(protocol->origin));
                write(1, ": ", 2);
                write(1, protocol->message, strlen(protocol->message));
                write(1, "\n", 1);
            }
            /** type = 4 -> Whoisonline */
            if(protocol->type == 4)
            {
                write(1, "Cliente online: ", 16);
                write(1, protocol->message, strlen(protocol->message));
                write(1, "\n", 1);
            }
            /** type = 7 -> Leave */
            if(protocol->type == 7)
            {
                write(1, protocol->origin, strlen(protocol->origin));
                write(1, " desconectado.\n", 15);
            }
            /** type = 9 -> Ban */
            if(protocol->type == 9)
            {
                write(1, protocol->message, strlen(protocol->message));
                write(1, "\n", 1);
                return 0;
            }
        }
        else if(FD_ISSET(0,&rset)) /** Recebe input do teclado */
        {
            bytes=read(0,type,BUF_SIZE);
            type[bytes-1]='\0';

            /** type = 5 -> Create */
            if(strcmp(type, "5") == 0)
            {
                create_json_string(json, type, origin, "", "");
                write(s, json, strlen(json) + 1);
            }
            /** type = 6 -> Join */
            else if(strcmp(type, "6") == 0)
            {
                write(1, "ID da Sala: ", 12);
                bytes=read(0,group_id,BUF_SIZE);
                group_id[bytes-1]='\0';

                create_json_string(json, type, origin, "", group_id);
                write(s, json, strlen(json) + 1);
            }
            /** type = 2 -> Broadcast */
            else if(strcmp(type, "2") == 0)
            {
                write(1, "Mensagem: ", 10);
                bytes=read(0,message,BUF_SIZE);
                message[bytes-1]='\0';

                create_json_string(json, type, origin, "", message);
                write(s, json, strlen(json) + 1);
            }
            /** type = 3 -> Private */
            else if(strcmp(type, "3") == 0)
            {
                write(1, "Destinatario: ", 14);
                bytes=read(0,destination,BUF_SIZE);
                destination[bytes-1]='\0';

                write(1, "Mensagem: ", 10);
                bytes=read(0,message,BUF_SIZE);
                message[bytes-1]='\0';

                create_json_string(json, type, origin, destination, message);
                write(s, json, strlen(json) + 1);
            }
            /** type = 4 -> Whoisonline */
            else if(strcmp(type, "4") == 0)
            {
                create_json_string(json, type, origin, "", "");
                write(s, json, strlen(json) + 1);
            }
            /** type = 8 -> ShowRooms */
            else if(strcmp(type, "8") == 0)
            {
                create_json_string(json, type, origin, "", "");
                write(s, json, strlen(json) + 1);
            }
            /** type = 7 -> Leave */
            else if(strcmp(type, "7") == 0)
            {
                create_json_string(json, type, origin, "", "");
                write(s, json, strlen(json) + 1);
                return 0;
            }
            /** type = 9 -> Ban */
            else if(strcmp(type, "9") == 0)
            {
                write(1, "BAN: ", 5);
                bytes=read(0,message,BUF_SIZE);
                message[bytes-1]='\0';

                create_json_string(json, type, origin, "", message);
                write(s, json, strlen(json) + 1);
            }
        }
    }
    return 0;
}

void create_json_string(char *json, char *type, char *origin, char *destination, char *message)
{
    json[0] = '\0';
    char* cat_type = "{\"type\":";
    char* cat_origin = ",\"origin\":\"";
    char* cat_destination = "\",\"destination\":\"";
    char* cat_message = "\",\"message\":\"";
    char* cat_end = "\"}";

    strcat(json, cat_type);
    strcat(json, type);
    strcat(json, cat_origin);
    strcat(json, origin);
    strcat(json, cat_destination);
    strcat(json, destination);
    strcat(json, cat_message);
    strcat(json, message);
    strcat(json, cat_end);
}
