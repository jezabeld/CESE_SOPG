/**
 * @file server_tcp.c
 * @brief Servidor TCP para base de datos clave-valor
 * @author Jez
 * @date 27 July 2025
 */

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h> // Para mkdir()
#include <unistd.h>

#define SERVER_PORT 5000
#define MAX_MSG_LENGTH 128
#define MAX_VAL_READ_LEN 100
#define MAX_WORDS 3
#define MAX_PATH_LEN 128
#define DB_FOLDER_PERM 0755
#define FILES_PERM 0644

/****************** prototipos funciones auxiliares ******************/
/**
 * @brief Configura y pone en escucha el socket del servidor
 * @param port Puerto en el que escuchar
 * @return Descriptor del socket del servidor
 */
int serverSocketSet(int port);

/**
 * @brief Acepta una conexión entrante
 * @param serverSoc Socket del servidor
 * @return Descriptor del socket del cliente
 */
int serverSocketAccept(int serverSoc);

/**
 * @brief Lee un mensaje del cliente
 * @param s Socket del cliente
 * @param buffer Buffer donde almacenar el mensaje
 * @return Número de bytes leídos
 */
int serverReadMessage(int s, char* buffer);

/**
 * @brief Envía un mensaje al cliente
 * @param s Socket del cliente
 * @param buffer Buffer con el mensaje a enviar
 * @return Número de bytes enviados
 */
int serverSendMessage(int s, char* buffer);

/**
 * @brief Envía mensaje de uso al cliente
 * @param s Socket del cliente
 */
void serverSendUsageMsg(int s);

/**
 * @brief Crea una nueva clave en la base de datos
 * @param pathKey Ruta del archivo de la clave
 * @param value Valor a almacenar
 */
void dbCreateKey(const char* pathKey, const char* value);

/**
 * @brief Obtiene el valor de una clave
 * @param pathKey Ruta del archivo de la clave
 * @param value Buffer donde almacenar el valor
 */
void dbGetValue(const char* pathKey, char* value);

/**
 * @brief Elimina una clave de la base de datos
 * @param pathKey Ruta del archivo de la clave
 */
void dbDeleteValue(const char* pathKey);

/**
 * @brief Tokeniza una cadena separada por espacios
 * @param string Cadena a tokenizar
 * @param maxTokens Número máximo de tokens
 * @param array Array donde almacenar los tokens
 * @return Número de tokens encontrados
 */
static int utilsStringTokenize(char* string, int maxTokens, char* array[]);

/**
 * @brief Asegura que un directorio existe, lo crea si no existe
 * @param path Ruta del directorio
 */
static void utilsEnsureDirectoryExists(const char* path);

/**
 * @brief Verifica si un archivo existe
 * @param path Ruta del archivo
 * @return 1 si existe, 0 si no existe
 */
static int utilsFileExists(const char* path);

/**
 * @brief Genera la ruta completa de un archivo
 * @param folder Carpeta base
 * @param filename Nombre del archivo
 * @param fullpath Buffer donde almacenar la ruta completa
 */
static void utilsGenerateFilePath(const char* folder, const char* filename, char* fullpath);

/**
 * @brief Limpia recursos y termina el programa
 * @param code Código de salida
 */
static void utilsCleanupAndExit(int code);

/**
 * @brief Manejador de señales
 * @param sig Número de señal recibida
 */
static void utilsSignalHandler(int sig);

/**
 * @brief Agrega múltiples señales al manejador
 * @param sa Estructura sigaction
 * @param array Array de señales
 * @param len Longitud del array
 */
static void utilsAddSignalsToHandler(struct sigaction* sa, int array[], int len);

/************************ variables globales *************************/
/** @brief Ruta de la carpeta de base de datos */
const char* PATH_DB_FOLDER = "./db";

/** @brief Socket del servidor */
int serverSoc;

/** @brief Socket del cliente actual */
int clientSoc;

/*************************** main function ***************************/
/**
 * @brief Función principal del servidor TCP
 *
 * Configura el manejo de señales, crea el socket del servidor y entra en un
 * bucle infinito para atender clientes. Procesa comandos SET, GET y DEL.
 *
 * Si recibe Ctrl+C termina controladamente cerrando los sockets.
 *
 * @return EXIT_SUCCESS en caso de terminación controlada.
 */
int main(void) {

    struct sigaction sa = { 0 };
    sa.sa_handler = utilsSignalHandler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);

    int signals[] = { SIGINT, SIGPIPE };
    utilsAddSignalsToHandler(&sa, signals, sizeof(signals) / sizeof(signals[0]));

    // Seteamos el socket del server
    serverSoc = serverSocketSet(SERVER_PORT);

    while (1) {

        // Espera conexión entrante
        clientSoc = serverSocketAccept(serverSoc);

        // Leemos mensaje de cliente
        char msg[MAX_MSG_LENGTH] = { 0 };
        int bytesRead = serverReadMessage(clientSoc, msg);
        char* words[MAX_WORDS] = { 0 };
        if (bytesRead > 5) { // 3 del comando + 1 espacio + al menos 1 clave, si no se cumple ni
                             // miro que llego

            int params = utilsStringTokenize(msg, MAX_WORDS, words);
            printf("server: parámetros recibidos %d\n", params);

            if (params > 1) { // ademas del comando hay algo mas

                // aseguro q exista la carpeta
                utilsEnsureDirectoryExists(PATH_DB_FOLDER);
                // creo la ruta del archivo
                char fullpath[MAX_PATH_LEN];
                utilsGenerateFilePath(PATH_DB_FOLDER, words[1], fullpath);

                if (strcmp(words[0], "SET") == 0) {
                    printf("server: comando SET detectado\n");

                    if (params == 3) { // tengo clave y valor
                        printf("server: SET %s %s\n", words[1], words[2]);

                        // chequeo si ya existe la clave
                        if (utilsFileExists(fullpath)) {
                            printf("server: archivo a setear ya existe\n");
                            serverSendMessage(clientSoc, "ALREADYSET\n");
                        } else {
                            // crear el registro
                            dbCreateKey(fullpath, words[2]);
                            printf("server: archivo creado: %s, valor: %s\n", fullpath, words[2]);
                            serverSendMessage(clientSoc, "OK\n");
                        }
                    } else {
                        printf("server: SET comando muy corto.\n");
                        serverSendMessage(clientSoc, "ERROR: el comando SET requiere clave y valor.\n");
                    }
                } else if (strcmp(words[0], "GET") == 0) {
                    printf("server: comando GET detectado\n");
                    if (params == 2) { // solo el cmd y una clave
                        printf("server: GET %s\n", words[1]);
                        // chequeo si existe la clave
                        if (utilsFileExists(fullpath)) {
                            // obtengo el valor
                            char value[MAX_VAL_READ_LEN];
                            dbGetValue(fullpath, value);
                            printf("server: valor a devolver %s\n", value);
                            // y lo devuelvo
                            char resp[MAX_MSG_LENGTH];
                            sprintf(resp, "OK\n%s\n", value);
                            serverSendMessage(clientSoc, resp);
                        } else {
                            printf("server: archivo solicitado no existe: %s\n", fullpath);
                            serverSendMessage(clientSoc, "NOTFOUND\n");
                        }
                    } else {
                        printf("server: GET comando muy largo.\n");
                        serverSendMessage(clientSoc, "ERROR: el comando GET solo requiere clave.\n");
                    }
                } else if (strcmp(words[0], "DEL") == 0) {
                    printf("server: comando DEL detectado\n");
                    if (params == 2) { // solo el cmd y una clave
                        printf("server: DEL %s\n", words[1]);
                        // chequeo si existe la clave
                        if (utilsFileExists(fullpath)) {
                            // eliminar el registro
                            printf("server: archivo a eliminar %s\n", fullpath);
                            dbDeleteValue(fullpath);
                            serverSendMessage(clientSoc, "OK\n");
                        } else {
                            printf("server: archivo solicitado no existe: %s\n", fullpath);
                            serverSendMessage(clientSoc, "NOTFOUND\n");
                        }
                    } else {
                        printf("server: DEL comando muy largo.\n");
                        serverSendMessage(clientSoc, "ERROR: el comando DEL solo requiere clave.\n");
                    }
                } else {
                    printf("server: ningún comando detectado\n");
                    serverSendMessage(clientSoc, "ERROR: ningún comando válido detectado.\n");
                    serverSendUsageMsg(clientSoc);
                }
            } else {
                printf("server: comando muy corto.\n");
                serverSendMessage(clientSoc, "ERROR: comando muy corto.\n");
                serverSendUsageMsg(clientSoc);
            }
        } else if (bytesRead > 0) {
            printf("server: comando muy corto.\n");
            serverSendMessage(clientSoc, "ERROR: comando muy corto.\n");
            serverSendUsageMsg(clientSoc);
        }

        // Active Close
        close(clientSoc);
    }

    // para consistencia
    close(serverSoc);
    return EXIT_SUCCESS;
}

/*********************** funciones del servidor ************************/
int serverSocketSet(int port) {
    // se crea el socket
    int s = socket(AF_INET, SOCK_STREAM, 0);

    // Cargamos datos de IP:PORT del server
    struct sockaddr_in serveraddr = { 0 };
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(port);
    if (inet_pton(AF_INET, "127.0.0.1", &(serveraddr.sin_addr)) <= 0) {
        fprintf(stderr, "ERROR invalid server IP\n");
        utilsCleanupAndExit(EXIT_FAILURE);
    }

    // Abrimos puerto
    if (bind(s, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) == -1) {
        perror("Error in bind");
        utilsCleanupAndExit(EXIT_FAILURE);
    }

    // Seteamos socket en modo Listening
    if (listen(s, 1) == -1) {
        perror("Error in listen");
        utilsCleanupAndExit(EXIT_FAILURE);
    }

    return s;
}

int serverSocketAccept(int serverSoc) {
    // Ejecutamos accept() para recibir conexiones entrantes
    socklen_t addr_len = sizeof(struct sockaddr_in);
    struct sockaddr_in clientaddr;
    int clientSoc;
    printf("server: esperando una conexión...\n");
    if ((clientSoc = accept(serverSoc, (struct sockaddr*)&clientaddr, &addr_len)) == -1) {
        perror("Error in accept");
        utilsCleanupAndExit(EXIT_FAILURE);
    }

    char ipClient[32];
    inet_ntop(AF_INET, &(clientaddr.sin_addr), ipClient, sizeof(ipClient));
    printf("server: conexión desde:  %s\n", ipClient);

    return clientSoc;
}

int serverReadMessage(int s, char* buffer) {
    int n;
    if ((n = read(s, buffer, MAX_MSG_LENGTH)) == -1) {
        perror("Error in read");
        utilsCleanupAndExit(EXIT_FAILURE);
    }
    if (n > 0) buffer[n - 1] = 0x00; // sobrescribo el salto de linea
    printf("server: recibidos %d bytes:%s\n", n, n ? buffer : "");
    return n;
}

int serverSendMessage(int s, char* buffer) {
    int n;
    int len = strlen(buffer);
    if (len > MAX_MSG_LENGTH) {
        fprintf(stderr, "ERROR in serverSendMessage: message too long.\n");
        utilsCleanupAndExit(EXIT_FAILURE);
    }
    if ((n = write(s, buffer, len)) == -1) {
        perror("Error in write");
        utilsCleanupAndExit(EXIT_FAILURE);
    }
    printf("server: enviados %d bytes\n", n);
    return n;
}

void serverSendUsageMsg(int s) {
    serverSendMessage(s, "Usage:\n<CMD> <key> [<value>]\nComandos:\n\tSET\tSetea un registro clave-valor nuevo.\n");
    serverSendMessage(s, "\tGET\tObtiene el valor de una clave.\n\tDEL\tElimina un registro a partir de su clave.\n");
}

/*********************** funciones de base de datos ************************/
void dbCreateKey(const char* pathKey, const char* value) {
    // creo el archivo y lo abro
    int fd = open(pathKey, O_WRONLY | O_CREAT, FILES_PERM);
    if (fd == -1) {
        perror("Error in open");
        utilsCleanupAndExit(EXIT_FAILURE);
    }
    // escribo el archivo
    ssize_t n;
    if ((n = write(fd, value, strlen(value))) == -1) {
        perror("Error in write");
        close(fd);
        utilsCleanupAndExit(EXIT_FAILURE);
    }
    if (n != strlen(value)) {
        fprintf(stderr, "ERROR writing file, value: %s, bytes: %ld, bytes written: %ld.\n", value, strlen(value), n);
        utilsCleanupAndExit(EXIT_FAILURE);
    }

    if (close(fd) == -1) {
        perror("Error in close");
        utilsCleanupAndExit(EXIT_FAILURE);
    }
}

void dbGetValue(const char* pathKey, char* value) {
    // abro el archivo
    int fd = open(pathKey, O_RDONLY);
    if (fd == -1) {
        perror("Error in open");
        utilsCleanupAndExit(EXIT_FAILURE);
    }

    ssize_t n;
    if ((n = read(fd, value, MAX_VAL_READ_LEN)) == -1) {
        perror("Error in read");
        close(fd);
        utilsCleanupAndExit(EXIT_FAILURE);
    }
    value[n] = '\0'; // me aseguro que tenga caracter null al final

    if (close(fd) == -1) {
        perror("Error in close");
        utilsCleanupAndExit(EXIT_FAILURE);
    }
}

void dbDeleteValue(const char* pathKey) {
    /*
     * unlink() deletes a name from the filesystem.  If that name was the
     *  last link to a file and no processes have the file open, the file
     *  is deleted and the space it was using is made available for reuse.
     */
    if (unlink(pathKey) != 0) {
        perror("Error in unlink");
        utilsCleanupAndExit(EXIT_FAILURE);
    }
}

/*********************** funciones utilitarias ************************/
static int utilsStringTokenize(char* string, int maxTokens, char* array[]) {
    /*
     * strtok() function breaks a string into a sequence of zero or more
     *  nonempty tokens.  On the first call to strtok(), the string
     *  to be parsed should be specified in str. In each subsequent call
     *  that should parse the same string, str must be NULL.
     *  It returns a pointer to the next token, or NULL if there are no more tokens.
     */
    int i = 0;
    char* p = strtok(string, " ");

    while (p != NULL && i < maxTokens) {
        array[i++] = p;
        p = strtok(NULL, " ");
    }
    if (p != NULL && i >= maxTokens) {
        fprintf(stderr, "ERROR invalid command.\nUsage:\n<CMD> <key> [<value>]\n");
        utilsCleanupAndExit(EXIT_FAILURE);
    }

    return i;
}

static void utilsGenerateFilePath(const char* folder, const char* filename, char* fullpath) {
    /*
     * Esto también se podría hacer con snprintf:
     *   snprintf(fullpath, MAX_PATH_LEN, "%s/%s", folder, filename);
     *  pero me pareció más acorde al programa hacerlo con funciones bajo nivel
     *  (aunque es mucho más propenso a errores)
     */
    if ((strlen(folder) + strlen(filename) + 2) < MAX_PATH_LEN) { // +2 para la barra y el \0
        strncpy(fullpath, folder, MAX_PATH_LEN - 1);
        strcat(fullpath, "/");
        // sumo 1 por la barra, resto 1 para no llegar al final (explicito solo para que sea mas claro)
        strncat(fullpath, filename, MAX_PATH_LEN - strlen(folder) + 1 - 1);
        fullpath[strlen(folder) + strlen(filename) + 1] = '\0'; // Asegura el null character
        printf("utils: fullpath del archivo: %s\n", fullpath);
    } else {
        fprintf(stderr, "ERROR path+archivo muy largo\n");
        utilsCleanupAndExit(EXIT_FAILURE);
    }
}

static int utilsFileExists(const char* path) {
    /*
     * access() checks whether the calling process can access the file
     *  F_OK tests for the existence of the file.
     *  On success (all requested permissions granted, or mode is F_OK and the
     *  file exists), zero is returned. Otherwise, -1 is returned, and errno is set
     *  to indicate the error.
     */
    return (access(path, F_OK) != -1);
}

static void utilsEnsureDirectoryExists(const char* path) {
    if (access(path, F_OK) == 0) {
        // Ya existe
        printf("utils: Carpeta detectada correctamente.\n");
    } else {
        // No existe, intento crearlo
        if (mkdir(path, DB_FOLDER_PERM) != 0) {
            perror("mkdir"); // Fallo al crear
            utilsCleanupAndExit(EXIT_FAILURE);
        }
        printf("utils: Carpeta creada correctamente.\n");
    }
}

static void utilsCleanupAndExit(int code) {
    if (clientSoc) close(clientSoc);
    if (serverSoc) close(serverSoc);
    exit(code);
}

static void utilsAddSignalsToHandler(struct sigaction* sa, int array[], int len) {
    for (int i = 0; i < len; i++) {
        if (sigaction(array[i], sa, NULL) == -1) {
            perror("Error in sigaction");
            utilsCleanupAndExit(EXIT_FAILURE);
        }
    }
}

static void utilsSignalHandler(int sig) {
    printf("handler: señal recibida %d.\n", sig);
    switch (sig) {
    case SIGPIPE:
        printf("handler: cliente corto la conexión. Cerrando socket del cliente.\n");
        if (clientSoc) close(clientSoc);
        break;
    case SIGINT:
        printf("handler: desconectando server.\n");
        utilsCleanupAndExit(EXIT_SUCCESS);
        break;
    default:
        break;
    }
}

/*********************** end of file ************************/