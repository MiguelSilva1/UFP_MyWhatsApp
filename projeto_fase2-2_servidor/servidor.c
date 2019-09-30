#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include "my_protocol.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <errno.h>
#include <semaphore.h>

#define SERV_PORT 10007
#define QUEUE_SIZE 10
#define BUF_SIZE 1024
/**vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv*/
#define N 10
#define BUFFER_SIZE 5


struct sockaddr_in init_server_info()
{
    struct sockaddr_in servaddr;
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(SERV_PORT);
    return servaddr;
}

typedef struct roomclient
{
    char* pname;
    int connfd;
    struct roomclient* pnext;
} ROOMCLIENT;

typedef struct room
{
    int roomid;
    unsigned long thread_id;
    ROOMCLIENT *pfirst; /** moderador */
    int nroomclients;
    struct room* pnext;
    fd_set allset;
    ROOMCLIENT *pfirst_banned;
} ROOM;

typedef struct roomslist
{
    ROOM *pfirst;
    int nrooms;
} ROOMSLIST;

struct choose_room_struct
{
    int connfd;
};

struct room_struct
{
    ROOM *room;
};

void hdl (int sig)
{
}

void create_room(ROOMSLIST *prl, char *name, int connfd);
void print_rooms(ROOMSLIST rl);
void print_room_clients(ROOMSLIST rl, int room_id);
void insert_room_client(ROOMSLIST *prl, int room_id, char *name, int connfd);
void remove_client(ROOMSLIST *prl, int room_id, int connfd);
void insert_room_client_ban(ROOMSLIST *prl, int room_id, char *name, int connfd);
void print_room_clients_banned(ROOMSLIST rl, int room_id);
void create_json_string(char *json, char *type, char *origin, char *destination, char *message);
void* choose_room();
void* room_thread(void* parameters);

ROOMSLIST rooms_list = {NULL, 0};
/**vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv*/
pthread_mutex_t trinco_produtor = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t trinco_consumidor = PTHREAD_MUTEX_INITIALIZER;
sem_t pode_produzir;
sem_t pode_consumir;
int count, in, out;
int buffer[BUFFER_SIZE];
void produz(int connfd);
int consome();
/**^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/
int main(int argc, char **argv)
{
    int maxfd, listenfd;
    fd_set rset;
    struct sockaddr_in cliaddr, servaddr;
    listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    servaddr = init_server_info();
    bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
    listen(listenfd, QUEUE_SIZE);
    maxfd = listenfd;

/**vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv*/
    int i = 0;
    int n_cons = 3;

    sem_init(&pode_produzir, 0, N);
    sem_init(&pode_consumir, 0, 0);

    pthread_t choose_room_thread_id[n_cons];

    for(i = 0; i < n_cons; i++)
    {
        pthread_create(&choose_room_thread_id[i], NULL, &choose_room, NULL);
    }

    for(;;)
    {
        FD_ZERO(&rset);
        FD_SET(listenfd, &rset);
        int nready = pselect(maxfd + 1, &rset, NULL, NULL, NULL, NULL);

        /** Se é um novo cliente */
        if (FD_ISSET(listenfd, &rset))
        {
            printf("\nNovo cliente conectado.\n");
            socklen_t clilen = sizeof(cliaddr);
            int connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &clilen);
/**vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv*/
            sem_wait(&pode_produzir);
            pthread_mutex_lock(&trinco_produtor);

            produz(connfd);

            pthread_mutex_unlock(&trinco_produtor);
            sem_post(&pode_consumir);

            printf("Item inserido: %d\n", connfd);
        }
    }

    for(i = 0; i < n_cons; i++)
    {
        pthread_join(choose_room_thread_id[i], NULL);
    }

    sem_destroy(&pode_produzir);
    sem_destroy(&pode_consumir);
/**^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

    return 0;
}

void* choose_room()
{
/**vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv*/
    for(;;)
    {
        int connfd;

        sem_wait(&pode_consumir);
        pthread_mutex_lock(&trinco_consumidor);

        connfd = consome();

        pthread_mutex_unlock(&trinco_consumidor);
        sem_post(&pode_produzir);

        printf("Item removido: %d\n", connfd);
/**^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/
        char buf[BUF_SIZE];
        int room_id;
        pthread_t room_thread_id;
        struct room_struct room_args;

        int bytes = read(connfd, buf, BUF_SIZE);
        buf[bytes]='\0';
        /** Protocolo json */
        PROTOCOL* protocol=parse_message(buf);
        ROOM *room_aux;
        char json[200] = "";
        char roomid_aux[2]= "";
        ROOM *room_aux2;
        ROOMCLIENT* roomclient_aux;
        ROOMCLIENT* roomclient_aux2;
        int pode_inserir_na_sala = 1;

        /** Se for o primeiro cliente, cria sala */
        if(rooms_list.pfirst == NULL)
        {
            create_room(&rooms_list, protocol->origin, connfd);
            print_rooms(rooms_list);
            print_room_clients(rooms_list, 0);
            printf("\n");

            FD_ZERO(&rooms_list.pfirst->allset);
            FD_SET(connfd, &rooms_list.pfirst->allset);
            room_args.room = rooms_list.pfirst;
            pthread_create(&room_thread_id, NULL, &room_thread, &room_args);
        }
        /** Se já há salas */
        else
        {
            /** Envia lista de salas disponíveis ao cliente */
            room_aux = rooms_list.pfirst;
            while(room_aux != NULL)
            {
                sprintf(roomid_aux, "%d", room_aux->roomid);
                /** type = 8 -> ShowRooms */
                create_json_string(json, "8", "", protocol->origin, roomid_aux);
                write(connfd, json, strlen(json));
                room_aux = room_aux->pnext;
                sleep(1);
            }

            bytes = read(connfd, buf, BUF_SIZE);
            buf[bytes]='\0';
            /** Converte mensagem em estrutura com os campos da mensagem */
            protocol=parse_message(buf);

            /** type = 5 -> Create */
            if(protocol->type == 5)
            {
                create_room(&rooms_list, protocol->origin, connfd);

                print_rooms(rooms_list);
                ROOM *room_aux2 = rooms_list.pfirst;
                while(room_aux2->pnext != NULL)
                {
                    room_aux2 = room_aux2->pnext;
                }
                print_room_clients(rooms_list, room_aux2->roomid);
                printf("\n");

                FD_ZERO(&room_aux2->allset);
                FD_SET(room_aux2->pfirst->connfd, &room_aux2->allset);
                room_args.room = room_aux2;
                pthread_create(&room_thread_id, NULL, &room_thread, &room_args);
            }
            /** type = 6 -> Join */
            else if(protocol->type == 6)
            {
                room_id = atoi(protocol->message);
                /** Procura pela sala */
                room_aux2 = rooms_list.pfirst;
                while(room_aux2 != NULL)
                {
                    if(room_aux2->roomid == room_id)
                    {
                        /** Verifica se já há algum cliente com o mesmo nome */
                        roomclient_aux2 = room_aux2->pfirst;
                        while(roomclient_aux2 != NULL)
                        {
                            if(strcmp(roomclient_aux2->pname, protocol->origin) == 0)
                            {
                                create_json_string(json, "9", "", "", "Nome em uso.");
                                write(connfd, json, strlen(json));
                                close(connfd);

                                pode_inserir_na_sala = 0;
                            }
                            roomclient_aux2 = roomclient_aux2->pnext;
                        }
                        /** Verifica se sala está cheia (5 clientes) */
                        if(room_aux2->nroomclients == 5)
                        {
                            create_json_string(json, "9", "", "", "Sala cheia.");
                            write(connfd, json, strlen(json));
                            close(connfd);

                            pode_inserir_na_sala = 0;
                        }

                        /** Verifica se está banido */
                        roomclient_aux = room_aux2->pfirst_banned;
                        while(roomclient_aux != NULL)
                        {
                            /** Se estiver banido, informa-o e termina */
                            if(strcmp(roomclient_aux->pname, protocol->origin) == 0)
                            {
                                create_json_string(json, "9", "", "", "Banido desta sala.");
                                write(connfd, json, strlen(json));
                                close(roomclient_aux->connfd);

                                pode_inserir_na_sala = 0;
                            }
                            roomclient_aux = roomclient_aux->pnext;
                        }
                    }
                    room_aux2 = room_aux2->pnext;
                }
                /** Se for possível, insere-o na sala */
                if(pode_inserir_na_sala == 1)
                {
                    insert_room_client(&rooms_list, room_id, protocol->origin, connfd);
                    print_rooms(rooms_list);
                    ROOM *room_aux2 = rooms_list.pfirst;
                    while(room_aux2->pnext != NULL)
                    {
                        room_aux2 = room_aux2->pnext;
                    }
                    print_room_clients(rooms_list, room_aux2->roomid);

                    pthread_kill(room_aux2->thread_id, SIGUSR1);
                }
            }
        }
        printf("Item consumido: %d\n", connfd);
        printf("\n");
    }
/**^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/
}
/**vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv*/
void produz(int connfd)
{
    if(count != BUFFER_SIZE)
    {
        buffer[in] = connfd;
        in = (in + 1) % BUFFER_SIZE;
        count++;
    }
}

int consome()
{
    int connfd;
    if(count != 0)
    {
        connfd = buffer[out];
        out = (out + 1) % BUFFER_SIZE;
        count--;
    }
    return connfd;
}
/**^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/
void* room_thread(void* parameters)
{
    struct room_struct* p = (struct room_struct*) parameters;
    printf("Thread sala criada\n");
    printf("ID da sala: %d\n", p->room->roomid);
    p->room->thread_id = (unsigned long) pthread_self();
    printf("ID da thread da sala: %lu\n", p->room->thread_id);

    sigset_t mask;
    sigset_t orig_mask;
    struct sigaction act;
    memset (&act, 0, sizeof(act));
    act.sa_handler = hdl;
    if (sigaction(SIGUSR1, &act, 0))
    {
        perror ("sigaction");
        return NULL;
    }
    sigemptyset (&mask);
    sigaddset (&mask, SIGUSR1);
    if (sigprocmask(SIG_BLOCK, &mask, &orig_mask) < 0)
    {
        perror ("sigprocmask");
        return NULL;
    }

    fd_set rset;
    int max_sd=0;
    char buf[BUF_SIZE];
    max_sd = p->room->pfirst->connfd;

    ROOMCLIENT* roomclient_aux;
    ROOMCLIENT* roomclient_aux2;
    ROOMCLIENT* roomclient_aux3;
    ROOMCLIENT* roomclient_aux4;
    ROOM* room_aux;
    int connfd_aux = 0;
    char json[200] = "";
    char type[2]= "";
    char room_aux_id[2]= "";

    for (;;)
    {
        roomclient_aux = p->room->pfirst;
        while(roomclient_aux != NULL)
        {
            FD_SET(roomclient_aux->connfd, &p->room->allset);
            printf("Connfd %d no allset\n", roomclient_aux->connfd);
            if(roomclient_aux->connfd > max_sd)
            {
                max_sd = roomclient_aux->connfd;
            }
            roomclient_aux = roomclient_aux->pnext;
        }
        rset = p->room->allset;
        printf("Antes do pselect()\n");
        printf("\n");
        int activity = pselect(max_sd + 1, &rset, NULL, NULL, NULL, &orig_mask);
        printf("Depois do pselect()\n");
        if(activity<0 && errno==EINTR)
        {
            printf("Sinal recebido: %lu\n", (unsigned long) pthread_self());

            /** Envia aviso de novo cliente para todos os clientes */
            /** Procura pelo cliente novo */
            roomclient_aux2 = p->room->pfirst;
            while(roomclient_aux2->pnext != NULL)
            {
                roomclient_aux2 = roomclient_aux2->pnext;
            }
            printf("Novo (ultimo) cliente: %s\n", roomclient_aux2->pname);
            /** Envia o nome desse cliente aos restantes clientes */
            roomclient_aux3 = p->room->pfirst;
            while(roomclient_aux3 != NULL)
            {
                /** type = 1 -> Registration */
                create_json_string(json, "1", roomclient_aux2->pname, "", "");
                write(roomclient_aux3->connfd, json, strlen(json));
                roomclient_aux3 = roomclient_aux3->pnext;
            }
            continue;
        }
        else
        {
            printf("Nova mensagem\n");
            roomclient_aux = p->room->pfirst;
            while(roomclient_aux != NULL)
            {
                if(FD_ISSET(roomclient_aux->connfd, &rset))
                {
                    printf("Mensagem do cliente %d\n",roomclient_aux->connfd);
                    printf("Antes do read()\n");
                    int bytes = read(roomclient_aux->connfd, buf, BUF_SIZE);
                    printf("Depois do read()\n");

                    /** Se bytes = 0 -> desconectou */
                    if(bytes == 0)
                    {
                        close(roomclient_aux->connfd);
                        remove_client(&rooms_list, p->room->roomid, roomclient_aux->connfd);
                        print_room_clients(rooms_list, p->room->roomid);
                        printf("\n");
                        if(p->room->nroomclients == 0)
                        {
                            pthread_exit(NULL);
                        }
                        break;
                    }
                    /** Converte mensagem em estrutura com os campos da mensagem */
                    PROTOCOL* protocol=parse_message(buf);
                    print_protocol(protocol);

                    /** type = 2 -> Broadcast */
                    if(protocol->type == 2)
                    {
                        /** Envia mensagem a todos os clientes */
                        roomclient_aux = p->room->pfirst;
                        while(roomclient_aux != NULL)
                        {
                            write(roomclient_aux->connfd, buf, bytes);
                            roomclient_aux = roomclient_aux->pnext;
                        }
                        roomclient_aux = p->room->pfirst;
                        break;
                    }
                    /** type = 3 -> Private */
                    else if(protocol->type == 3)
                    {
                        roomclient_aux = p->room->pfirst;
                        /** Procura pelo cliente destination */
                        while(roomclient_aux != NULL)
                        {
                            if(strcmp(roomclient_aux->pname, protocol->destination) == 0)
                            {
                                write(roomclient_aux->connfd, buf, bytes);
                                break;
                            }
                            roomclient_aux = roomclient_aux->pnext;
                        }
                        roomclient_aux = p->room->pfirst;
                        break;
                    }
                    /** type = 4 -> Whoisonline */
                    else if(protocol->type == 4)
                    {
                        /** Procura pelo connfd do cliente origin */
                        roomclient_aux = p->room->pfirst;
                        while(roomclient_aux != NULL)
                        {
                            if(strcmp(roomclient_aux->pname, protocol->origin) == 0)
                            {
                                connfd_aux = roomclient_aux->connfd;
                                break;
                            }
                            roomclient_aux = roomclient_aux->pnext;
                        }

                        /** Envia lista de clientes ao cliente origin */
                        roomclient_aux = p->room->pfirst;
                        while(roomclient_aux != NULL)
                        {
                            /** Converte o int protocol->type para a string type */
                            sprintf(type, "%d", protocol->type);
                            create_json_string(json, type, "", "", roomclient_aux->pname);
                            write(connfd_aux, json, strlen(json));
                            roomclient_aux = roomclient_aux->pnext;
                            sleep(1);
                        }
                        roomclient_aux = p->room->pfirst;
                        break;
                    }
                    /** type = 8 -> ShowRooms */
                    else if(protocol->type == 8)
                    {
                        /** Procura pelo connfd do cliente origin */
                        roomclient_aux = p->room->pfirst;
                        while(roomclient_aux != NULL)
                        {
                            if(strcmp(roomclient_aux->pname, protocol->origin) == 0)
                            {
                                connfd_aux = roomclient_aux->connfd;
                                break;
                            }
                            roomclient_aux = roomclient_aux->pnext;
                        }
                        room_aux = rooms_list.pfirst;
                        while(room_aux != NULL)
                        {
                            /** Converte o int protocol->type para a string type */
                            sprintf(type, "%d", protocol->type);
                            /** Converte o int room_aux->roomid para a string room_aux_id */
                            sprintf(room_aux_id, "%d", room_aux->roomid);
                            create_json_string(json, type, "", "", room_aux_id);
                            write(connfd_aux, json, strlen(json));
                            room_aux = room_aux->pnext;
                            sleep(1);
                        }
                        roomclient_aux = p->room->pfirst;
                        break;
                    }
                    /** type = 7 -> Leave */
                    if(protocol->type == 7)
                    {
                        /** Envia mensagem a todos os clientes */
                        roomclient_aux = p->room->pfirst;
                        while(roomclient_aux != NULL)
                        {
                            if(strcmp(roomclient_aux->pname, protocol->origin) == 0)
                            {
                                connfd_aux = roomclient_aux->connfd;
                            }
                            write(roomclient_aux->connfd, buf, bytes);
                            roomclient_aux = roomclient_aux->pnext;
                        }
                        roomclient_aux = p->room->pfirst;
                        /** Remove o cliente da lista de clientes da sala */
                        close(connfd_aux);
                        remove_client(&rooms_list, p->room->roomid, connfd_aux);
                        print_room_clients(rooms_list, p->room->roomid);
                        printf("\n");

                        if(p->room->nroomclients == 0)
                        {
                            pthread_exit(NULL);
                        }
                        break;
                    }
                    /** type = 9 -> Ban */
                    if(protocol->type == 9)
                    {
                        /** Verifica se é o moderador */
                        if(roomclient_aux->connfd == p->room->pfirst->connfd)
                        {
                            printf("moderador ban %s\n", protocol->message);
                            /** Procura pelo cliente a banir */
                            roomclient_aux4 = p->room->pfirst;
                            while(roomclient_aux4 != NULL)
                            {
                                if(strcmp(roomclient_aux4->pname, protocol->message) == 0)
                                {
                                    /** Insere o cliente na lista de clientes banidos */
                                    insert_room_client_ban(&rooms_list, p->room->roomid, roomclient_aux4->pname, roomclient_aux4->connfd);
                                    print_room_clients_banned(rooms_list, p->room->roomid);
                                    printf("\n");
                                    /** Envia aviso de ban */
                                    create_json_string(json, "9", "", "", "Banido da sala.");
                                    write(roomclient_aux4->connfd, json, strlen(json));
                                    /** Remove o cliente da sala */
                                    close(roomclient_aux4->connfd);
                                    remove_client(&rooms_list, p->room->roomid, roomclient_aux4->connfd);
                                    print_room_clients(rooms_list, p->room->roomid);
                                    printf("\n");
                                    break;
                                }
                                roomclient_aux4 = roomclient_aux4->pnext;
                            }
                        }
                        else
                        {
                            printf("Nao e o moderador\n");
                        }
                        roomclient_aux = p->room->pfirst;
                        break;
                    }
                }
                roomclient_aux = roomclient_aux->pnext;
            }
        }
    }
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

void create_room(ROOMSLIST *prl, char *name, int connfd)
{
    ROOMCLIENT *tempgc = malloc(sizeof(ROOMCLIENT));
    tempgc->pname = malloc(sizeof(char)*(strlen(name) + 1));
    strcpy(tempgc->pname, name);
    tempgc->connfd = connfd;
    tempgc->pnext = NULL;

    ROOM *tempg = malloc(sizeof(ROOM));
    tempg->pfirst = tempgc;
    tempg->nroomclients = 1;
    tempg->pnext = NULL;
    tempg->pfirst_banned = NULL;

    if(prl->nrooms == 0)
    {
        tempg->roomid = 0;
        prl->pfirst = tempg;
        prl->nrooms++;
        return;
    }
    ROOM *current = prl->pfirst;
    while(current->pnext != NULL)
    {
        current = current->pnext;
    }
    tempg->roomid = (current->roomid) + 1;
    current->pnext = tempg;
    prl->nrooms++;
}

void print_rooms(ROOMSLIST rl)
{
    printf("Numero de salas: %d\n", rl.nrooms);
    ROOM *tempg = rl.pfirst;

    int i;
    for(i = 0; i < rl.nrooms; i++)
    {
        printf("ROOM[%d]: id = %d\tNumero de clientes = %d\n", i, tempg->roomid, tempg->nroomclients);
        tempg = tempg->pnext;
    }
}

void print_room_clients_banned(ROOMSLIST rl, int room_id)
{
    ROOM *tempg = rl.pfirst;
    int i;
    for(i = 0; i < rl.nrooms; i++)
    {
        if(tempg->roomid == room_id)
        {
            printf("ROOM ID = %d\n", tempg->roomid);
            ROOMCLIENT *tempgc = tempg->pfirst_banned;
            int j = 0;
            while(tempgc != NULL)
            {
                printf("BANNED_CLIENT[%d]: nome = %s\tConnfd = %d\n", j, tempgc->pname, tempgc->connfd);
                j++;
                tempgc = tempgc->pnext;
            }
            return;
        }
        tempg = tempg->pnext;
    }
}

void print_room_clients(ROOMSLIST rl, int room_id)
{
    ROOM *tempg = rl.pfirst;
    int i;
    for(i = 0; i < rl.nrooms; i++)
    {
        if(tempg->roomid == room_id)
        {
            printf("ROOM ID = %d\n", tempg->roomid);
            ROOMCLIENT *tempgc = tempg->pfirst;
            int j;
            for(j = 0; j < tempg->nroomclients; j++)
            {
                printf("CLIENT[%d]: nome = %s\tConnfd = %d\n", j, tempgc->pname, tempgc->connfd);
                tempgc = tempgc->pnext;
            }
            return;
        }
        tempg = tempg->pnext;
    }
}

void insert_room_client_ban(ROOMSLIST *prl, int room_id, char *name, int connfd)
{
    ROOMCLIENT *tempgc = malloc(sizeof(ROOMCLIENT));
    tempgc->pname = malloc(sizeof(char)*(strlen(name)+1));
    strcpy(tempgc->pname, name);
    tempgc->connfd = connfd;
    tempgc->pnext = NULL;

    ROOM *tempg = prl->pfirst;
    while(tempg != NULL)
    {
        if(tempg->roomid == room_id)
        {
            if(tempg->pfirst_banned == NULL)
            {
                tempg->pfirst_banned = tempgc;
                return;
            }
            ROOMCLIENT *current = tempg->pfirst_banned;
            while(current->pnext != NULL)
            {
                current = current->pnext;
            }
            current->pnext = tempgc;
            return;
        }
        tempg = tempg->pnext;
    }
}

void insert_room_client(ROOMSLIST *prl, int room_id, char *name, int connfd)
{
    ROOMCLIENT *tempgc = malloc(sizeof(ROOMCLIENT));
    tempgc->pname = malloc(sizeof(char)*(strlen(name) + 1));
    strcpy(tempgc->pname, name);
    tempgc->connfd = connfd;
    tempgc->pnext = NULL;

    ROOM *tempg = prl->pfirst;
    while(tempg != NULL)
    {
        if(tempg->roomid == room_id)
        {
            ROOMCLIENT *current = tempg->pfirst;
            while(current->pnext != NULL)
            {
                current = current->pnext;
            }
            current->pnext = tempgc;
            tempg->nroomclients++;
            return;
        }
        tempg = tempg->pnext;
    }
}

void remove_client(ROOMSLIST *prl, int room_id, int connfd)
{
    ROOM *room_aux = prl->pfirst;
    //procura pela sala (room_aux)
    while(room_aux->roomid != room_id)
    {
        room_aux = room_aux->pnext;
    }
    //se for o primeiro cliente da sala, poe pfirst a apontar para o segundo cliente e elimina o primeiro
    if(room_aux->pfirst->connfd == connfd)
    {
        ROOMCLIENT *pClientDel = room_aux->pfirst;
        room_aux->pfirst = room_aux->pfirst->pnext;
        room_aux->nroomclients--;
        free(pClientDel);
        //se a sala ficar sem clientes, elimina a sala
        if(room_aux->nroomclients == 0)
        {
            //se for a primeira sala da lista, poe pfirst a apontar para a segunda sala e elimina a primeira
            if(prl->pfirst->roomid == room_aux->roomid)
            {
                ROOM *pRoomDel = prl->pfirst;
                prl->pfirst = prl->pfirst->pnext;
                prl->nrooms--;
                free(pRoomDel);
                return;
            }
            //se nao for a primeira sala, procura pela sala (next_room), poe a sala anterior a apontar para a seguinte e elimina a sala
            ROOM *current_room = prl->pfirst, *next_room = prl->pfirst->pnext;
            while(next_room != NULL)
            {
                if(next_room->roomid == room_id)
                {
                    current_room->pnext = next_room->pnext;
                    prl->nrooms--;
                    free(next_room);
                    return;
                }
                current_room = next_room;
                next_room = next_room->pnext;
            }
        }
        return;
    }
    //se nao for o primeiro cliente, procura pelo cliente (next_client), poe o cliente anterior a apontar para o seguinte e elimina o cliente
    ROOMCLIENT *current_client = room_aux->pfirst, *next_client = room_aux->pfirst->pnext;
    while(next_client != NULL)
    {
        if(next_client->connfd == connfd)
        {
            current_client->pnext = next_client->pnext;
            room_aux->nroomclients--;
            free(next_client);
            return;
        }
        current_client = next_client;
        next_client = next_client->pnext;
    }
}
