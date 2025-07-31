#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/stat.h> // Para mkdir()

#define SERVER_PORT 5000
#define MAX_MSG_LENGTH 128
#define MAX_WORDS 3
#define MAX_PATH_LEN 128
#define DB_FOLDER_PERM  0755
#define FILES_PERM 0644

const char * PATH_DB_FOLDER = "./db";

/****************** prototipos funciones auxiliares ******************/
int serverSocketSet(int port);
int serverSocketAccept(int serverSoc);
int serverReadMessage(int s, char * buffer);
int serverSendMessage(int s, char * buffer);
void serverSendUsageMsg(int s);
void dbCreateKey(const char * pathKey, const char * value);
static void utilityStringTokenize(char * string, int maxTokens, char * array[]);
static void utilsEnsureDirectoryExists(const char * path);
static int utilsFIleExists(const char * path);
static void utilsGenerateFilePath(const char *folder, const char * filename, char * fullpath);
 
/*************************** main function ***************************/
int main(void){

    // Seteamos el socket del server
    int serverSoc = serverSocketSet(SERVER_PORT);

    while (1){

        // Espera conexión entrante
        int clientSoc = serverSocketAccept(serverSoc);

        // Leemos mensaje de cliente
        char msg[MAX_MSG_LENGTH];
        int bytesRead = serverReadMessage(clientSoc, msg);

        if(bytesRead > 5){ // 3 del comando + 1 espacio + al menos 1 clave 
            char *words[MAX_WORDS];
            utilityStringTokenize(msg, MAX_WORDS, words);
            if(words[1]){
                if (strcmp(words[0], "SET") == 0) {
                    printf("Comando SET detectado\n");
                    if(words[2]){
                        printf("SET %s %s\n", words[1], words[2]);
                        // crear el registro
                        utilsEnsureDirectoryExists(PATH_DB_FOLDER); // aseguro q este la carpeta
                        char fullpath[MAX_PATH_LEN];
                        utilsGenerateFilePath(PATH_DB_FOLDER, words[1], fullpath); // creo la ruta del archivo
                        if (utilsFIleExists(fullpath)){ 
                            printf("ERROR: archivo a setear ya existe");
                            // char resp[MAX_MSG_LENGTH]; 
                            // sprintf(resp, "ERROR: la clave %s ya existe.\n", words[1]);
                            serverSendMessage(clientSoc, "ALREADYSET\n");
                        } else {
                            dbCreateKey(fullpath, words[2]);
                            printf("Archivo creado: %s, valor: %s\n", fullpath, words[2]);
                            serverSendMessage(clientSoc, "OK\n");
                        }
                    } else {
                        printf("SET Comando muy corto.\n");
                        serverSendMessage(clientSoc, "ERROR: el comando SET requiere clave y valor.\n");

                    }
                } else if (strcmp(words[0], "GET") == 0) {
                    printf("Comando GET detectado\n");
                    if(!words[2]){
                        printf("GET %s\n", words[1]);
                        // devolver el valor

                    } else {
                        printf("GET Comando muy largo.\n");
                        serverSendMessage(clientSoc, "ERROR: el comando GET solo requiere clave.\n");
                    }

                } else if (strcmp(words[0], "DEL") == 0) {
                    printf("Comando DEL detectado\n");
                    if(!words[2]){
                        printf("DEL %s\n", words[1]);
                        // eliminar el registro

                    } else {
                        printf("DEL Comando muy largo.\n");
                        serverSendMessage(clientSoc, "ERROR: el comando DEL solo requiere clave.\n");
                    }
                } else {
                    printf("Ningun comando detectado\n");
                    serverSendMessage(clientSoc, "ERROR: ningún comando válido detectado.\n");
                    serverSendUsageMsg(clientSoc);
                }
            } else {
                printf("Comando muy corto.\n");
                serverSendMessage(clientSoc, "ERROR: comando muy corto.\n");
                serverSendUsageMsg(clientSoc);
            }
        } else {
            printf("Comando muy corto.\n");
            serverSendMessage(clientSoc, "ERROR: comando muy corto.\n");
            serverSendUsageMsg(clientSoc);
        }

        // Active Close
        close(clientSoc);
    }

    return 0;
}

/*********************** funciones auxiliares ************************/
int serverSocketSet(int port){
    // se crea el socket
    int s = socket(AF_INET, SOCK_STREAM, 0);

    // Cargamos datos de IP:PORT del server
    struct sockaddr_in serveraddr = {0};
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(port);
    if (inet_pton(AF_INET, "127.0.0.1", &(serveraddr.sin_addr)) <= 0) {
        fprintf(stderr, "ERROR invalid server IP\n");
        exit(EXIT_FAILURE);
    }

    // Abrimos puerto 
    if (bind(s, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) == -1) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    // Seteamos socket en modo Listening
    if (listen(s, 1) == -1) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    return s;
}

int serverSocketAccept(int serverSoc){
    // Ejecutamos accept() para recibir conexiones entrantes
    socklen_t addr_len = sizeof(struct sockaddr_in);
    struct sockaddr_in clientaddr;
    int clientSoc;
    printf("server: esperando una conexion...\n");
    if ((clientSoc = accept(serverSoc, (struct sockaddr *)&clientaddr, &addr_len)) == -1) {
        perror("accept");
        exit(EXIT_FAILURE);
    }

    char ipClient[32];
    inet_ntop(AF_INET, &(clientaddr.sin_addr), ipClient, sizeof(ipClient));
    printf("server: conexion desde:  %s\n", ipClient);

    return clientSoc;
}

int serverReadMessage(int s, char * buffer){
    int n;
    if ((n = read(s, buffer, MAX_MSG_LENGTH)) == -1) {
        perror("read");
        exit(EXIT_FAILURE);
    }
    buffer[n-1] = 0x00; // sobreescribo el enter
    printf("server: recibi %d bytes:%s\n", n, buffer);
    return n;
}

int serverSendMessage(int s, char * buffer){
    int n;
    int len = strlen(buffer);
    if(len > MAX_MSG_LENGTH){
        fprintf(stderr, "ERROR invalid command message to send too long.\n");
        exit(EXIT_FAILURE);
    }
    if ((n = write(s, buffer, len)) == -1) {
        perror("write");
        exit(EXIT_FAILURE);
    }
    printf("server: envie %d bytes\n", n);
    return n;
}
void serverSendUsageMsg(int s){
    serverSendMessage(s, "Usage:\n<CMD> <key> [<value>]\nComandos:\n\tSET\tSetea un registro clave-valor nuevo.\n");
    serverSendMessage(s, "\tGET\tObtiene el valor de una clave.\n\tDEL\tElimina un registro a partir de su clave.\n");
}

/* DB */
void dbCreateKey(const char * pathKey, const char * value){
    // creo el archivo y lo abro
    int fd = open(pathKey, O_WRONLY | O_CREAT , FILES_PERM);
    if (fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }
    // escribo el archivo
    ssize_t n; 
    if ((n = write(fd, value, strlen(value))) == -1) {
        perror("write");
        close(fd);
        exit(EXIT_FAILURE);
    }
    if(n != strlen(value)){
        fprintf(stderr, "ERROR writing file, value: %s, length: %ld, written: %ld.\n", value, strlen(value), n);
        exit(EXIT_FAILURE);
    }

}




/* Utils */

static void utilityStringTokenize(char * string, int maxTokens, char * array[]){
    int i = 0;
    char *p = strtok (string, " ");

    while (p != NULL && i < maxTokens)
    {
        array[i++] = p;
        p = strtok (NULL, " ");
    }
    if(p != NULL && i >= maxTokens){
        fprintf(stderr, "ERROR invalid command.\nUsage:\n<CMD> <key> [<value>]\n");
        exit(EXIT_FAILURE);
    }
}

static void utilsGenerateFilePath(const char *folder, const char * filename, char * fullpath){
    if((strlen(folder) + strlen(filename) + 2)<MAX_PATH_LEN){  // +2 para la barra y el \0
        strncpy(fullpath, folder, MAX_PATH_LEN - 1);
        strcat(fullpath, "/");  
        strncat(fullpath, filename, MAX_PATH_LEN- strlen(folder) + 1 - 1); // sumo 1 por la barra, resto 1 para no llegar al final
        fullpath[strlen(folder) + strlen(filename) + 1] = '\0'; // Asegura el null character
        printf("Fullpath del archivo: %s\n", fullpath);
    } else {
        fprintf(stderr, "ERROR path+archivo muy largo\n");
        exit(EXIT_FAILURE);
    }
}

static int utilsFIleExists(const char * path){
    /* access() checks whether the calling process can access the file
    F_OK tests for the existence of the file.
    On success (all requested permissions granted, or mode is F_OK and the file exists), zero is returned.
    Otherwise, -1 is returned, and errno is set to indicate the error.
    */
    return (access(path, F_OK) != -1);
}

static void utilsEnsureDirectoryExists(const char * path){
    if (access(path, F_OK) == 0) {
        // Ya existe
        printf("Carpeta detectada correctamente.\n");
    } else {
        // No existe, intento crearlo
        if (mkdir(path, DB_FOLDER_PERM) != 0) {
            perror("mkdir");// Fallo al crear
            exit(EXIT_FAILURE);
        }
        printf("Carpeta creada correctamente.\n");
    }
}

