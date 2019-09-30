#include "my_protocol.h"

char* get_token(jsmntok_t* t,char* message)
{
    int token_size=t->end-t->start;
    char* token=(char*)malloc(sizeof(char)*(token_size+1)); //allocate memory to the token
    strncpy(token,message+t->start,token_size); //copy token
    token[token_size]='\0'; //close the token
    return token;
}

PROTOCOL* parse_message(char* message){
    PROTOCOL* protocol=(PROTOCOL*)malloc(sizeof(PROTOCOL));
    jsmntok_t tokens[MAX_TOKENS];
    jsmn_parser parser;
    jsmn_init(&parser);
    jsmn_parse(&parser, message, strlen(message), tokens, MAX_TOKENS);
	int i = 0;
    for(i=0;i<256;i++)
    {
        jsmntok_t* t=&tokens[i];
        if(t->end<=0) break;
        char* token=get_token(t,message);

        if(strcmp("type",token)==0)
        {
            free(token);
            i++;
            jsmntok_t* t=&tokens[i];
            char* token=get_token(t,message);
            protocol->type=atoi(token);
        }
        else if(strcmp("origin",token)==0)
        {
            free(token);
            i++;
            jsmntok_t* t=&tokens[i];
            char* token=get_token(t,message);
            protocol->origin=(char*)malloc(sizeof(char)*(strlen(token)+1));
            strcpy(protocol->origin,token);
        }
        else if(strcmp("destination",token)==0)
        {
            free(token);
            i++;
            jsmntok_t* t=&tokens[i];
            char* token=get_token(t,message);
            protocol->destination=(char*)malloc(sizeof(char)*(strlen(token)+1));
            strcpy(protocol->destination,token);
        }
        else if(strcmp("message",token)==0)
        {
            free(token);
            i++;
            jsmntok_t* t=&tokens[i];
            char* token=get_token(t,message);
            protocol->message=(char*)malloc(sizeof(char)*(strlen(token)+1));
            strcpy(protocol->message,token);
        }
        free(token);
    }

    return protocol;
}

void print_protocol(PROTOCOL* protocol)
{
    printf("type: %d origin: %s destination: %s message: %s\n",protocol->type,protocol->origin,protocol->destination,protocol->message);
}
