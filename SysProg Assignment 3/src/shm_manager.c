//
//FILE               : shm_manager.c
//PROJECT            : SysProg Assign 3
//PROGRAMMER         : Bibi Murwared Enayat Zada
//FIRST VERSION      : 2025-11-14
//DESCRIPTION        : This program manages the shared memory for Assignment 3.
//                     It creates shared memory and a semaphore, allows reading
//                     the stored trips, and destroys the shared memory using
//                     the required rogue write.
//

#include "ipc_shared.h"

//Global variables
int shmid = -1;
int semid = -1;
SharedMemory *shm = NULL;

//Function prototypes
void display_menu();
void create_shared_memory();
void read_shared_memory();
void kill_shared_memory();
void cleanup();
int validate_destination(char *dest);
int ask_yes_no(const char *msg);

int main(void) {
    int choice;

    printf("=== Shared Memory Manager ===\n\n");

    while (1) {
        display_menu();

        if (scanf("%d", &choice) != 1) {
            printf("Invalid input! Please enter 1-4.\n");
            while (getchar() != '\n');
            continue;
        }
        getchar();

        switch (choice) {
            case 1:
                create_shared_memory();
                break;
            case 2:
                read_shared_memory();
                break;
            case 3:
                kill_shared_memory();
                break;
            case 4:
                cleanup();
                printf("Exiting...\n");
                exit(0);
            default:
                printf("Invalid choice! Please select 1-4.\n");
        }
    }

    return 0;
}

//
//FUNCTION     : display_menu
//DESCRIPTION  : Prints the menu options
//PARAMETERS   : None
//RETURNS      : Nothing
//
void display_menu(void) {
    printf("\n=== Shared Memory Manager Menu ===\n");
    printf("1. Create shared memory\n");
    printf("2. Read shared memory\n");
    printf("3. Kill shared memory with rogue write\n");
    printf("4. Exit\n");
    printf("Enter choice: ");
}

//
//FUNCTION     : create_shared_memory
//DESCRIPTION  : Creates shared memory and semaphore, initializes trips
//PARAMETERS   : None
//RETURNS      : Nothing
//
void create_shared_memory(void) {
    if (shmid != -1) {
        printf("Shared memory already exists!\n");
        return;
    }

    //Create shared memory
    shmid = shmget(SHM_KEY, sizeof(SharedMemory), PERMISSIONS | IPC_CREAT);
    if (shmid == -1) {
        perror("Unable to create shared memory");
        return;
    }

    //Attach shared memory
    shm = (SharedMemory *)shmat(shmid, NULL, 0);
    if (shm == (void *)-1) {
        perror("Failed to attach shared memory");
        shmctl(shmid, IPC_RMID, NULL);
        shmid = -1;
        return;
    }

    //Create semaphore
    semid = create_semaphore(SEM_KEY);
    if (semid == -1) {
        shmdt(shm);
        shmctl(shmid, IPC_RMID, NULL);
        shmid = -1;
        shm = NULL;
        printf("Failed to create semaphore\n");
        return;
    }

    //Initialize trips
    sem_lock(semid);
    shm->tripCount = 0;
    for (int i = 0; i < MAX_TRIPS; i++) {
        shm->trips[i].active = 0;
    }
    sem_unlock(semid);

    printf("Shared memory and semaphore created successfully.\n");

    //Ask to add trips
    if (!ask_yes_no("\nWould you like to add trips now")) {
        shmdt(shm);
        return;
    }

    while (1) {
        if (shm->tripCount >= MAX_TRIPS) {
            printf("Maximum trips reached!\n");
            break;
        }

        Trip newTrip;
        char input[100];

        //Get destination
        while (1) {
            printf("\nEnter destination: ");
            fgets(input, sizeof(input), stdin);
            input[strcspn(input, "\n")] = 0;

            strncpy(newTrip.destination, input, MAX_NAME - 1);
            newTrip.destination[MAX_NAME - 1] = '\0';

            if (strlen(newTrip.destination) == 0) {
                printf("Destination cannot be empty!\n");
                continue;
            }
            if (!validate_destination(newTrip.destination)) {
                printf("Invalid destination. Letters and spaces only.\n");
                continue;
            }
            break;
        }

        //Get price
        while (1) {
            printf("Enter price: ");
            fgets(input, sizeof(input), stdin);

            float priceVal;
            if (sscanf(input, "%f", &priceVal) == 1 && priceVal > 0) {
                newTrip.price = priceVal;
                break;
            }
            printf("Invalid price. Please enter a positive number.\n");
        }

        newTrip.active = 1;

        //Add trip to shared memory
        sem_lock(semid);
        shm->trips[shm->tripCount] = newTrip;
        shm->tripCount++;
        sem_unlock(semid);

        printf("Trip added successfully!\n");

        if (!ask_yes_no("Add another trip"))
            break;
    }

    shmdt(shm);
}

//
//FUNCTION     : read_shared_memory
//DESCRIPTION  : Displays all trips in shared memory
//PARAMETERS   : None
//RETURNS      : Nothing
//
void read_shared_memory(void) {
    shmid = shmget(SHM_KEY, sizeof(SharedMemory), PERMISSIONS);
    if (shmid == -1) {
        printf("Unable to connect to shared memory.\n");
        return;
    }

    semid = get_semaphore(SEM_KEY);
    if (semid == -1) {
        printf("Unable to connect to semaphore.\n");
        return;
    }

    shm = (SharedMemory *)shmat(shmid, NULL, 0);
    if (shm == (void *)-1) {
        printf("Unable to attach to shared memory.\n");
        return;
    }

    sem_lock(semid);

    printf("\n=== Available Trips ===\n");
    printf("Total trips: %d\n", shm->tripCount);

    if (shm->tripCount == 0) {
        printf("No trips available.\n");
    } else {
        for (int i = 0; i < shm->tripCount; i++) {
            if (shm->trips[i].active) {
                printf("%d. %s - $%.2f\n", i + 1,
                       shm->trips[i].destination,
                       shm->trips[i].price);
            }
        }
    }

    sem_unlock(semid);
    shmdt(shm);
    shm = NULL;
}

//
//FUNCTION     : kill_shared_memory
//DESCRIPTION  : Performs rogue write and deletes shared memory
//PARAMETERS   : None
//RETURNS      : Nothing
//
void kill_shared_memory(void) {
    if (shmid == -1) {
        shmid = shmget(SHM_KEY, sizeof(SharedMemory), PERMISSIONS);
        if (shmid == -1) {
            printf("Shared memory not found!\n");
            return;
        }
    }

    shm = (SharedMemory *)shmat(shmid, NULL, 0);
    if (shm == (void *)-1) {
        printf("Unable to attach to shared memory.\n");
        return;
    }

    if (semid == -1) {
        semid = get_semaphore(SEM_KEY);
    }

    if (semid != -1) {
        sem_lock(semid);
    }

    printf("\nAttempting rogue write to kill shared memory...\n");

    //Attempt to write to out-of-range memory address
    char *rogue_ptr = (char *)shm + sizeof(SharedMemory) + 1000;
    printf("Writing to address: %p (out of bounds)\n", (void *)rogue_ptr);

    *rogue_ptr = 'X';

    if (semid != -1) {
        sem_unlock(semid);
    }

    printf("Rogue write completed.\n");

    shmdt(shm);
    cleanup();
    shmid = -1;
    shm = NULL;
    semid = -1;
}

//
//FUNCTION     : cleanup
//DESCRIPTION  : Removes shared memory and semaphore
//PARAMETERS   : None
//RETURNS      : Nothing
//
void cleanup(void) {
    if (shm != NULL) {
        shmdt(shm);
    }
    if (shmid != -1) {
        shmctl(shmid, IPC_RMID, NULL);
        printf("Shared memory removed.\n");
    }
    if (semid != -1) {
        remove_semaphore(semid);
        printf("Semaphore removed.\n");
    }
}

//
//FUNCTION     : validate_destination
//DESCRIPTION  : Checks if destination contains only letters and spaces
//PARAMETERS   : char *dest - destination string
//RETURNS      : 1 if valid, 0 if invalid
//
int validate_destination(char *dest) {
    if (strlen(dest) == 0)
        return 0;

    for (int i = 0; i < strlen(dest); i++) {
        if (!isalpha(dest[i]) && dest[i] != ' ') {
            return 0;
        }
    }
    return 1;
}

//
//FUNCTION     : ask_yes_no
//DESCRIPTION  : Prompts user for yes/no answer
//PARAMETERS   : const char *msg - prompt message
//RETURNS      : 1 for yes, 0 for no
//
int ask_yes_no(const char *msg) {
    char input[10];

    while (1) {
        printf("%s (y/n): ", msg);
        fgets(input, sizeof(input), stdin);

        if (strlen(input) > 0) {
            char c = input[0];
            if (c == 'y' || c == 'Y')
                return 1;
            if (c == 'n' || c == 'N')
                return 0;
        }

        printf("Invalid choice. Please type y or n.\n");
    }
}