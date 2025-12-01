//
//FILE          : server.c
//PROJECT       : SysProg Assignment 3
//PROGRAMMER    : Nicholas Reilly
//FIRST VERSION : Nov 19 2025
//DESCRIPTION   : TCP server that accepts multiple client connections,
//                receives client data, and displays it.
//                Uses fork() to handle multiple clients.
//

#include "ipc_shared.h"

//Global variables
float totalPrice = 0.0;
int recordCount = 0;
volatile sig_atomic_t running = 1;
int server_socket = -1;

//Function prototypes
void signal_handler(int signum);
void display_client(ClientMessage *msg);
void handle_client(int client_socket, int client_num);
void show_total();

int main(void) {
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_socket;
    int client_count = 0;

    //Set up signal handler
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    //Ignore SIGCHLD to prevent zombie processes
    signal(SIGCHLD, SIG_IGN);

    printf("=== Server (Reader) ===\n");
    printf("Starting...\n\n");

    //Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Socket creation failed");
        exit(1);
    }

    //Set socket options to reuse address
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("Setsockopt failed");
        close(server_socket);
        exit(1);
    }

    //Configure server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERVER_PORT);

    //Bind socket
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Bind failed");
        close(server_socket);
        exit(1);
    }

    //Listen for connections
    if (listen(server_socket, BACKLOG) == -1) {
        perror("Listen failed");
        close(server_socket);
        exit(1);
    }

    printf("Server listening on port %d...\n", SERVER_PORT);
    printf("Waiting for client connections...\n");
    printf("Press Ctrl+C to stop server.\n\n");

    //Main accept loop
    while (running) {
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);

        if (client_socket == -1) {
            if (errno == EINTR) {
                continue;  //Interrupted by signal
            }
            perror("Accept failed");
            continue;
        }

        client_count++;
        printf("Client %d connected from %s\n",
               client_count,
               inet_ntoa(client_addr.sin_addr));

        //Fork to handle client
        pid_t pid = fork();

        if (pid == 0) {
            //Child process
            close(server_socket);  //Child doesn't need server socket
            handle_client(client_socket, client_count);
            close(client_socket);
            exit(0);
        } else if (pid > 0) {
            //Parent process
            close(client_socket);  //Parent doesn't need client socket
        } else {
            perror("Fork failed");
            close(client_socket);
        }
    }

    //Cleanup
    close(server_socket);

    return 0;
}

//
//FUNCTION     : signal_handler
//DESCRIPTION  : Handles SIGINT (Ctrl+C) to shutdown server gracefully
//PARAMETERS   : int signum - signal number
//RETURNS      : Nothing
//
void signal_handler(int signum) {
    if (signum == SIGINT) {
        running = 0;
        printf("\n\nReceived SIGINT (Ctrl+C)...\n");
        show_total();

        if (server_socket != -1) {
            close(server_socket);
        }

        exit(0);
    }
}

//
//FUNCTION     : display_client
//DESCRIPTION  : Displays client message in one formatted line
//PARAMETERS   : ClientMessage *msg - client message structure
//RETURNS      : Nothing
//
void display_client(ClientMessage *msg) {
    printf("Client%d | %s %s | Age:%d | %s | %s | People:%d | $%.2f\n",
           msg->clientId,
           msg->firstName,
           msg->lastName,
           msg->age,
           msg->address,
           msg->destination,
           msg->numPeople,
           msg->tripPrice);

    totalPrice += msg->tripPrice;
    recordCount++;
}

//
//FUNCTION     : show_total
//DESCRIPTION  : Displays summary of total records and price
//PARAMETERS   : None
//RETURNS      : Nothing
//
void show_total() {
    printf("\n=== SUMMARY ===\n");
    printf("Records: %d | Total: $%.2f\n\n", recordCount, totalPrice);
}

//
//FUNCTION     : handle_client
//DESCRIPTION  : Handles communication with a connected client
//PARAMETERS   : int client_socket - socket descriptor for client
//               int client_num - client identifier number
//RETURNS      : Nothing
//
void handle_client(int client_socket, int client_num) {
    ClientMessage msg;
    ssize_t bytes_received;

    while (1) {
        bytes_received = recv(client_socket, &msg, sizeof(ClientMessage), 0);

        if (bytes_received <= 0) {
            //Client disconnected or error
            if (bytes_received == 0) {
                printf("Client %d disconnected\n", client_num);
            } else {
                printf("Error receiving from Client %d\n", client_num);
            }
            break;
        }

        //Check for control signals
        if (msg.signal == SIGNAL_F1) {
            printf("Client %d sent exit signal\n", client_num);
            break;
        } else if (msg.signal == SIGNAL_F2) {
            printf("Client %d requested total display\n", client_num);
            show_total();
        } else {
            //Normal data message
            msg.clientId = client_num;  //Assign client ID
            display_client(&msg);
        }
    }
}