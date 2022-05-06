/* Project-2
 * Weijie Zheng
 */
#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

int count = 0;
int front = 0;      // the front of FIFO
int rear = 0;       // the rear of FIFO
int FIFOcount = 0;  // couunt the PCB's in FIFO
int processNum = 0; // count the number of processes
pthread_mutex_t lock;

// PCB struct
typedef struct
{
    char func[10]; // process name;
    int prio;      // priority
    int arriveTime;
    int burstTime;
    int input_1;
    int input_2;
    int result;
} MiniPCB;

// global variable for sharing between threads
MiniPCB *FIFOmsq[5];

// to pass multiple argument in pthread_create
typedef struct
{
    int algorithm;
    MiniPCB *readyQueue;
    int (*comp[4])(int i, int j);
    int f2;
} param;

// work functions
int sum(int i, int j)
{
    int temp = i;
    while (i < j)
    {
        temp += ++i;
    }
    return temp;
}

int product(int i, int j)
{
    return i * j;
}

int power(int i, int j)
{
    int temp = 1;
    for (int k = 0; k < j; k++)
    {
        temp *= i;
    }
    return temp;
}

int fibonacci(int i, int j)
{
    int f1 = 0;
    int f2 = 1;
    // add from the 3rd num
    for (int k = 3; k <= j; k++)
    {
        i = f1 + f2;
        f1 = f2;
        f2 = i;
    }
    return i;
}

// insertionsort by the next burst time
void SJFsort(MiniPCB readQueue[])
{
    for (int i = 1; i < processNum; i++)
    {
        MiniPCB temp = readQueue[i];
        int position = i;
        while (position > 0 && readQueue[position - 1].burstTime > temp.burstTime)
        {
            readQueue[position] = readQueue[position - 1];
            position--;
        }
        readQueue[position] = temp;
    }
}

//// insertionsort by the priority
void PRIORITYsort(MiniPCB readQueue[])
{
    for (int i = 1; i < processNum; i++)
    {
        MiniPCB temp = readQueue[i];
        int position = i;
        while (position > 0 && readQueue[position - 1].prio > temp.prio)
        {
            readQueue[position] = readQueue[position - 1];
            position--;
        }
        readQueue[position] = temp;
    }
}

// choose algorithm
int chooseAlgorithm(char *scheduling)
{
    if (strcmp(scheduling, "FCFS") == 0)
    {
        return 0;
    }
    else if (strcmp(scheduling, "SJF") == 0)
    {
        return 1;
    }
    else if (strcmp(scheduling, "PRIORITY") == 0)
    {
        return 2;
    }
    else
    {
        return -1;
    }
}

// scheduler
// because all PCB have been in the ReadyQueue, just sort them
MiniPCB *FCFS(MiniPCB readyQueue[])
{
    // index == arriveTime
    return &readyQueue[count++];
}
MiniPCB *SJF(MiniPCB readyQueue[])
{
    SJFsort(readyQueue);
    return &readyQueue[count++];
}
MiniPCB *PRIORITY(MiniPCB readyQueue[])
{
    PRIORITYsort(readyQueue);
    return &readyQueue[count++];
}

// setup function pointer array for scheduler
MiniPCB *(*schedule[3])(MiniPCB p[]) = {FCFS, SJF, PRIORITY};

// dispatch
void dispatch(MiniPCB *process, int (*comp[])(int, int))
{
    if (strcmp((*process).func, "sum") == 0)
    {
        (*process).result = comp[0]((*process).input_1, (*process).input_2);
    }
    else if (strcmp((*process).func, "product") == 0)
    {
        (*process).result = comp[1]((*process).input_1, (*process).input_2);
    }
    else if (strcmp((*process).func, "power") == 0)
    {
        (*process).result = comp[2]((*process).input_1, (*process).input_2);
    }
    else
    { // process.func, "fibonacci") == 0
        (*process).result = comp[3]((*process).input_1, (*process).input_2);
    }

    return;
}

// scheduler- dispatcher send message
void send(MiniPCB *process)
{
    while (FIFOcount == 5)
    {
        // do nothing when FIFO is full
    }
    pthread_mutex_lock(&lock);
    // critical section
    // enQueue
    FIFOmsq[rear] = process;
    rear = (rear + 1) % 5;
    FIFOcount++;

    pthread_mutex_unlock(&lock);
}
void recev(int f2)
{
    int str;
    char buf[50];
    while (FIFOcount == 0)
    {
        // do nothing when FIFO is empty
    }
    pthread_mutex_lock(&lock);

    // critical section
    MiniPCB *proc = FIFOmsq[front];
    str = sprintf(buf, "%s,%d,%d,%d\n", (*proc).func, (*proc).input_1, (*proc).input_2, (*proc).result);
    write(f2, buf, str);
    // deQueue
    front = (front + 1) % 5;
    FIFOcount--;

    pthread_mutex_unlock(&lock);
}

void *Scheduler_Dispatcher(void *arg)
{
    param *temp = (param *)arg;
    /* setup function pointer array for work */
    int (*comp[4])(int i, int j) = {sum, product, power, fibonacci};
    for (int i = 0; i < processNum; i++)
    {
        MiniPCB *proc = schedule[temp->algorithm](temp->readyQueue);
        dispatch(proc, comp);
        // send the process
        send(proc);
    }
    pthread_exit(0);
}
void *Logger(void *input)
{
    int f2 = *((int *)input);
    for (int i = 0; i < processNum; i++)
    {

        recev(f2);
    }
    pthread_exit(0);
}

int main(int argc, const char *argv[])
{
    /**
     * open source file and create target file
     */
    FILE *f1;
    int f2;
    // Do we have right number of arguments?
    if (argc != 4)
    {
        printf("Wrong number of command line arguments\n");
        return 1;
    }
    // Can we access thge source file?
    if (!(f1 = fopen(argv[2], "r")))
    {
        printf("Can't open %s \n", argv[1]);
        return 2;
    }
    // Can we create the target file?
    if ((f2 = creat(argv[3], 0644)) == -1)
    {
        printf("Can't create %s \n", argv[2]);
        return 3;
    }

    MiniPCB readyQueue[15];

    pthread_mutex_init(&lock, NULL);

    // count the number of the processes
    int flag = 0;
    while (!feof(f1))
    {
        flag = fgetc(f1);
        if (flag == '\n')
        {
            processNum++;
        }
    }
    processNum++;
    rewind(f1); // repoint to the beginning of the file

    // read input data into data struct
    int algorithm = chooseAlgorithm((char *)argv[1]);
    switch (algorithm)
    {
    case 0: // FCFS
        for (int i = 0; i < processNum; i++)
        {
            fscanf(f1, "%d, %[^,], %d, %d\n", &readyQueue[i].arriveTime, readyQueue[i].func, &readyQueue[i].input_1, &readyQueue[i].input_2);
        }
        break;
    case 1: // SJF
        for (int i = 0; i < processNum; i++)
        {
            fscanf(f1, "%d, %d, %[^,], %d, %d\n", &readyQueue[i].arriveTime, &readyQueue[i].burstTime, readyQueue[i].func, &readyQueue[i].input_1, &readyQueue[i].input_2);
        }
        break;
    case 2: // PRIORITY
        for (int i = 0; i < processNum; i++)
        {
            fscanf(f1, "%d, %d, %[^,], %d, %d\n", &readyQueue[i].arriveTime, &readyQueue[i].prio, readyQueue[i].func, &readyQueue[i].input_1, &readyQueue[i].input_2);
        }
        break;
    default:
        perror("wrong algorithm input\n");
        return -1;
    }
    fclose(f1);

    /**
     * pthread part
     */
    // to pass multiple argument in pthread_create
    // new struct for storing parameter
    param arg;
    arg.algorithm = algorithm;
    arg.readyQueue = readyQueue;
    arg.f2 = f2;
    // arg.comp[4] = comp[4];

    // the thread identifier
    pthread_t tids[2];
    // creat the thread
    int ret = pthread_create(&tids[0], NULL, Scheduler_Dispatcher, (void *)&arg);
    if (ret != 0)
    {
        perror("pthread_create error");
        return 4;
    }
    ret = pthread_create(&tids[1], NULL, Logger, (void *)&f2);
    if (ret != 0)
    {
        perror("pthread_create error");
        return 4;
    }
    // wait for the thread to exit
    pthread_join(tids[0], NULL);
    pthread_join(tids[1], NULL);

    // destroy the mutex
    pthread_mutex_destroy(&lock);
    close(f2);
    printf("work done\n");
    return 0;
}
