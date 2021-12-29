#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>

#define BUFFER_SIZE 10
#define N_MESSAGES 1000 // Number of messages produced by the producer

// Inter-process communication (IPC) variables
pthread_mutex_t mutex;
pthread_cond_t canWrite, canRead;
int buffer[BUFFER_SIZE];
int writeIdx = 0;  // Next free slot in the buffer
int readIdx = 0;   // Next slot to be read by a consumer
char finished = 0; // True when the producer has produced all messages

/**
 * @brief Stops the current thread for a given amount of nanoseconds.
 * 
 * @param ns Amount of nanoseconds to wait.
 */
static void wait_ns(long ns)
{
    static struct timespec waitTime;
    waitTime.tv_sec = 0;
    waitTime.tv_nsec = ns;
    nanosleep(&waitTime, NULL);
}

/**
 * @brief Stops the current thread for a random amount of nanoseconds.
 * The amount of nanoseconds to wait is between 0 and max_ns.
 * 
 * @param max_ns Maximum amount of nanoseconds to wait.
 */
static void random_wait_ns(long max_ns)
{
    static struct timespec waitTime;
    long ns = rand() % max_ns;
    waitTime.tv_sec = 0;
    waitTime.tv_nsec = ns;
    nanosleep(&waitTime, NULL);
}

/**
 * @brief Produces a message and puts it in the buffer as long as
 * there is space. Signals the consumer threads that there is a new
 * message when it produces.
 * 
 * @param arg Unused
 */
static void *producer(void *arg)
{
    for (int i = 0; i < N_MESSAGES; i++)
    {
        // Simulate a message production
        random_wait_ns(3E8);
        pthread_mutex_lock(&mutex);
        while ((writeIdx + 1) % BUFFER_SIZE == readIdx)
        {
            pthread_cond_wait(&canWrite, &mutex);
        }
        printf("+ %d\n", i);
        buffer[writeIdx] = i; // Produce a message
        writeIdx = (writeIdx + 1) % BUFFER_SIZE;
        // Tell consumers there is a new message
        pthread_cond_signal(&canRead);
        pthread_mutex_unlock(&mutex);
    }
    // Broadcast all consumers that the producer has finished producing
    pthread_mutex_lock(&mutex);
    finished = 1;
    printf("fin\n");
    pthread_cond_broadcast(&canRead);
    pthread_mutex_unlock(&mutex);
    return NULL;
}
/**
 * @brief 
 * 
 * @param arg Unused
 */
static void *consumer(void *arg)
{
    int item;
    while (1)
    {
        pthread_mutex_lock(&mutex);
        // Wait for a new message
        while (!finished && readIdx == writeIdx)
        {
            pthread_cond_wait(&canRead, &mutex);
        }
        // Check if the producer has finished producing
        if (finished && readIdx == writeIdx)
        {
            pthread_mutex_unlock(&mutex);
            return NULL;
        }
        item = buffer[readIdx];
        readIdx = (readIdx + 1) % BUFFER_SIZE; // Shift forward the read index
        pthread_cond_signal(&canWrite);
        pthread_mutex_unlock(&mutex);
        // Complex operation
        printf("- %d\n", item);
        random_wait_ns(1E9);
    }
    return NULL; // Unreachable
}

/**
 * @brief Reads the length of the buffer at a given interval of time
 * and sends it to a monitor server.
 * 
 * @param arg Pointer to an integer that contains the interval of time in us.
 */
static void *monitor(void *arg){
    int interval = *((int *)arg);
    int length;
    while(1){
        pthread_mutex_lock(&mutex);
        length = (writeIdx - readIdx + BUFFER_SIZE) % BUFFER_SIZE;
        // NOTE: Last length 0 will never be notified to the monitor server
        if (finished && readIdx == writeIdx)
        {
            pthread_mutex_unlock(&mutex);
            return NULL;
        }
        pthread_mutex_unlock(&mutex);
        printf("length: %d\n", length);
        wait_ns(interval);
    }
    return NULL;
}

int main(int argc, char *args[])
{
    // Variables declaration
    int nConsumers; // Number of consumer threads

    // Arguments parsing
    if (argc != 3)
    {
        printf("Usage: main <# consumers> <monitor interval [us]>\n");
        exit(0);
    }
    // Read the number of consumers from the first argument
    sscanf(args[1], "%d", &nConsumers);
    // Initialize mutex and condition variables
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&canWrite, NULL);
    pthread_cond_init(&canRead, NULL);
    // Create producer and consumers threads
    pthread_t threads[nConsumers + 2];
    printf("Starting producer and %d consumers\n", nConsumers);
    pthread_create(&threads[0], NULL, producer, NULL); // Producer
    for (int i = 1; i < nConsumers + 1; i++)
    {
        printf("Starting consumer %d\n", i);
        pthread_create(&threads[i], NULL, consumer, NULL); // Consumer
    }
    // Create monitor thread
    int interval = atoi(args[2]);
    printf("Starting monitor\n");
    pthread_create(&threads[nConsumers + 1], NULL, monitor, &interval);
    
    // Wait for all threads to finish
    for (int i = 0; i < nConsumers + 1; i++)
    {
        pthread_join(threads[i], NULL);
    }
}