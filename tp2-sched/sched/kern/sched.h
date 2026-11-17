/* See COPYRIGHT for copyright information. */

#ifndef JOS_KERN_SCHED_H
#define JOS_KERN_SCHED_H
#ifndef JOS_KERNEL
#error "This is a JOS kernel header; user programs should not #include it"
#endif

// This function does not return.
void sched_yield(void) __attribute__((noreturn));

// Wake up sleeping environments whose env_sleep_until has already elapsed.
// Part 2: implement in kern/sched.c, call from trap_dispatch.
void sched_wakeup_sleeping(void);

// *-*-*-*-*-*-*-*-*-*-*-*-*-*-*-
// chris
// Part 3: suma un tick de CPU al nivel de prioridad del proceso que se
// estaba ejecutando cuando salto el timer (estadisticas por prioridad).
// Se llama desde trap_dispatch en cada interrupcion de timer.
void sched_account_tick(void);

#endif  // !JOS_KERN_SCHED_H
// *-*-*-*-*-*-*-*-*-*-*-*-*-*-*-
