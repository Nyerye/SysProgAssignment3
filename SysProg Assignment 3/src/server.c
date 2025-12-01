//
//FILE          : server.c
//PROJECT       : SysProg Assignment 3
//PROGRAMMER    : Nicholas Reilly
//FIRST VERSION : Nov 19 2025
//DESCRIPTION   : TCP server that accepts multiple client connections,
//               receives client data, and displays it.
//               Uses fork() to handle multiple clients.
//               [NCURSES] Enhanced with ncurses GUI windows.
//

#include "ipc_shared.h"
#include <ncurses.h>  //[NCURSES]

//Global variables
float totalPrice = 0.0;
int recordCount = 0;
volatile sig_atomic_t running = 1;
int server_socket = -1;

//[NCURSES] Global ncurses windows
WINDOW *display_win, *input_win;

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

    //Set up signal handler (Ctrl+C encerra o servidor)
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    //Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Socket creation failed");
        exit(1);
    }

    //Reuse address
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("setsockopt");
        close(server_socket);
        exit(1);
    }

    //Configure server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port        = htons(SERVER_PORT);

    //Bind
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Bind failed");
        close(server_socket);
        exit(1);
    }

    //Listen
    if (listen(server_socket, BACKLOG) == -1) {
        perror("Listen failed");
        close(server_socket);
        exit(1);
    }

    //===NCURSES with no fork===
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    refresh();

    int display_height = DISPLAY_HEIGHT;
    int input_height   = INPUT_HEIGHT;

    display_win = newwin(display_height, COLS, 0, 0);
    input_win   = newwin(input_height,  COLS, display_height, 0);
    scrollok(display_win, TRUE);
    box(display_win, 0, 0);
    box(input_win, 0, 0);
        
    keypad(input_win, TRUE);

    //Initial messages
    wprintw(display_win, "Server listening on port %d...\n", SERVER_PORT);
    wprintw(display_win, "Waiting for client connections...\n\n");
    wprintw(input_win, "Server running. Use Ctrl+C to stop.\n");
    wrefresh(display_win);
    wrefresh(input_win);

    while (running) {
        client_len = sizeof(client_addr);

        client_socket = accept(server_socket,
                               (struct sockaddr *)&client_addr,
                               &client_len);
        if (client_socket == -1) {
            if (errno == EINTR) {
                //Interrupted by signal, continue loop to check running flag
                continue;
            }
            wprintw(display_win, "Accept failed: %s\n", strerror(errno));
            wrefresh(display_win);
            continue;
        }

        client_count++;
        wprintw(display_win, "Client %d connected from %s\n",
                client_count,
                inet_ntoa(client_addr.sin_addr));
        wrefresh(display_win);

        //Handle client in the main process (no fork)
        handle_client(client_socket, client_count);

        close(client_socket);
        wprintw(display_win, "Client %d finished.\n", client_count);
        wrefresh(display_win);
    }

    //Cleanup
    close(server_socket);
    endwin();
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

        endwin(); //[NCURSES]
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
    //[NCURSES] Use ncurses output
    wprintw(display_win, "Client%d | %s %s | Age:%d | %s | %s | People:%d | $%.2f\n",
           msg->clientId,
           msg->firstName,
           msg->lastName,
           msg->age,
           msg->address,
           msg->destination,
           msg->numPeople,
           msg->tripPrice);
    wrefresh(display_win);

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

    //[NCURSES] Also show in ncurses input window
    wprintw(input_win, "=== SUMMARY ===\n");
    wprintw(input_win, "Records: %d | Total: $%.2f\n", recordCount, totalPrice);
    wrefresh(input_win);
}

//
//FUNCTION     : handle_client
//DESCRIPTION  : Handles communication with a connected client
//PARAMETERS   : int client_socket - socket descriptor for client
//              int client_num - client identifier number
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
                wprintw(display_win, "Client %d disconnected\n", client_num);
            } else {
                wprintw(display_win, "Error receiving from Client %d\n", client_num);
            }
            wrefresh(display_win);
            break;
        }

        //Check for control signals
        if (msg.signal == SIGNAL_F1) {
            wprintw(display_win, "Client %d sent exit signal\n", client_num);
            wrefresh(display_win);
            break;
        } else if (msg.signal == SIGNAL_F2) {
            wprintw(display_win, "Client %d requested total display\n", client_num);
            wrefresh(display_win);
            show_total();
        } else {
            //Normal data message
            msg.clientId = client_num;  //Assign client ID
            display_client(&msg);
        }
    }
}
