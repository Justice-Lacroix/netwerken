#ifdef _WIN32
#define _WIN32_WINNT _WIN32_WINNT_WIN7 // Punt 1: UDP SOCKETS correct: het aanmaken, verbinden en opruimen van sockets
#include <winsock2.h> //for all socket programming
#include <ws2tcpip.h> //for getaddrinfo, inet_pton, inet_ntop
#include <stdio.h> //for fprintf
#include <unistd.h> //for close
#include <stdlib.h> //for exit
#include <string.h> //for memset
#include <time.h> //for time
#include <stdbool.h> //for bool type

void OSInit(void) // Punt 9: CLEAN CODE: De initialisatie van het besturingssysteem
{
    //Step 1.0 -> WSA start
    WSADATA wsaData;
    int WSAError = WSAStartup(MAKEWORD(2, 0), &wsaData);

    //Error
    if (WSAError != 0)
    {
        fprintf(stderr, "WSAStartup errno = %d\n", WSAError);
        exit(-1);
    }
}
void OSCleanup(void) // Punt 9: CLEAN CODE: De opruiming van het besturingssysteem
{
    WSACleanup();
}
#define perror(string) fprintf( stderr, string ": WSA errno = %d\n", WSAGetLastError() )
#else
#include <sys/socket.h> //for sockaddr, socket, socket
#include <sys/types.h> //for size_t
#include <netdb.h> //for getaddrinfo
#include <netinet/in.h> //for sockaddr_in
#include <arpa/inet.h> //for htons, htonl, inet_pton, inet_ntop
#include <errno.h> //for errno
#include <stdio.h> //for fprintf, perror
#include <unistd.h> //for close
#include <stdlib.h> //for exit
#include <string.h> //for memset
#include <time.h> //for time
#include <stdbool.h> //for bool type
void OSInit(void) {} // Punt 9: CLEAN CODE: De initialisatie van het besturingssysteem
void OSCleanup(void) {} // Punt 9: CLEAN CODE: De opruiming van het besturingssysteem
#endif

int initialization(); // Punt 1: UDP SOCKETS correct: het aanmaken, verbinden en opruimen van sockets
void execution(int internet_socket); // Punt 3: TIME-OUT correct: De uitvoering van het spel inclusief time-outs
void cleanup(int internet_socket); // Punt 1: UDP SOCKETS correct: het aanmaken, verbinden en opruimen van sockets

int asciiToInt(const char* str); // Functie om ASCII naar integer te converteren
bool check_timeout(clock_t start, int timeout_seconds); // Functie om time-outs te controleren

int main(int argc, char* argv[])
{
    srand(time(NULL));
    //////////////////
    //Initialization//
    //////////////////

    // Punt 9: CLEAN CODE: Initialisatie van het programma
    OSInit();

    // Punt 1: UDP SOCKETS correct: Het internet_socket wordt geïnitialiseerd.
    int internet_socket = initialization();

    /////////////
    //Execution//
    /////////////

    // Punt 3: TIME-OUT correct: De uitvoering van het spel inclusief time-outs
    execution(internet_socket);

    ////////////
    //Clean up//
    ////////////

    // Punt 1: UDP SOCKETS correct: het aanmaken, verbinden en opruimen van sockets
    cleanup(internet_socket);

    //printf("Compiler Works!");
    return 0;
}

int initialization()
{
    //Step 1.1 -> meegeven naar waar het pakket gaat, tipisch voor C
    struct addrinfo internet_address_setup;
    struct addrinfo* internet_address_result = NULL;
    memset(&internet_address_setup, 0, sizeof internet_address_setup); //Initialiseren op '0' dat er geen "Rommel" in zit
    internet_address_setup.ai_family = AF_UNSPEC;
    internet_address_setup.ai_socktype = SOCK_DGRAM; //SOCK_DGRAM is om UDP socket te kunnen openen
    internet_address_setup.ai_flags = AI_PASSIVE;    //We mogen van elk IP adress een verbinding mogen verwachten
    int getaddrinfo_return = getaddrinfo(NULL, "24042", &internet_address_setup, &internet_address_result); //Adress = NULL omdat we een server zijn

    //Error 
    if (getaddrinfo_return != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(getaddrinfo_return));
        exit(1);
    }


    int internet_socket = -1;
    struct addrinfo* internet_address_result_iterator = internet_address_result;

    while (internet_address_result_iterator != NULL) //Zolang er resultaten zijn...
    {
        //Step 1.2 -> socket aanmaken
        internet_socket = socket(internet_address_result_iterator->ai_family, internet_address_result_iterator->ai_socktype, internet_address_result_iterator->ai_protocol);

        //Error
        if (internet_socket == -1)
        {
            perror("socket");
        }
        else
        {
            //Step 1.3 -> bind
            int bind_return = bind(internet_socket, internet_address_result_iterator->ai_addr, internet_address_result_iterator->ai_addrlen);

            //Error
            if (bind_return == -1)
            {
                close(internet_socket);
                perror("bind");
            }
            else
            {
                break;
            }
        }
        internet_address_result_iterator = internet_address_result_iterator->ai_next;
    }

    freeaddrinfo(internet_address_result);    //Opruimen

    if (internet_socket == -1)
    {
        fprintf(stderr, "socket: no valid socket address found\n");
        exit(2);
    }

    return internet_socket;
}

void execution(int internet_socket)
{
    int initial_timeout_seconds = 8;  // Initiële time-out waarde
    int timeout_seconds = initial_timeout_seconds;

    while (1) {
        int highest = 0;
        clock_t start_time = clock();
        int target = rand() % 255 + 1;
        printf("Target number generated: %d\n", target);

        int closest_guess = -1;
        int closest_distance = INT_MAX;
        bool timeout = false;
        while (!timeout) {
            // Ontvang gok van client
            struct sockaddr_storage client_address;
            socklen_t client_address_len = sizeof client_address;
            char buffer[1000];
            int bytes_received = recvfrom(internet_socket, buffer, sizeof(buffer), 0, (struct sockaddr*)&client_address, &client_address_len);
            if (bytes_received < 0) {
                perror("recvfrom");
                break;
            }

            // Check for timeout
            if (check_timeout(start_time, timeout_seconds)) {
                printf("Timeout occurred. Sending result to client...\n");
                char result_msg[100];
                if (closest_guess != -1) {
                    sprintf(result_msg, "You lost! Closest guess was %d.", closest_guess);
                }
                else {
                    sprintf(result_msg, "You won! No guesses were made.");
                }
                sendto(internet_socket, result_msg, strlen(result_msg), 0, (struct sockaddr*)&client_address, client_address_len);
                timeout = true;
                break;
            }

            buffer[bytes_received] = '\0';
            int guess = asciiToInt(buffer);
            printf("Received guess from client: %d\n", guess);

            // Update closest guess
            int distance = abs(target - guess);
            if (distance < closest_distance) {
                closest_distance = distance;
                closest_guess = guess;
            }

            // Check for correct guess
            if (guess == target) {
                printf("Client guessed the correct number! Sending result...\n");
                char result_msg[100];
                sprintf(result_msg, "You won! The correct number was %d.", target);
                sendto(internet_socket, result_msg, strlen(result_msg), 0, (struct sockaddr*)&client_address, client_address_len);
                timeout = true;
                break;
            }

            // Halveer de time-out waarde na elke gok
            timeout_seconds /= 2;

            // Check voor time-out met de bijgewerkte time-out waarde
            if (check_timeout(start_time, timeout_seconds)) {
                printf("Timeout occurred. Sending result to client...\n");
                char result_msg[100];
                if (closest_guess != -1) {
                    sprintf(result_msg, "You lost! Closest guess was %d.", closest_guess);
                }
                else {
                    sprintf(result_msg, "You won! No guesses were made.");
                }
                sendto(internet_socket, result_msg, strlen(result_msg), 0, (struct sockaddr*)&client_address, client_address_len);
                timeout = true;
                break;
            }
        }

        // Reset de time-out waarde voor de volgende ronde
        timeout_seconds = initial_timeout_seconds;
    }
}


void cleanup(int internet_socket)
{
    //Step 3.1
    close(internet_socket);
}

int asciiToInt(const char* str) {
    int result = 0;
    while (*str && isdigit(*str)) {
        result = result * 10 + (*str - '0');
        str++;
    }
    return result;
}

bool check_timeout(clock_t start, int timeout_seconds) {
    clock_t current = clock();
    double elapsed = (double)(current - start) / CLOCKS_PER_SEC;
    return elapsed >= timeout_seconds;
}
