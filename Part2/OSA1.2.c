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
#include <unistd.h>

#include "littleThread.h"
#include "threads2.c" // rename this for different threads

Thread newThread; // the thread currently being set up
Thread mainThread; // the main thread
struct sigaction setUpAction;

Thread *thread3;

void switcher(Thread prevThread, Thread nextThread);
void scheduler(Thread old);
void printThreadStates();

/*
 * Switches execution from prevThread to nextThread.
 */
void switcher(Thread prevThread, Thread nextThread) {
	if (prevThread->state == FINISHED) { // it has finished
		printf("\ndisposing %d\n\n", prevThread->tid);

		if (prevThread->stackAddr != NULL) {
			free(prevThread->stackAddr); // Wow!
			prevThread->stackAddr = NULL;
		}

		Thread temp1 = prevThread->prev;
		Thread temp2 = prevThread->next;
		temp1->next = temp2;
		temp2->prev = temp1;
		nextThread->state = RUNNING;

		if (nextThread != mainThread) {
			printThreadStates();
		}

		longjmp(nextThread->environment, 1);
	} else if (setjmp(prevThread->environment) == 0) { // so we can come back here
		prevThread->state = READY;
		nextThread->state = RUNNING;

		printThreadStates();
		printf("scheduling %d\n\n", nextThread->tid);
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

void printThreadStates() {
	printf("Thread States\n");
	printf("=============\n");
	for (int i = 0; i < NUMTHREADS; i++) {
		char *state;
		switch (thread3[i]->state)
		{
			case READY:
				state="ready";
				break;
			case RUNNING:
				state="running";
				break;
			case FINISHED:
				state="finished";
				break;
			case SETUP:
				state="setup";
				break;
		}
		printf("threadID: %d state:%s\n", thread3[i]->tid, state);
	}
	printf("\n");
}

void scheduler(Thread old){
	static Thread marker = NULL;
	Thread nxt;

	if (marker == NULL) {
		marker = nxt;
	} else if (old == NULL) {
		old = marker;
	}
	nxt = old->next;

	while (nxt->state != READY && nxt != nxt->next) {
		nxt = nxt->next;
	}
	marker = nxt;

    if (nxt->next == nxt) {
        nxt = mainThread;
	}
	switcher(old,nxt);
}

void threadYield() {
    scheduler(NULL);
}

int main(void) {
	struct thread controller;
	mainThread = &controller;
	mainThread->state = RUNNING;
	thread3 = malloc(NUMTHREADS*sizeof(Thread));
	if (NUMTHREADS > 0) {
		setUpStackTransfer();
		// create the threads
		for (int t = 0; t < NUMTHREADS; t++) {
			thread3[t] = createThread(threadFuncs[t]);
		}
		for (int t = 0; t < NUMTHREADS; t++) {
			thread3[t]->prev = thread3[(t+NUMTHREADS-1)%NUMTHREADS];
			thread3[t]->next = thread3[(t+1)%NUMTHREADS];
		}
		mainThread->next=thread3[0];
		printThreadStates();
		puts("switching to first thread\n");
		scheduler(mainThread);
		puts("back to the main thread\n");
		printThreadStates();
	}
	return EXIT_SUCCESS;
}





















