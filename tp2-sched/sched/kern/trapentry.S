/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>

#include <kern/picirq.h>


###################################################################
# exceptions/interrupts
###################################################################

/* TRAPHANDLER defines a globally-visible function for handling a trap.
 * It pushes a trap number onto the stack, then jumps to _alltraps.
 * Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
 *
 * You shouldn't call a TRAPHANDLER function from C, but you may
 * need to _declare_ one in C (for instance, to get a function pointer
 * during IDT setup).  You can declare the function with
 *   void NAME();
 * where NAME is the argument passed to TRAPHANDLER.
 */
#define TRAPHANDLER(name, num)						\
	.globl name;		/* define global symbol for 'name' */	\
	.type name, @function;	/* symbol type is function */		\
	.align 2;		/* align function definition */		\
	name:			/* function starts here */		\
	pushl $(num);							\
	jmp _alltraps

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num)					\
	.globl name;							\
	.type name, @function;						\
	.align 2;							\
	name:								\
	pushl $0;							\
	pushl $(num);							\
	jmp _alltraps

.text

TRAPHANDLER_NOEC(trap0, T_DIVIDE)
TRAPHANDLER_NOEC(trap1, T_DEBUG)
TRAPHANDLER_NOEC(trap2, T_NMI)
TRAPHANDLER_NOEC(trap3, T_BRKPT)
TRAPHANDLER_NOEC(trap4, T_OFLOW)
TRAPHANDLER_NOEC(trap5, T_BOUND)
TRAPHANDLER_NOEC(trap6, T_ILLOP)
TRAPHANDLER_NOEC(trap7, T_DEVICE)
TRAPHANDLER(trap8, T_DBLFLT)
TRAPHANDLER_NOEC(trap9, 9)
TRAPHANDLER(trap10, T_TSS)
TRAPHANDLER(trap11, T_SEGNP)
TRAPHANDLER(trap12, T_STACK)
TRAPHANDLER(trap13, T_GPFLT)
TRAPHANDLER(trap14, T_PGFLT)
TRAPHANDLER_NOEC(trap15, 15)
TRAPHANDLER_NOEC(trap16, T_FPERR)
TRAPHANDLER(trap17, T_ALIGN)
TRAPHANDLER_NOEC(trap18, T_MCHK)
TRAPHANDLER_NOEC(trap19, T_SIMDERR)

TRAPHANDLER_NOEC(trap32, IRQ_OFFSET + IRQ_TIMER)

TRAPHANDLER_NOEC(trap48, T_SYSCALL)


/*
 * _alltraps
 *
 * Punto de entrada comun al que saltan TODOS los traps (excepciones,
 * syscalls, interrupciones de hardware) despues de que el stub generado
 * por TRAPHANDLER/TRAPHANDLER_NOEC empuja el numero de trap (y un error
 * code, real o un 0 de relleno) a la pila.
 *
 * En este punto la pila ya tiene, de la direccion mas baja (tope) hacia
 * arriba: trapno, err, eip, cs, eflags, [esp, ss si veniamos de ring 3].
 * Eso es la "segunda mitad" de un struct Trapframe (inc/trap.h) -justo
 * la parte que el hardware arma solo al generar la interrupcion-. Lo que
 * falta para tener el struct completo es la "primera mitad": los
 * registros de proposito general (PushRegs) y los segmentos es/ds.
 *
 * El objetivo es terminar con %esp apuntando al inicio de un
 * struct Trapframe armado en la pila, y pasarselo como unico argumento
 * a trap() (inc/trap.h: void trap(struct Trapframe *tf)), que es quien
 * decide que hacer con el trap y nunca retorna a este punto.
 */
_alltraps:
        # tf_ds y tf_es: guardamos los segmentos que tenia el codigo
        # interrumpido ANTES de pisarlos, para poder restaurarlos mas
        # adelante si correspondiera (ver context_switch). El orden
        # importa: como la pila crece hacia abajo, lo ultimo que
        # pusheamos queda a la direccion mas baja, asi que para que
        # "es" quede antes que "ds" en memoria (como pide el struct)
        # hay que pushear ds primero y es despues.
        pushl %ds
        pushl %es

        # struct PushRegs: pushal empuja edi, esi, ebp, esp(original),
        # ebx, edx, ecx, eax -en ese orden-, que es exactamente el
        # orden de los campos de struct PushRegs en inc/trap.h.
        pushal

        # El codigo que venia ejecutandose (interrumpido) pudo haber
        # estado en modo usuario, con %ds/%es apuntando a segmentos de
        # usuario. Antes de tocar memoria del kernel (lo que hace
        # trap() en C) hay que asegurarse de que %ds/%es apunten al
        # segmento de datos del KERNEL (GD_KD, definido en
        # inc/memlayout.h), sino cualquier acceso a memoria del lado
        # del kernel podria interpretarse con permisos/base incorrectos.
        movw $GD_KD, %ax
        movw %ax, %ds
        movw %ax, %es

        # En este punto %esp apunta exactamente al inicio del struct
        # Trapframe que acabamos de terminar de armar en la pila (abajo
        # quedan trapno, err, eip, cs, eflags, etc., que ya estaban ahi
        # desde antes de entrar a _alltraps). Lo pusheamos como
        # argumento -convencion cdecl- y llamamos a trap(), que es la
        # funcion de kern/trap.c que arma el resto del manejo (la
        # declaracion de C espera 'struct Trapframe *').
        pushl %esp
        call trap

        # trap() no deberia volver nunca (termina en env_run() o
        # sched_yield(), que tampoco retornan), pero por las dudas no
        # dejamos esto colgando sin instruccion: un trap que sí
        # retornara aca seria un bug de otro lado, no algo que
        # debamos resolver en este punto.
        1: jmp 1b