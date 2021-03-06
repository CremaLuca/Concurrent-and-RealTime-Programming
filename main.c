#include <pthread.h>
#include <stdio.h>     // printf
#include <stdlib.h>    // exit, strtol, malloc, free
#include <unistd.h>    // close, sleep
#include <arpa/inet.h> // inet_addr

#define BUFFER_SIZE 10
#define N_MESSAGES 1000       // Number of messages produced by the producer
#define PRODUCER_MAX_WAIT 3E8 // Maximum random sleep time for a producer
#define CONSUMER_MAX_WAIT 1E9 // Maximum random sleep time for a consumer

// Debug print colors - https://stackoverflow.com/a/23657072/5764028
#define PRODUCER_C "\x1B[32m"
#define CONSUMER_C "\x1B[34m"
#define MONITOR_C "\x1B[35m"
#define RESET_C "\x1B[0m"

#define FALSE 0
#define TRUE 1

// Inter-process communication (IPC) variables
pthread_mutex_t mutex;
pthread_cond_t can_write, can_read;
int buffer[BUFFER_SIZE];
int w_idx = 0;         // Next free slot in the buffer
int r_idx = 0;         // Next slot to be read by a consumer
char finished = FALSE; // True when the producer has produced N_MESSAGES messages
int produced = 0;      // Number of messages produced
int* consumed;         // Array of integers keeping track of the number of messages consumed by each consumer

/**
 * @brief Stops the current thread for a given amount of seconds.
 *
 * Marked inline to avoid the overhead of a function call.
 *
 * @param s Amount of seconds to wait.
 */
static inline void wait_s(const long s) {
    sleep(s);
}

/**
 * @brief Stops the current thread for a random amount of nanoseconds.
 * The amount of nanoseconds to wait is between 0 and max_ns.
 *
 * Marked inline to avoid the overhead of a function call.
 *
 * @param max_ns Maximum amount of nanoseconds to wait.
 */
static inline void random_wait_ns(const long max_ns) {
    long ns = rand() % max_ns;
    static struct timespec wait_time = {
        .tv_sec = 0
    };
    wait_time.tv_nsec = ns;
    nanosleep(&wait_time, NULL);
}

/**
 * @brief Produces a message and puts it in the buffer as long as
 * there is space. Signals the consumer threads that there is a new
 * message when it produces.
 *
 * @param arg Unused
 */
static void* producer(void* arg) {
    for (int i = 0; i < N_MESSAGES; i++)
    {
        // Simulate a message production
        random_wait_ns(PRODUCER_MAX_WAIT);
        pthread_mutex_lock(&mutex);
        while ((w_idx + 1) % BUFFER_SIZE == r_idx)
        {
            pthread_cond_wait(&can_write, &mutex);
        }
#ifdef DEBUG
        printf(PRODUCER_C "[P ]: + %d\n" RESET_C, i);
#endif
        buffer[w_idx] = i; // Produce a message
        w_idx = (w_idx + 1) % BUFFER_SIZE;
        produced += 1;
        // Tell consumers there is a new message
        pthread_cond_signal(&can_read);
        pthread_mutex_unlock(&mutex);
    }
    // Broadcast all consumers that the producer has finished producing
    pthread_mutex_lock(&mutex);
    finished = TRUE;
#ifdef DEBUG
    printf(PRODUCER_C "[Producer]: finished.\n" RESET_C);
#endif
    pthread_cond_broadcast(&can_read);
    pthread_mutex_unlock(&mutex);
    return NULL;
}

/**
 * @brief Consumes a message from the buffer if there is one, otherwise
 * it waits until there is one or the producer has finished. Signals
 * the producer thread that the buffer is not full when it consumes.
 * The message consumption is simulated by sleeping for a random amount
 * of time.
 *
 * @param arg consumer_id: int
 */
static void* consumer(void* arg) {
    // Argument parsing
    const int consumer_id = *((int*)arg);
    free(arg);

    int consumed_item;
    while (TRUE)
    {
        pthread_mutex_lock(&mutex);
        // Wait for a new message
        while (!finished && r_idx == w_idx)
        {
            pthread_cond_wait(&can_read, &mutex);
        }
        // Check if the producer has finished producing
        if (finished && r_idx == w_idx)
        {
            pthread_mutex_unlock(&mutex);
            break;
        }
        consumed_item = buffer[r_idx];
        r_idx = (r_idx + 1) % BUFFER_SIZE; // Shift the read index
        consumed[consumer_id - 1] += 1;
        pthread_cond_signal(&can_write);
        pthread_mutex_unlock(&mutex);
#ifdef DEBUG
        printf(CONSUMER_C "[C%d]: - %d\n" RESET_C, consumer_id, consumed_item);
#endif
        // Simulate complex operation
        random_wait_ns(CONSUMER_MAX_WAIT);
    }
    return NULL;
}

/**
 * @brief Struct containing the parameters for the monitor thread.
 */
struct monitor_params
{
    int interval;
    int n_consumers;
    struct sockaddr_in server_addr;
};

/**
 * @brief Reads the length of the buffer at a given interval of time
 * and sends it to a monitor server.
 *
 * @param arg monitor_params struct containing the interval, the number of consumers and the server address.
 */
static void* monitor(void* arg) {
    // Arguments parsing
    const struct monitor_params* data = (struct monitor_params*)arg;
    const int interval = data->interval;
    const int n_consumers = data->n_consumers;
    const struct sockaddr_in server_addr = data->server_addr;

    // Open a socket to the monitor serve
    const int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("[Monitor thread]: socket creation failed");
        exit(EXIT_FAILURE);
    }
    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("[Monitor thread]: socket connection failed");
        exit(EXIT_FAILURE);
    }
#ifdef DEBUG
    printf(MONITOR_C "[Monitor thread]: Connected to monitor server\n" RESET_C);
#endif
    // Send the number of consumers to the monitor server
    if (send(sockfd, &n_consumers, sizeof(n_consumers), 0) < 0)
    {
        perror("[Monitor thread]: number of consumers send failed");
        exit(EXIT_FAILURE);
    }
    while (TRUE)
    {
        pthread_mutex_lock(&mutex);
        const int queue_length = (w_idx - r_idx + BUFFER_SIZE) % BUFFER_SIZE;
        // NOTE: Last length 0 will never be notified to the monitor server
        if (finished && r_idx == w_idx)
        {
            pthread_mutex_unlock(&mutex);
            break;
        }
        pthread_mutex_unlock(&mutex);
#ifdef DEBUG
        printf(MONITOR_C "[Monitor thread]: queue: %d, produced: %d,", queue_length, produced);
        for (int i = 0; i < n_consumers; i++)
        {
            printf(" [%d]: %d", i, consumed[i]);
        }
        printf("\n" RESET_C);
#endif
        // Prepare data to send to the monitor server
        int monitor_msg[n_consumers + 2]; // Array of integer messages to send
        monitor_msg[0] = htonl(queue_length);  // First element is the queue length
        monitor_msg[1] = htonl(produced);     // Second element is the number of produced messages so far
        for (int i = 0; i < n_consumers; i++)
        {
            // Third to last elements are the number of consumed messages by each consumer
            monitor_msg[i + 2] = htonl(consumed[i]);
        }
        // Send the message to the monitor server
        if (send(sockfd, &monitor_msg, sizeof(monitor_msg), 0) < 0)
        {
            perror("[Monitor thread]: data send failed");
            exit(EXIT_FAILURE);
        }
        // Wait for the next sample time
        wait_s(interval);
    }
    // Close the socket with the monitor server
    close(sockfd);
    return NULL;
}

int main(int argc, char* args[]) {
    if (argc != 5)
    {
        printf("Usage: %s <# consumers:int> <monitor ip:char*> <monitor port:int> <monitor interval:int [s]>\n", args[0]);
        exit(EXIT_FAILURE);
    }
    // Parse command line arguments
    const int n_consumers = (int)strtol(args[1], NULL, 10);  // Using strtol instead of atoi because of this SO answer https://stackoverflow.com/a/7021750/5764028
    char monitor_ip[16]; // Hostname of the monitor server
    sscanf(args[2], "%15s", monitor_ip);  // Read up to 15 characters (+ terminator) of the monitor ip
    const int monitor_port = (int)strtol(args[3], NULL, 10);

    // Initialize mutex and condition variables
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&can_write, NULL);
    pthread_cond_init(&can_read, NULL);

    // Create producer thread
    pthread_t threads[n_consumers + 2];
    printf("[Main]: Starting producer\n");
    pthread_create(&threads[0], NULL, producer, NULL); // Producer

    // Allocate memory for consumed items counters
    consumed = malloc(sizeof(int) * n_consumers);
    if (consumed == NULL)
    {
        perror("[Main]: consumed malloc failed");
        exit(EXIT_FAILURE);
    }

    // Create consumer threads
    printf("[Main]: Creating %d consumer threads\n", n_consumers);
    for (int i = 1; i < n_consumers + 1; i++)
    {
        printf("[Main]: Starting consumer %d\n", i);
        int* id = malloc(sizeof(*id));
        if (id == NULL)
        {
            perror("[Main]: id malloc failed");
            exit(EXIT_FAILURE);
        }

        // Passing value to allocated memory for id
        *id = i;
        pthread_create(&threads[i], NULL, consumer, id); // Consumer
    }

    // Gather required monitor parameters
    struct monitor_params m_params = {
        interval: strtol(args[4], NULL, 10),
        n_consumers : n_consumers,
        server_addr : {
            sin_family: AF_INET,
            sin_port : htons(monitor_port),
            sin_addr : {
                s_addr: inet_addr(monitor_ip)
            },
        },
    };
    printf("[Main]: Starting monitor\n");
    pthread_create(&threads[n_consumers + 1], NULL, monitor, &m_params);

    // Wait for all threads to finish
    for (int i = 0; i < n_consumers + 1; i++)
    {
        pthread_join(threads[i], NULL);
    }
    free(consumed);
}