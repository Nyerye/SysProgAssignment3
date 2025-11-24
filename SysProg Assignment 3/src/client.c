//
// FILE          : client.c
// PROJECT       : SysProg Assignment 3
// PROGRAMMER    : Josiah Williams
// FIRST VERSION : 2025-11-08
// DESCRIPTION   : TCP client that gathers trip and client information,
//                 reads available trips from shared memory, and sends
//                 data to server via socket.
//

#include "ipc_shared.h"

// Global variables
int client_socket = -1;
int shmid = -1;
int semid = -1;
SharedMemory *shm = NULL;

// Function prototypes
void cleanup();
int validate_name(char *name);
int validate_age(int age);
void get_client_data(ClientMessage *msg);

int main(int argc, char *argv[]) {
    ClientMessage msg;
    struct sockaddr_in server_addr;
    char server_ip[20] = "127.0.0.1";  // Default localhost
    char choice;

    // Check for server IP argument
    if (argc > 1) {
        strncpy(server_ip, argv[1], sizeof(server_ip) - 1);
    }

    printf("=== Client (Writer) ===\n");
    printf("Starting...\n\n");

    // Get shared memory
    shmid = shmget(SHM_KEY, sizeof(SharedMemory), PERMISSIONS);
    if (shmid == -1) {
        printf("Error: Please start shared memory manager first!\n");
        exit(1);
    }

    // Attach shared memory
    shm = (SharedMemory *)shmat(shmid, NULL, 0);
    if (shm == (void *)-1) {
        perror("Failed to attach shared memory");
        exit(1);
    }

    // Get semaphore
    semid = get_semaphore(SEM_KEY);
    if (semid == -1) {
        printf("Semaphore not found! Please start shared memory manager first.\n");
        cleanup();
        exit(1);
    }

    printf("Connected to shared memory.\n");

    // Create socket
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("Socket creation failed");
        cleanup();
        exit(1);
    }

    // Configure server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);

    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        printf("Invalid server address\n");
        cleanup();
        exit(1);
    }

    // Connect to server
    printf("Connecting to server at %s:%d...\n", server_ip, SERVER_PORT);

    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        printf("Connection to server failed. Is server running?\n");
        cleanup();
        exit(1);
    }

    printf("Connected to server successfully!\n\n");
    printf("Commands: Type 'exit' to quit, 'total' to request total display\n\n");

    // Main client loop
    while (1) {
        // Get client data
        get_client_data(&msg);
        msg.signal = 0;  // Normal data message

        // Send message to server
        if (send(client_socket, &msg, sizeof(ClientMessage), 0) == -1) {
            perror("Failed to send data to server");
            cleanup();
            exit(1);
        }

        printf("\nClient data sent successfully!\n");

        // Ask if more data needs to be entered
        printf("\nEnter more client data? (y/n/total/exit): ");
        char input[20];
        fgets(input, sizeof(input), stdin);
        input[strcspn(input, "\n")] = 0;

        if (strlen(input) > 0) {
            if (strcmp(input, "exit") == 0) {
                // Send F1 signal
                memset(&msg, 0, sizeof(ClientMessage));
                msg.signal = SIGNAL_F1;
                send(client_socket, &msg, sizeof(ClientMessage), 0);
                printf("Exiting...\n");
                break;
            } else if (strcmp(input, "total") == 0) {
                // Send F2 signal
                memset(&msg, 0, sizeof(ClientMessage));
                msg.signal = SIGNAL_F2;
                send(client_socket, &msg, sizeof(ClientMessage), 0);
                printf("Total display requested\n\n");
                continue;
            }

            choice = input[0];
            if (choice != 'y' && choice != 'Y') {
                break;
            }
        }
    }

    printf("Exiting client program...\n");
    cleanup();

    return 0;
}

//
// FUNCTION     : cleanup
// DESCRIPTION  : Cleans up resources
// PARAMETERS   : None
// RETURNS      : Nothing
//
void cleanup() {
    if (shm != NULL) {
        shmdt(shm);
    }
    if (client_socket != -1) {
        close(client_socket);
    }
}

//
// FUNCTION     : validate_name
// DESCRIPTION  : Checks if name contains only letters and spaces
// PARAMETERS   : char *name - name string to validate
// RETURNS      : 1 if valid, 0 if invalid
//
int validate_name(char *name) {
    if (strlen(name) == 0) {
        return 0;
    }

    for (int i = 0; i < strlen(name); i++) {
        if (!isalpha(name[i]) && name[i] != ' ') {
            return 0;
        }
    }

    return 1;
}

//
// FUNCTION     : validate_age
// DESCRIPTION  : Checks if age is within valid range
// PARAMETERS   : int age - age to validate
// RETURNS      : 1 if valid, 0 if invalid
//
int validate_age(int age) {
    return age >= MIN_AGE && age <= MAX_AGE;
}

//
// FUNCTION     : get_client_data
// DESCRIPTION  : Gets client and trip information from user
// PARAMETERS   : ClientMessage *msg - structure to store client data
// RETURNS      : Nothing
//
void get_client_data(ClientMessage *msg) {
    char fullName[MAX_FULLNAME];
    char *token;
    char input[200];

    // Get first and last name
    while (1) {
        printf("\nEnter client name (First Last): ");
        fgets(fullName, sizeof(fullName), stdin);
        fullName[strcspn(fullName, "\n")] = 0;

        if (!validate_name(fullName)) {
            printf("Invalid name! Use only letters and spaces.\n");
            continue;
        }

        // Split into first and last name
        token = strtok(fullName, " ");
        if (token == NULL) {
            printf("Please enter both first and last name!\n");
            continue;
        }
        strncpy(msg->firstName, token, MAX_NAME - 1);
        msg->firstName[MAX_NAME - 1] = '\0';

        token = strtok(NULL, " ");
        if (token == NULL) {
            printf("Please enter both first and last name!\n");
            continue;
        }
        strncpy(msg->lastName, token, MAX_NAME - 1);
        msg->lastName[MAX_NAME - 1] = '\0';

        break;
    }

    // Get age
    while (1) {
        printf("Enter age: ");
        fgets(input, sizeof(input), stdin);

        if (sscanf(input, "%d", &msg->age) != 1) {
            printf("Invalid input! Please enter a number.\n");
            continue;
        }

        if (!validate_age(msg->age)) {
            printf("Invalid age! Please enter a value between %d and %d.\n",
                   MIN_AGE, MAX_AGE);
            continue;
        }

        break;
    }

    // Get address
    while (1) {
        printf("Enter address: ");
        fgets(msg->address, MAX_ADDRESS, stdin);
        msg->address[strcspn(msg->address, "\n")] = 0;

        if (strlen(msg->address) == 0) {
            printf("Address cannot be empty!\n");
            continue;
        }

        break;
    }

    // Display available trips from shared memory
    printf("\n=== Available Trips ===\n");

    sem_lock(semid);

    if (shm->tripCount == 0) {
        printf("No trips available!\n");
        sem_unlock(semid);
        cleanup();
        exit(1);
    }

    for (int i = 0; i < shm->tripCount; i++) {
        if (shm->trips[i].active) {
            printf("%d. %s - $%.2f\n", i + 1,
                   shm->trips[i].destination,
                   shm->trips[i].price);
        }
    }

    int tripChoice;
    while (1) {
        printf("Select trip number: ");
        fgets(input, sizeof(input), stdin);

        if (sscanf(input, "%d", &tripChoice) != 1) {
            printf("Invalid input! Please enter a number.\n");
            continue;
        }

        if (tripChoice < MIN_TRIP || tripChoice > shm->tripCount ||
            !shm->trips[tripChoice - 1].active) {
            printf("Invalid trip selection!\n");
            continue;
        }

        // Copy trip information
        strncpy(msg->destination, shm->trips[tripChoice - 1].destination, MAX_NAME - 1);
        msg->destination[MAX_NAME - 1] = '\0';
        msg->tripPrice = shm->trips[tripChoice - 1].price;

        break;
    }

    sem_unlock(semid);

    // Get number of people
    while (1) {
        printf("Enter number of people: ");
        fgets(input, sizeof(input), stdin);

        if (sscanf(input, "%d", &msg->numPeople) != 1) {
            printf("Invalid input!\n");
            continue;
        }

        if (msg->numPeople < MIN_PEOPLE) {
            printf("Number of people must be at least %d!\n", MIN_PEOPLE);
            continue;
        }

        // Calculate total price
        msg->tripPrice *= msg->numPeople;

        break;
    }
}