# Concurrent and RealTime Programming

Code for the Concurrent and Realtime Programming UNIPD Course's oral exam.

## Exercise

Producer-(multiple) consumers program with remote status monitoring. An actor (thread or process), separate from the producer and the consumers shall periodically monitor the message queue length, the number of produced messages and the number of received messages for every consumer. The collected information shall be sent via TCP/IP to a server that shall print the received information.

## Usage

### Compile

To compile the source code, use the following command:

```bash
make all
```

If you want the extra debug information use

```bash
make debug
```

### Run

NOTE: Suggested number of consumers is 3 so that the queue fill level varies meaningfully.

#### Monitor server

First, you have to run the monitor server.

```bash
./monitor_server <monitor port>
```

#### Client

Then, you have to run the client with producers, consumers and the monitor thread.

```bash
./main <n. consumers> <monitor ip> <monitor port> <monitor interval [s]>
```

## Extra

Check which functions get inlined by the compiler.

```bash
gcc -O3 -fopt-info-inline-optimized-missed=missed.txt main.c -o main -lpthread
```
