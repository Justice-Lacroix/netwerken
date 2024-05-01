#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT _WIN32_WINNT_WIN7
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <unistd.h>
void OSInit(void)
{
#ifdef _WIN32
    WSADATA wsaData;
    int WSAError = WSAStartup(MAKEWORD(2, 0), &wsaData); // Initialiseer Winsock
    if (WSAError != 0)
    {
        fprintf(stderr, "WSAStartup errno = %d\n", WSAError);
        exit(-1);
    }
#endif
}

void OSCleanup(void)
{
#ifdef _WIN32
    WSACleanup(); // Cleanup Winsock
#endif
}
#define perror(string) fprintf( stderr, string ": WSA errno = %d\n", WSAGetLastError() ) // Defineer perror voor Windows
#else
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#endif

#define MAXDATASIZE 100

int initialization();
int connection(int internet_socket);
void execution(int internet_socket);
void cleanup(int internet_socket, int client_internet_socket);

int main(int argc, char* argv[])
{
    //////////////////
    //Initialization//
    //////////////////

    OSInit(); // Initialiseer het besturingssysteem (Winsock voor Windows)

    int internet_socket = initialization(); // Initialiseer het internet_socket voor communicatie

    //////////////
    //Connection//
    //////////////

    int client_internet_socket = connection(internet_socket); // Wacht op een verbinding met een client

    /////////////
    //Execution//
    /////////////

    execution(client_internet_socket); // Voer het hoofdgedeelte van het programma uit

    ////////////
    //Clean up//
    ////////////

    cleanup(internet_socket, client_internet_socket); // Ruim alle geopende sockets op

    OSCleanup(); // Ruim het besturingssysteem op (Winsock voor Windows)

    return 0;
}

// Functie voor het initialiseren van het internet_socket voor communicatie
int initialization()
{
    struct addrinfo internet_address_setup;
    struct addrinfo* internet_address_result;
    memset(&internet_address_setup, 0, sizeof internet_address_setup);
    internet_address_setup.ai_family = AF_UNSPEC;
    internet_address_setup.ai_socktype = SOCK_STREAM;
    internet_address_setup.ai_flags = AI_PASSIVE;
    int getaddrinfo_return = getaddrinfo(NULL, "24042", &internet_address_setup, &internet_address_result); // Haal adresinformatie op voor het binden van het socket
    if (getaddrinfo_return != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(getaddrinfo_return));
        exit(1);
    }

    int internet_socket = -1;
    struct addrinfo* internet_address_result_iterator = internet_address_result;
    while (internet_address_result_iterator != NULL)
    {
        internet_socket = socket(internet_address_result_iterator->ai_family, internet_address_result_iterator->ai_socktype, internet_address_result_iterator->ai_protocol); // Maak een socket
        if (internet_socket == -1)
        {
            perror("socket");
        }
        else
        {
            int bind_return = bind(internet_socket, internet_address_result_iterator->ai_addr, internet_address_result_iterator->ai_addrlen); // Bind het socket aan een adres
            if (bind_return == -1)
            {
                perror("bind");
                close(internet_socket);
            }
            else
            {
                int listen_return = listen(internet_socket, 1); // Begin met luisteren op het socket
                if (listen_return == -1)
                {
                    close(internet_socket);
                    perror("listen");
                }
                else
                {
                    break; // Stop de lus als alles goed gaat
                }
            }
        }
        internet_address_result_iterator = internet_address_result_iterator->ai_next;
    }

    freeaddrinfo(internet_address_result); // Maak de lijst met adresinformatie vrij

    if (internet_socket == -1)
    {
        fprintf(stderr, "socket: no valid socket address found\n");
        exit(2);
    }

    return internet_socket;
}

// Functie voor het accepteren van een verbinding met een client
int connection(int internet_socket)
{
    struct sockaddr_storage client_addr;
    socklen_t addr_size = sizeof client_addr;
    int client_socket = accept(internet_socket, (struct sockaddr*)&client_addr, &addr_size); // Accepteer de verbinding
    if (client_socket == -1)
    {
        perror("accept");
        close(internet_socket);
        exit(1);
    }
    return client_socket;
}

// Functie voor het uitvoeren van het hoofdgedeelte van het programma
void execution(int client_internet_socket)
{
    srand(time(NULL)); // Initialiseer de randomizer
    int generated_number = rand() % 1000000; // Genereer een willekeurig nummer
    int generated_number_network_order = htonl(generated_number); // Zet het nummer om naar netwerk-bytevolgorde

    char recv_buffer[MAXDATASIZE];
    int num_bytes_received;

    while (1)
    {
        num_bytes_received = recv(client_internet_socket, recv_buffer, MAXDATASIZE - 1, 0); // Ontvang een bericht van de client
        if (num_bytes_received == -1)
        {
            perror("recv");
            exit(1);
        }
        recv_buffer[num_bytes_received] = '\0';

        int guess = atoi(recv_buffer); // Zet het ontvangen bericht om naar een integer

        char response[MAXDATASIZE];
        if (guess < ntohl(generated_number_network_order))
        {
            strcpy(response, "Lager\n"); // Stuur 'Lager' als de gok te laag is
        }
        else if (guess > ntohl(generated_number_network_order))
        {
            strcpy(response, "Hoger\n"); // Stuur 'Hoger' als de gok te hoog is
        }
        else
        {
            strcpy(response, "Correct\n"); // Stuur 'Correct' als de gok correct is
            int num_bytes_sent = send(client_internet_socket, response, strlen(response), 0); // Stuur het antwoord naar de client
            if (num_bytes_sent == -1)
            {
                perror("send");
                exit(1);
            }
            continue; // Blijf in de loop en wacht op een nieuwe gok van dezelfde client
        }

        int num_bytes_sent = send(client_internet_socket, response, strlen(response), 0); // Stuur het antwoord naar de client
        if (num_bytes_sent == -1)
        {
            perror("send");
            exit(1);
        }
    }
}

// Functie voor het opruimen van geopende sockets
void cleanup(int internet_socket, int client_internet_socket)
{
    int shutdown_return = shutdown(client_internet_socket, SD_RECEIVE); // Stop het ontvangen op het client-socket
    if (shutdown_return == -1)
    {
        perror("shutdown");
    }

    close(client_internet_socket); // Sluit het client-socket
    close(internet_socket); // Sluit het server-socket
}
