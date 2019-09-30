gcc -pthread -o servidor.out servidor.c -include my_protocol.h my_protocol.c jsmn.h jsmn.c
./servidor.out -lpthread
