// test_client.c
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SERVER_PORT 5000
#define SERVER_IP "127.0.0.1"
#define MAX_MSG_LENGTH 128

void sendCommand(int sock, const char* cmd, char buffer[]);
int connectToServer();

int sock;
char buffer[MAX_MSG_LENGTH];

int main(void) {

    // Test: SET
    sock = connectToServer();
    sendCommand(sock, "SET manzana apple\n", buffer);
    printf("Respuesta esperada:\n%s\n", "OK");
    printf("Respuesta del servidor:\n%s\n", buffer);
    sock = connectToServer();
    sendCommand(sock, "SET perro dog\n", buffer);
    printf("Respuesta esperada:\n%s\n", "OK");
    printf("Respuesta del servidor:\n%s\n", buffer);
    sock = connectToServer();
    sendCommand(sock, "SET hola hello\n", buffer);
    printf("Respuesta esperada:\n%s\n", "OK");
    printf("Respuesta del servidor:\n%s\n", buffer);
    // Test: GET
    sock = connectToServer();
    sendCommand(sock, "GET perro\n", buffer);
    printf("Respuesta esperada:\n%s\n", "OK\ndog");
    printf("Respuesta del servidor:\n%s\n", buffer);
    sock = connectToServer();
    sendCommand(sock, "GET casa\n", buffer);
    printf("Respuesta esperada:\n%s\n", "NOTFOUND");
    printf("Respuesta del servidor:\n%s\n", buffer);
    // Test: DEL
    sock = connectToServer();
    sendCommand(sock, "DEL perro\n", buffer);
    printf("Respuesta esperada:\n%s\n", "OK");
    printf("Respuesta del servidor:\n%s\n", buffer);
    // Test: GET otra vez (debería dar NOTFOUND)
    sock = connectToServer();
    sendCommand(sock, "GET perro\n", buffer);
    printf("Respuesta esperada:\n%s\n", "NOTFOUND");
    printf("Respuesta del servidor:\n%s\n", buffer);

    // Cerrar conexión
    close(sock);
    return 0;
}

void sendCommand(int sock, const char* cmd, char buffer[]) {
    int len = strlen(cmd);
    if (write(sock, cmd, len) != len) {
        perror("write");
        exit(EXIT_FAILURE);
    }

    int n = read(sock, buffer, MAX_MSG_LENGTH - 1);
    if (n < 0) {
        perror("read");
        exit(EXIT_FAILURE);
    }

    buffer[n] = '\0';
}

int connectToServer() {
    // Creamos socket
    int s = socket(PF_INET, SOCK_STREAM, 0);

    // Cargamos datos de direccion de server
    struct sockaddr_in serveraddr;
    bzero((char*)&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_IP, &(serveraddr.sin_addr)) <= 0) {
        fprintf(stderr, "ERROR invalid server IP\n");
        exit(EXIT_FAILURE);
    }

    // Ejecutamos connect()
    if (connect(s, (const struct sockaddr*)&serveraddr, sizeof(serveraddr)) < 0) {
        fprintf(stderr, "ERROR connecting\n");
        close(s);
        exit(EXIT_FAILURE);
    }

    return s;
}