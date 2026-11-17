// User-space thread library
//
// Provide thread_create() as an abstraction over sys_thread_create().
// thread_create allocates the thread's stack. It is not freed when the thread
// ends (a thread cannot free the stack it is running on): it goes away with
// the rest of the process address space.

#include <inc/lib.h>

// Size of each thread's stack (in bytes). Can be changed if justified.
#define THREAD_STACK_SIZE PGSIZE

// Donde van los stacks (criterio explicado en el informe, Parte 4):
//
//   USTACKTOP
//   [USTACKTOP - PGSIZE, USTACKTOP)  stack del thread principal
//   una pagina de guarda, sin mapear
//   stack del thread 0
//   una pagina de guarda, sin mapear
//   stack del thread 1
//   ... (cada thread nuevo queda mas abajo en memoria)
//
// Cada thread usa un slot fijo: su stack mas una pagina de guarda debajo.
// La guarda nunca se mapea: si un thread se pasa de su stack, en vez de
// pisar el stack del vecino se produce un page fault.
#define THREAD_GUARD_SIZE PGSIZE
#define THREAD_SLOT_SIZE (THREAD_STACK_SIZE + THREAD_GUARD_SIZE)
#define THREAD_FIRST_TOP (USTACKTOP - PGSIZE - THREAD_GUARD_SIZE)

// Maximo de threads que se pueden crear por proceso. Los stacks de threads
// que ya terminaron no se reutilizan, asi que es un tope de creaciones.
#define THREAD_MAX 256

// Proximo slot libre. Es una variable global del proceso, o sea que la
// comparten todos sus threads: thread_create no es seguro si dos threads
// lo llaman a la vez.
static int next_slot = 0;

envid_t
thread_create(void (*func)(void *), void *arg)
{
	uintptr_t top;
	void *bottom;
	uint32_t *sp;
	envid_t id;
	int r;

	if (next_slot >= THREAD_MAX)
		return -E_NO_MEM;

	// direccion justo por encima del stack de este thread
	top = THREAD_FIRST_TOP - (uintptr_t) next_slot * THREAD_SLOT_SIZE;
	bottom = (void *) (top - THREAD_STACK_SIZE);

	r = sys_page_alloc(0, bottom, PTE_P | PTE_U | PTE_W);
	if (r < 0)
		return r;

	// Dejamos el stack como si func hubiera sido llamada con "call":
	//   [esp]      direccion de retorno: exit, asi el thread termina
	//              al volver de func
	//   [esp + 4]  primer argumento de func: arg
	sp = (uint32_t *) (top - 2 * sizeof(uint32_t));
	sp[0] = (uintptr_t) exit;
	sp[1] = (uintptr_t) arg;

	id = sys_thread_create((void *) func, sp);
	if (id < 0) {
		sys_page_unmap(0, bottom);
		return id;
	}

	next_slot++;
	return id;
}
