//
//FILE          : client.c
//PROJECT       : SysProg Assignment 3
//PROGRAMMER    : Josiah Williams
//FIRST VERSION : 2025-11-08
//DESCRIPTION   : TCP client that gathers trip and client information,
//               reads available trips from shared memory, and sends
//               data to server via socket.
//               [NCURSES] Enhanced with ncurses GUI windows.
//

#include "ipc_shared.h"
#include <ncurses.h>     //[NCURSES]

//----------------------------------------------------
//Global variables
//----------------------------------------------------
int client_socket = -1;
int shmid         = -1;
int semid         = -1;
SharedMemory *shm = NULL;

//[NCURSES] Global ncurses windows
WINDOW *display_win;
WINDOW *input_win;

//----------------------------------------------------
//Function prototypes
//----------------------------------------------------
void cleanup(void);
int  validate_name(char *name);
int  validate_age(int age);
void get_client_data(ClientMessage *msg);
void reset_input_window(void);

//
//FUNCTION     : main
//DESCRIPTION  : Entry point for the TCP client.
//              Connects to shared memory and server, then
//              enters ncurses-based interaction loop.
//PARAMETERS   : int argc, char *argv[] - optional server IP
//RETURNS      : int - exit code
//
int main(int argc, char *argv[])
{
    ClientMessage msg;
    struct sockaddr_in server_addr;
    char server_ip[20] = "127.0.0.1";    //default localhost
    char cont_input[32];

    //Optional server IP from command line
    if (argc > 1) {
        strncpy(server_ip, argv[1], sizeof(server_ip) - 1);
        server_ip[sizeof(server_ip) - 1] = '\0';
    }

    printf("=== Client (Writer) ===\n");
    printf("Starting...\n");

    //[NCURSES] Initialize ncurses interface
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
    keypad(input_win, TRUE);     //F1/F2 handled on this window

    wrefresh(display_win);
    wrefresh(input_win);

    //--------------------------------------------------
    //Attach to shared memory & semaphore
    //--------------------------------------------------
    shmid = shmget(SHM_KEY, sizeof(SharedMemory), PERMISSIONS);
    if (shmid == -1) {
        wprintw(display_win, "Error: start shm_manager and create shared memory first.\n");
        wrefresh(display_win);
        endwin();
        return 1;
    }

    shm = (SharedMemory *)shmat(shmid, NULL, 0);
    if (shm == (void *)-1) {
        perror("shmat");
        endwin();
        return 1;
    }

    semid = get_semaphore(SEM_KEY);
    if (semid == -1) {
        wprintw(display_win, "Semaphore not found! Run shm_manager first.\n");
        wrefresh(display_win);
        cleanup();
        endwin();
        return 1;
    }

    wprintw(display_win, "Connected to shared memory.\n");
    wrefresh(display_win);

    //--------------------------------------------------
    //Create socket and connect to server
    //--------------------------------------------------
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("socket");
        cleanup();
        endwin();
        return 1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons(SERVER_PORT);

    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        wprintw(display_win, "Invalid server address: %s\n", server_ip);
        wrefresh(display_win);
        cleanup();
        endwin();
        return 1;
    }

    wprintw(display_win, "Connecting to server at %s:%d...\n",
            server_ip, SERVER_PORT);
    wrefresh(display_win);

    if (connect(client_socket,
                (struct sockaddr *)&server_addr,
                sizeof(server_addr)) == -1) {
        perror("connect");
        wprintw(display_win, "Connection to server failed.\n");
        wrefresh(display_win);
        cleanup();
        endwin();
        return 1;
    }

    wprintw(display_win, "Connected to server successfully!\n");
    wrefresh(display_win);

    //--------------------------------------------------
    //Main ncurses loop
    //--------------------------------------------------
    for (;;) {
        //Reset command window for menu
        reset_input_window();
        nodelay(input_win, FALSE);   //blocking input

        mvwprintw(input_win, 1, 2,
                  "[F1]=Exit  [F2]=Total  [Enter]=New client");
        wrefresh(input_win);

        int ch = wgetch(input_win);

        if (ch == KEY_F(1)) {
            memset(&msg, 0, sizeof(msg));
            msg.signal = SIGNAL_F1;
            send(client_socket, &msg, sizeof(msg), 0);
            wprintw(display_win, "\nF1 pressed — closing client.\n");
            wrefresh(display_win);
            break;
        } else if (ch == KEY_F(2)) {
            memset(&msg, 0, sizeof(msg));
            msg.signal = SIGNAL_F2;
            send(client_socket, &msg, sizeof(msg), 0);
            wprintw(display_win, "\nF2 pressed — total requested from server.\n");
            wrefresh(display_win);
            continue;   //back to command menu
        }

        //Any other key: collect new client data
        memset(&msg, 0, sizeof(msg));
        msg.signal = 0;

        get_client_data(&msg);

        if (send(client_socket, &msg, sizeof(msg), 0) == -1) {
            perror("send");
            wprintw(display_win, "\nFailed to send data to server.\n");
            wrefresh(display_win);
            break;
        }

        wprintw(display_win, "\nClient data sent successfully.\n");
        wrefresh(display_win);

        //Ask if the user wants to enter another client
        reset_input_window();
        wprintw(input_win, "Enter more client data? (y/n): ");
        wrefresh(input_win);

        echo();
        wgetnstr(input_win, cont_input, sizeof(cont_input) - 1);
        noecho();

        //If user just hits ENTER (empty), keep going
        if (cont_input[0] == '\0') {
            continue;
        }

        if (cont_input[0] != 'y' && cont_input[0] != 'Y') {
            break;
        }
    }

    cleanup();
    endwin();
    return 0;
}

//
//FUNCTION     : cleanup
//DESCRIPTION  : Detaches shared memory and closes the client socket.
//PARAMETERS   : None
//RETURNS      : Nothing
//
void cleanup(void)
{
    if (shm != NULL) {
        shmdt(shm);
        shm = NULL;
    }
    if (client_socket != -1) {
        close(client_socket);
        client_socket = -1;
    }
}

//
//FUNCTION     : validate_name
//DESCRIPTION  : Checks if a name contains only letters and spaces.
//PARAMETERS   : char *name - name string to validate
//RETURNS      : 1 if valid, 0 otherwise
//
int validate_name(char *name)
{
    if (strlen(name) == 0) {
        return 0;
    }

    for (int i = 0; i < (int)strlen(name); i++) {
        if (!isalpha((unsigned char)name[i]) && name[i] != ' ') {
            return 0;
        }
    }

    return 1;
}

//
//FUNCTION     : validate_age
//DESCRIPTION  : Checks if age is within valid range.
//PARAMETERS   : int age - age to validate
//RETURNS      : 1 if valid, 0 otherwise
//
int validate_age(int age)
{
    return (age >= MIN_AGE && age <= MAX_AGE);
}

//
//FUNCTION     : get_client_data
//DESCRIPTION  : Uses ncurses to gather name, age, address,
//              trip choice and number of people from user,
//              reading available trips from shared memory.
//PARAMETERS   : ClientMessage *msg - structure to store client data
//RETURNS      : Nothing
//
void get_client_data(ClientMessage *msg)
{
    char fullName[MAX_FULLNAME];
    char *token;
    char input[200];

    //--------------------------------------------------
    //Get first and last name
    //--------------------------------------------------
    while (1) {
        reset_input_window();
        wprintw(input_win, "Enter client name (First Last): ");
        wrefresh(input_win);

        echo();
        wgetnstr(input_win, fullName, sizeof(fullName) - 1);
        noecho();

        fullName[strcspn(fullName, "\n")] = '\0';

        if (!validate_name(fullName)) {
            wprintw(input_win,
                    "\nInvalid name! Use only letters and spaces.\n");
            wrefresh(input_win);
            napms(1000);
            continue;
        }

        //Split into first and last name
        token = strtok(fullName, " ");
        if (token == NULL) {
            wprintw(input_win,
                    "\nPlease enter both first and last name!\n");
            wrefresh(input_win);
            napms(1000);
            continue;
        }
        strncpy(msg->firstName, token, MAX_NAME - 1);
        msg->firstName[MAX_NAME - 1] = '\0';

        token = strtok(NULL, " ");
        if (token == NULL) {
            wprintw(input_win,
                    "\nPlease enter both first and last name!\n");
            wrefresh(input_win);
            napms(1000);
            continue;
        }
        strncpy(msg->lastName, token, MAX_NAME - 1);
        msg->lastName[MAX_NAME - 1] = '\0';

        break;
    }

    //--------------------------------------------------
    //Get age
    //--------------------------------------------------
    while (1) {
        reset_input_window();
        wprintw(input_win, "Enter age: ");
        wrefresh(input_win);

        echo();
        wgetnstr(input_win, input, sizeof(input) - 1);
        noecho();

        if (sscanf(input, "%d", &msg->age) != 1) {
            wprintw(input_win,
                    "\nInvalid input! Please enter a number.\n");
            wrefresh(input_win);
            napms(1000);
            continue;
        }

        if (!validate_age(msg->age)) {
            wprintw(input_win,
                    "\nInvalid age! Please enter a value between %d and %d.\n",
                    MIN_AGE, MAX_AGE);
            wrefresh(input_win);
            napms(1000);
            continue;
        }

        break;
    }

    //--------------------------------------------------
    //Get address
    //--------------------------------------------------
    while (1) {
        reset_input_window();
        wprintw(input_win, "Enter address: ");
        wrefresh(input_win);

        echo();
        wgetnstr(input_win, msg->address, MAX_ADDRESS - 1);
        noecho();
        msg->address[strcspn(msg->address, "\n")] = '\0';

        if (strlen(msg->address) == 0) {
            wprintw(input_win, "\nAddress cannot be empty!\n");
            wrefresh(input_win);
            napms(1000);
            continue;
        }

        break;
    }

    //--------------------------------------------------
    //Display available trips from shared memory
    //--------------------------------------------------
    wprintw(display_win, "\n=== Available Trips ===\n");
    wrefresh(display_win);

    sem_lock(semid);

    if (shm->tripCount == 0) {
        wprintw(display_win, "\nNo trips available!\n");
        wrefresh(display_win);
        sem_unlock(semid);
        cleanup();
        endwin();
        exit(1);
    }

    for (int i = 0; i < shm->tripCount; i++) {
        if (shm->trips[i].active) {
            wprintw(display_win, "%d. %s - $%.2f\n",
                    i + 1,
                    shm->trips[i].destination,
                    shm->trips[i].price);
        }
    }
    wrefresh(display_win);

    //--------------------------------------------------
    //Select trip
    //--------------------------------------------------
    int tripChoice;
    while (1) {
        reset_input_window();
        wprintw(input_win, "Select trip number: ");
        wrefresh(input_win);

        echo();
        wgetnstr(input_win, input, sizeof(input) - 1);
        noecho();

        if (sscanf(input, "%d", &tripChoice) != 1) {
            wprintw(input_win,
                    "\nInvalid input! Please enter a number.\n");
            wrefresh(input_win);
            napms(1000);
            continue;
        }

        if (tripChoice < MIN_TRIP ||
            tripChoice > shm->tripCount ||
            !shm->trips[tripChoice - 1].active) {
            wprintw(input_win, "\nInvalid trip selection!\n");
            wrefresh(input_win);
            napms(1000);
            continue;
        }

        //Copy trip information
        strncpy(msg->destination,
                shm->trips[tripChoice - 1].destination,
                MAX_NAME - 1);
        msg->destination[MAX_NAME - 1] = '\0';
        msg->tripPrice = shm->trips[tripChoice - 1].price;

        break;
    }

    sem_unlock(semid);

    //--------------------------------------------------
    //Get number of people
    //--------------------------------------------------
    while (1) {
        reset_input_window();
        wprintw(input_win, "Enter number of people: ");
        wrefresh(input_win);

        echo();
        wgetnstr(input_win, input, sizeof(input) - 1);
        noecho();

        if (sscanf(input, "%d", &msg->numPeople) != 1) {
            wprintw(input_win, "\nInvalid input!\n");
            wrefresh(input_win);
            napms(1000);
            continue;
        }

        if (msg->numPeople < MIN_PEOPLE) {
            wprintw(input_win,
                    "\nNumber of people must be at least %d!\n",
                    MIN_PEOPLE);
            wrefresh(input_win);
            napms(1000);
            continue;
        }

        //Calculate total price
        msg->tripPrice *= msg->numPeople;
        break;
    }
}

//
//FUNCTION     : reset_input_window
//DESCRIPTION  : Clears and redraws the input window box, then
//              positions the cursor inside the border.
//PARAMETERS   : None
//RETURNS      : Nothing
//
void reset_input_window(void)
{
    werase(input_win);
    box(input_win, 0, 0);
    wmove(input_win, 1, 1);
    wrefresh(input_win);
}
