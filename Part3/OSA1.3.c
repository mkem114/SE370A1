/*
 ============================================================================
 Name        : OSA1.c
 Author      : Robert Sheehan
 Modifier	 : Michael Kemp - mkem114 - 6273632
 Version     : 1.0
 Description : Single thread implementation.
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>

#include "littleThread.h"
#include "threads3.c" // rename this for different threads

Thread newThread; // the thread currently being set up
Thread mainThread; // the main thread
struct sigaction setUpAction;

//Global pointer to the initial array of Threads
Thread *thread3;

//Prototype statements
void switcher(Thread prevThread, Thread nextThread);

void scheduler(Thread old);

void printThreadStates();

//http://www.informit.com/articles/article.aspx?p=23618&seqNum=14
//Timer handler, forces current thread to yield
void timesUp(int signum) {
	//Yielding for the current thread
	threadYield();
}

/*
 * Switches execution from prevThread to nextThread.
 */
void switcher(Thread prevThread, Thread nextThread) {
	if (prevThread->state == FINISHED) { // it has finished
		printf("\ndisposing %d\n\n", prevThread->tid);

		//Only frees the thread's stack if it's not already
		if (prevThread->stackAddr != NULL) {
			free(prevThread->stackAddr); // Wow!
			prevThread->stackAddr = NULL;
		}

		//Removes the finished thread from the linked list
		Thread temp1 = prevThread->prev;
		Thread temp2 = prevThread->next;
		temp1->next = temp2;
		temp2->prev = temp1;
		nextThread->state = RUNNING;

		//Print the switching if not going back to the main function
		if (nextThread != mainThread) {
			printThreadStates();
		}

		longjmp(nextThread->environment, 1);
	} else if (setjmp(prevThread->environment) == 0) { // so we can come back here
		//Updates states of old threads and new threads
		prevThread->state = READY;
		nextThread->state = RUNNING;

		printThreadStates();
		//printf("scheduling %d\n\n", nextThread->tid);
		longjmp(nextThread->environment, 1);
	}
}

/*
 * Associates the signal stack with the newThread.
 * Also sets up the newThread to start running after it is long jumped to.
 * This is called when SIGUSR1 is received.
 */
void associateStack(int signum) {
	Thread localThread = newThread; // what if we don't use this local variable?
	localThread->state = READY; // now it has its stack
	if (setjmp(localThread->environment) != 0) { // will be zero if called directly
		(localThread->start)();
		localThread->state = FINISHED;
		scheduler(localThread); // at the moment back to the main thread
	}
}

/*
 * Sets up the user signal handler so that when SIGUSR1 is received
 * it will use a separate stack. This stack is then associated with
 * the newThread when the signal handler associateStack is executed.
 */
void setUpStackTransfer() {
	setUpAction.sa_handler = (void *) associateStack;
	setUpAction.sa_flags = SA_ONSTACK;
	sigaction(SIGUSR1, &setUpAction, NULL);
}

/*
 *  Sets up the new thread.
 *  The startFunc is the function called when the thread starts running.
 *  It also allocates space for the thread's stack.
 *  This stack will be the stack used by the SIGUSR1 signal handler.
 */
Thread createThread(void (startFunc)()) {
	static int nextTID = 0;
	Thread thread;
	stack_t threadStack;

	if ((thread = malloc(sizeof(struct thread))) == NULL) {
		perror("allocating thread");
		exit(EXIT_FAILURE);
	}
	thread->tid = nextTID++;
	thread->state = SETUP;
	thread->start = startFunc;
	if ((threadStack.ss_sp = malloc(SIGSTKSZ)) == NULL) { // space for the stack
		perror("allocating stack");
		exit(EXIT_FAILURE);
	}
	thread->stackAddr = threadStack.ss_sp;
	threadStack.ss_size = SIGSTKSZ; // the size of the stack
	threadStack.ss_flags = 0;
	if (sigaltstack(&threadStack, NULL) < 0) { // signal handled on threadStack
		perror("sigaltstack");
		exit(EXIT_FAILURE);
	}
	newThread = thread; // So that the signal handler can find this thread
	kill(getpid(), SIGUSR1); // Send the signal. After this everything is set.
	return thread;
}

//Prints the state of all threads
void printThreadStates() {
	//Print header
	printf("Thread States\n");
	printf("=============\n");
	//For every thread
	for (int i = 0; i < NUMTHREADS; i++) {
		//Represent the current state as a string
		char *state;
		switch (thread3[i]->state) {
			case READY:
				state = "ready";
				break;
			case RUNNING:
				state = "running";
				break;
			case FINISHED:
				state = "finished";
				break;
			case SETUP:
				state = "setup";
				break;
		}
		//Print to the screen
		printf("threadID: %d state:%s\n", thread3[i]->tid, state);
	}
	printf("\n");
}

//Determine the next thread to run
void scheduler(Thread old) {
	static Thread marker = NULL;
	Thread nxt;

	//A startup thang
	if (marker == NULL) {
		marker = nxt;
	}
	//If we don't know the currently executing thread then assume
	if (old == NULL) {
		old = marker;
	}
	nxt = old->next;

	//Until we have a thread to run next that's in the ready state load the next one
	while (nxt->state != READY && nxt != nxt->next) {
		nxt = nxt->next;
	}
	marker = nxt;

	//If there's only one thread left
	if (nxt->next == nxt) {
		//Go back to main
		nxt = mainThread;
		//Make sure there's no threads left that are ready
		for (int i = 0; i < NUMTHREADS; i++) {
			//If there is then run them
			if (thread3[i]->state != FINISHED) {
				nxt = thread3[i];
			}
		}
	}

	//Switch to the next thread
	switcher(old, nxt);
}

//Pause the current thread
void threadYield() {
	scheduler(NULL);
}

int main(void) {
	struct thread controller;
	mainThread = &controller;
	mainThread->state = RUNNING;
	thread3 = malloc(NUMTHREADS * sizeof(Thread));
	if (NUMTHREADS > 0) {
		setUpStackTransfer();
		// create the threads
		for (int t = 0; t < NUMTHREADS; t++) {
			thread3[t] = createThread(threadFuncs[t]);
		}
		// Link the list together
		for (int t = 0; t < NUMTHREADS; t++) {
			thread3[t]->prev = thread3[(t + NUMTHREADS - 1) % NUMTHREADS];
			thread3[t]->next = thread3[(t + 1) % NUMTHREADS];
		}

		//The following resource taught me how to run a timer every 20ms (=20000ns)
		//http://www.informit.com/articles/article.aspx?p=23618&seqNum=14
		struct sigaction sa;
		struct itimerval timer;
		//http://www.informit.com/articles/article.aspx?p=23618&seqNum=14
		memset(&sa, 0, sizeof(sa));
		sa.sa_handler = &timesUp; //Put function name in handler
		sigaction(SIGVTALRM, &sa, NULL); //Assign handler to signal
		//http://www.informit.com/articles/article.aspx?p=23618&seqNum=14
		timer.it_value.tv_sec = 0; //First 0
		timer.it_value.tv_usec = 20000; //First 20
		timer.it_interval.tv_sec = 0; //Every 0
		timer.it_interval.tv_usec = 20000; //to 20ms after that
		setitimer(ITIMER_VIRTUAL, &timer, NULL); // Start timer

		//Set next of main thread to first created one
		mainThread->next = thread3[0];
		printThreadStates();
		puts("switching to first thread\n");
		//Bombs away!
		scheduler(mainThread);
		puts("back to the main thread\n");
		printThreadStates();
	}
	return EXIT_SUCCESS;
}