//
// FILE               : common.c
// PROJECT            : SysProg Assign 3
// PROGRAMMER         : Rodrigo P Gomes
// FIRST VERSION      : 2025-11-08
// DESCRIPTION        : Common functions shared across all programs including
//                      semaphore management for synchronizing access to shared memory
//

#include "ipc_shared.h"

//
// FUNCTION     : sem_lock
// DESCRIPTION  : Locks (decrements) the semaphore - P operation
// PARAMETERS   : int semid - semaphore identifier
// RETURNS      : Nothing (exits on error)
//
void sem_lock(int semid) {
    struct sembuf sb;
    sb.sem_num = 0;
    sb.sem_op = -1;  // P operation (wait/lock)
    sb.sem_flg = 0;
    
    if (semop(semid, &sb, 1) == -1) {
        perror("semop lock");
        exit(1);
    }
}

//
// FUNCTION     : sem_unlock
// DESCRIPTION  : Unlocks (increments) the semaphore - V operation
// PARAMETERS   : int semid - semaphore identifier
// RETURNS      : Nothing (exits on error)
//
void sem_unlock(int semid) {
    struct sembuf sb;
    sb.sem_num = 0;
    sb.sem_op = 1;   // V operation (signal/unlock)
    sb.sem_flg = 0;
    
    if (semop(semid, &sb, 1) == -1) {
        perror("semop unlock");
        exit(1);
    }
}

//
// FUNCTION     : create_semaphore
// DESCRIPTION  : Creates a new semaphore and initializes it to 1 (unlocked)
// PARAMETERS   : key_t key - IPC key for semaphore
// RETURNS      : int - semaphore ID or -1 on error
//
int create_semaphore(key_t key) {
    int semid;
    union semun arg;
    
    // Create semaphore
    semid = semget(key, 1, IPC_CREAT | IPC_EXCL | PERMISSIONS);
    if (semid == -1) {
        perror("semget create");
        return -1;
    }
    
    // Initialize semaphore to 1 (unlocked)
    arg.val = 1;
    if (semctl(semid, 0, SETVAL, arg) == -1) {
        perror("semctl SETVAL");
        return -1;
    }
    
    return semid;
}

//
// FUNCTION     : get_semaphore
// DESCRIPTION  : Gets an existing semaphore
// PARAMETERS   : key_t key - IPC key for semaphore
// RETURNS      : int - semaphore ID or -1 on error
//
int get_semaphore(key_t key) {
    int semid;
    
    semid = semget(key, 1, PERMISSIONS);
    if (semid == -1) {
        perror("semget get");
        return -1;
    }
    
    return semid;
}

//
// FUNCTION     : remove_semaphore
// DESCRIPTION  : Removes a semaphore
// PARAMETERS   : int semid - semaphore identifier
// RETURNS      : Nothing
//
void remove_semaphore(int semid) {
    if (semctl(semid, 0, IPC_RMID) == -1) {
        perror("semctl IPC_RMID");
    }
}