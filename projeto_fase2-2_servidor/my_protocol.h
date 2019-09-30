#ifndef MY_PROTOCOL_H_INCLUDED
#define MY_PROTOCOL_H_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "jsmn.h"
#define MAX_TOKENS 256

typedef struct protocol{
    int type;
    char* origin;
    char* destination;
    char* message;
}PROTOCOL;

char* get_token(jsmntok_t* t,char* message);
PROTOCOL* parse_message(char* message);
void print_protocol(PROTOCOL* protocol);

#endif // MY_PROTOCOL_H_INCLUDED
