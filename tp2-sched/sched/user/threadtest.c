// Test for user-space threads.
//
// Goal: create at least NTHREADS threads with thread_create() that share the
// shared_counter global variable and print their progress, showing that they
// all see the same memory (unlike what would happen with fork).

#include <inc/lib.h>

#define NTHREADS 3
#define NITER 5

// Variable shared by every thread in the process (lives in the BSS, which is
// part of the shared address space).
static volatile int shared_counter = 0;

// Function each thread runs.
//
// Hints:
//   - Print the value of shared_counter before and after modifying it, to
//     show that one thread's changes are visible to the others.
//   - Yield the CPU with sys_yield() inside the loop to interleave the
//     threads' execution.
//   - To identify itself, use sys_getenvid(): the global variable thisenv is
//     shared among the threads and points to the main process's env.
static void
thread_func(void *arg)
{
	int i;
	int thread_id = (int) (uintptr_t) arg;
	envid_t env_id = sys_getenvid();

	for (i = 0; i < NITER; i++) {
		cprintf("Thread %d, env %08x: shared_counter antes = "
		        "%d\n",
		        thread_id,
		        env_id,
		        shared_counter);

		shared_counter++;

		cprintf("Thread %d, env %08x: shared_counter despues = "
		        "%d\n",
		        thread_id,
		        env_id,
		        shared_counter);

		sys_yield();
	}
}

void
umain(int argc, char **argv)
{
	int i;

	for (i = 0; i < NTHREADS; i++) {
		thread_create(thread_func, (void *) (uintptr_t) i);
	}

	while (shared_counter < (NTHREADS * NITER)) {
		sys_yield();
	}

	cprintf("Creación de threads finalizada.\n");
	cprintf("Contador Esperado de threads: %d)\n", NTHREADS * NITER);
	cprintf("Contador Obtenido: %d\n", shared_counter);

	if (shared_counter == NTHREADS * NITER) {
		cprintf("Los threads comparten memoria correctamente.\n");
	}

	else {
		cprintf("Error: posible condicion de carrera o memoria no "
		        "compartida.\n");
	}

	// Hints:
	//   1. Create NTHREADS threads with thread_create(thread_func, arg),
	//      passing a different identifier for each one in arg.
	//   2. Wait for them to finish before printing the final result
	//      (with sys_yield(), or with your own waiting scheme).
	//   3. Print shared_counter at the end: if the threads share memory it
	//      should be NTHREADS * NITER, not 0.
}