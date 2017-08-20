/*
 ============================================================================
 Name        : OSA1.c
 Author      : Robert Sheehan
 Modifier	 : Michael Kemp - mkem114
 Version     : 1.0
 Description : Single thread implementation.
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>


#include "littleThread.h"
#include "threads1.c" // rename this for different threads

Thread newThread; // the thread currently being set up
Thread mainThread; // the main thread
struct sigaction setUpAction;

Thread initial;
Thread current;

/*
 * Switches execution from prevThread to nextThread.
 */
void switcher(Thread prevThread, Thread nextThread) {
	if (prevThread->state == FINISHED) { // it has finished
		printf("\ndisposing %d\n", prevThread->tid);
		free(prevThread->stackAddr); // Wow!
		current = nextThread;
		longjmp(nextThread->environment, 1);
	} else if (setjmp(prevThread->environment) == 0) { // so we can come back here
		prevThread->state = READY;
		nextThread->state = RUNNING;
		printf("scheduling %d\n", nextThread->tid);
		current = nextThread;
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
		switcher(localThread, mainThread); // at the moment back to the main thread
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
    Thread temp = initial;
    do {
        char *state;
        switch (temp->state)
        {
            case READY: state="ready";
            case RUNNING: state="running";
            case FINISHED: state="finished";
            case SETUP: state="setup";
        }
        printf("threadID: %d state:%s\n", temp->tid, state);
        temp = temp->next;
    } while (temp != initial);
}

Thread scheduler() {
    Thread temp = current->next;
    while (temp != current) {
        if (temp->state == READY) {
            return temp;
        }
    }
    return NULL;
}

int main(void) {
	struct thread controller;
	Thread threads[NUMTHREADS];
	mainThread = &controller;
	mainThread->state = RUNNING;
	if (NUMTHREADS > 0) {
		setUpStackTransfer();
		// create the threads
		initial = createThread(threadFuncs[0]);
		threads[0] = initial;
		for (int t = 1; t < NUMTHREADS; t++) {
			threads[t] = createThread(threadFuncs[t]);
		}
		for (int t = 0; t < NUMTHREADS; t++) {
			threads[t]->prev = threads[(t+NUMTHREADS-1)%NUMTHREADS];
			threads[t]->next = threads[(t+1)%NUMTHREADS];
		}
		puts("switching to first thread");
		switcher(mainThread, initial);
		puts("back to the main thread");
	}
	return EXIT_SUCCESS;
}
