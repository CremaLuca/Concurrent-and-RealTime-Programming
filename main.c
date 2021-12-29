#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define BUFFER_SIZE 10
#define N_MESSAGES 1000 // Number of messages produced by the producer

// TODO: Get these parameters from the command line
#define PORT 8080
#define MAXLINE 1024

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
 * @brief Stops the current thread for a given amount of seconds.
 * 
 * @param s Amount of seconds to wait.
 */
static void wait_s(long s)
{
    sleep(s);
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
#ifdef DEBUG
        printf("+ %d\n", i);
#endif
        buffer[writeIdx] = i; // Produce a message
        writeIdx = (writeIdx + 1) % BUFFER_SIZE;
        // Tell consumers there is a new message
        pthread_cond_signal(&canRead);
        pthread_mutex_unlock(&mutex);
    }
    // Broadcast all consumers that the producer has finished producing
    pthread_mutex_lock(&mutex);
    finished = 1;
#ifdef DEBUG
    printf("[Producer]: finished.\n");
#endif
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
            break;
        }
        item = buffer[readIdx];
        readIdx = (readIdx + 1) % BUFFER_SIZE; // Shift forward the read index
        pthread_cond_signal(&canWrite);
        pthread_mutex_unlock(&mutex);
// Complex operation
#ifdef DEBUG
        printf("- %d\n", item);
#endif
        random_wait_ns(1E9);
    }
    return NULL;
}

struct monitor_data
{
    int interval;
    struct sockaddr_in server_addr;
};

/**
 * @brief Reads the length of the buffer at a given interval of time
 * and sends it to a monitor server.
 * 
 * @param arg Pointer to an integer that contains the interval of time in us.
 */
static void *monitor(void *arg)
{
    // Parse thread arguments
    struct monitor_data *data = (struct monitor_data *)arg;
    int interval = data->interval;
    // Open a socket to the monitor server
    int sockfd;
    struct sockaddr_in server_addr = data->server_addr;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("[Monitor thread]: socket creation failed");
        exit(EXIT_FAILURE);
    }
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        perror("[Monitor thread]: socket connection failed");
        exit(EXIT_FAILURE);
    }
#ifdef DEBUG
    printf("[Monitor thread]: Connected to monitor server\n");
#endif
    int length;
    unsigned int netLength;
    while (1)
    {
        pthread_mutex_lock(&mutex);
        length = (writeIdx - readIdx + BUFFER_SIZE) % BUFFER_SIZE;
        // NOTE: Last length 0 will never be notified to the monitor server
        if (finished && readIdx == writeIdx)
        {
            pthread_mutex_unlock(&mutex);
            break;
        }
        pthread_mutex_unlock(&mutex);
#ifdef DEBUG
        printf("[Monitor thread]: read length: %d\n", length);
#endif
        // Convert the integer number into network byte order
        netLength = htonl(length);
        // Actually send the length value
        if (send(sockfd, &netLength, sizeof(netLength), 0) < 0)
        {
            perror("[Monitor thread]: send failed");
            exit(EXIT_FAILURE);
        }
        // Wait for the next sample time
        wait_s(interval);
    }
    close(sockfd);
    return NULL;
}

int main(int argc, char *args[])
{
    // Variables declaration
    int nConsumers;             // Number of consumer threads
    char monitor_ip[16]; // Hostname of the monitor server
    int monitor_port;           // Port of the monitor server
    if (argc != 5)
    {
        printf("Usage: %s <# consumers> <monitor ip> <monitor port> <monitor interval [s]>\n", args[0]);
        exit(EXIT_FAILURE);
    }
    // Parse arguments
    nConsumers = strtol(args[1], NULL, 10);
    sscanf(args[2], "%s", monitor_ip);
    monitor_port = strtol(args[3], NULL, 10);

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
    // Initialize monitor data
    struct monitor_data mdata = {
        interval : strtol(args[4], NULL, 10),
        server_addr : {
            sin_family : AF_INET,
            sin_port : htons(monitor_port),
            sin_addr : {
                s_addr : inet_addr(monitor_ip)
            },
        },
    };
    printf("Starting monitor\n");
    pthread_create(&threads[nConsumers + 1], NULL, monitor, &mdata);

    // Wait for all threads to finish
    for (int i = 0; i < nConsumers + 1; i++)
    {
        pthread_join(threads[i], NULL);
    }
}