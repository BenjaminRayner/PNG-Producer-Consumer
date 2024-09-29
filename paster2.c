#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <pthread.h>
#include <string.h>

#include "catpng.c"

#define STRIPCOUNT 50
#define HEIGHT 6
#define WIDTH 400
#define INFLATEDSIZE HEIGHT*(WIDTH*4+1)

/* Will share strips and status inform across processes. */
typedef struct {
    strip_data stripBuffer[STRIPCOUNT];
    unsigned char inflatedData[INFLATEDSIZE*STRIPCOUNT];
    int pindex;
    int cindex;
    int p_currentStrip;
    int c_currentStrip;
    sem_t spaces;
    sem_t items;
    pthread_mutex_t p_mutex;
    pthread_mutex_t c_mutex;
    pthread_mutex_t buf_mutex;
} shared_data;

/* Address for shared data segment. */
shared_data* global;

int BUFFER_SIZE;
int consumerSleep;
char URL[53] = "http://ece252-1.uwaterloo.ca:2530/image?img=M&part=N";

void producer() {
    while (1) {
        /* Each producer grabs different strip number. */
        pthread_mutex_lock(&global->p_mutex);
        int stripNum = ++global->p_currentStrip;
        pthread_mutex_unlock(&global->p_mutex);

        /* If all strips are taken care of, break and kill. */
        if (stripNum >= STRIPCOUNT) {
            break;
        }

        /* Retrieve a strip according to stripNum. */
        sprintf(&URL[51], "%i", stripNum);
        strip_data strip;
        strip = requestStrip(URL);
        strip.stripNum = stripNum;

        /* Wait until buffer has room, then add to buffer. */
        sem_wait(&global->spaces);
        pthread_mutex_lock(&global->buf_mutex);
        memcpy(&global->stripBuffer[global->pindex], &strip, sizeof(strip_data));
        global->pindex = (global->pindex + 1) % BUFFER_SIZE;
        pthread_mutex_unlock(&global->buf_mutex);
        sem_post(&global->items);
    }
}

void consumer() {
    while (1) {
        usleep(consumerSleep * 1000);

        /* Each consumer attacks different strip. */
        pthread_mutex_lock(&global->c_mutex);
        int nextStrip = ++global->c_currentStrip;
        pthread_mutex_unlock(&global->c_mutex);

        /* If all strips are taken care of, break and kill. */
        if (nextStrip >= STRIPCOUNT) {
            break;
        }
        strip_data stripTemp;

        /* Wait until buffer has items, then remove from buffer. */
        sem_wait(&global->items);
        pthread_mutex_lock(&global->buf_mutex);
        stripTemp = global->stripBuffer[global->cindex];
        global->cindex = (global->cindex + 1) % BUFFER_SIZE;
        pthread_mutex_unlock(&global->buf_mutex);
        sem_post(&global->spaces);

        mem_inf(&global->inflatedData[INFLATEDSIZE*stripTemp.stripNum], NULL, stripTemp.strip, stripTemp.size);
    }
}

int main(int argc, char* argv[]) {

    BUFFER_SIZE = atoi(argv[1]);        /* Number of image strips in shared memory. */
    int numProducers = atoi(argv[2]);   /* Number of producers that will be created. */
    int numConsumers = atoi(argv[3]);   /* Number of consumers that will be created. */
    consumerSleep = atoi(argv[4]);      /* Time consumer must sleep before reading from buffer. */
    int imgNumber = atoi(argv[5]);      /* Number that will be retrieved from server. */

    pid_t pid;                                                                                           /* Process ID of the fork. */
    pid_t producerPID[numProducers];                                                                     /* Stores an array of producer fork Process IDs. */
    pid_t consumerPID[numConsumers];                                                                     /* Stores an array of consumer fork Process IDs. */
    int shmid = shmget(IPC_PRIVATE, sizeof(shared_data), IPC_CREAT | 0666);                       /* ID of created shared memory segment of size "shared_data" */
    snprintf(URL, 53, "http://ece252-1.uwaterloo.ca:2530/image?img=%i&part=N", imgNumber); /* Change URL to point to correct image. */

    /* ----- Initialize our shared memory segment with our shared_data struct. ----- */
    global = (shared_data*) shmat(shmid, NULL, 0);
    global->pindex = 0;
    global->cindex = 0;
    global->p_currentStrip = -1;
    global->c_currentStrip = -1;
    sem_init(&global->spaces, 1, BUFFER_SIZE);
    sem_init(&global->items, 1, 0);

    pthread_mutexattr_t attrmutex;
    pthread_mutexattr_init(&attrmutex);
    pthread_mutexattr_setpshared(&attrmutex, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&global->c_mutex, &attrmutex);
    pthread_mutex_init(&global->p_mutex, &attrmutex);
    pthread_mutex_init(&global->buf_mutex, &attrmutex);
    /* ----------------------------------------------------------------------------- */

    /* Spawn producer processes and send them to the producer function. */
    for (int i = 0; i < numProducers; ++i) {
        pid = fork();
        if (pid > 0) {
            producerPID[i] = pid;
        }
        else if (pid == 0) {
            producer();
            return 0;
        }
        else {
            perror("fork");
            abort();
        }
    }

    /* Spawn consumer processes and send them to the consumer function. */
    for (int i = 0; i < numConsumers; ++i) {
        pid = fork();
        if (pid > 0) {
            consumerPID[i] = pid;
        }
        else if (pid == 0) {
            consumer();
            return 0;
        }
        else {
            perror("fork");
            abort();
        }
    }

    /* Wait for the processes to finish. */
    for (int i = 0; i < numProducers; ++i) {
        waitpid(producerPID[i], NULL, 0);
    }
    for (int i = 0; i < numConsumers; ++i) {
        waitpid(consumerPID[i], NULL, 0);
    }

    catpng(global->inflatedData);

    /* Cleanup. */
    sem_destroy(&global->items);
    sem_destroy(&global->spaces);
    pthread_mutex_destroy(&global->c_mutex);
    pthread_mutex_destroy(&global->p_mutex);
    pthread_mutex_destroy(&global->buf_mutex);
    pthread_mutexattr_destroy(&attrmutex);
    shmdt(global);
    shmctl(shmid, IPC_RMID, NULL);

    return 0;
}