// *-*-*-*-*-*-*-*-*-*-*-*-*-*-*-
// chris
// Part 3: test de aging (anti-starvation).
//
// Escenario: dos procesos CPU-bound que NUNCA ceden la CPU por su cuenta,
// asi que lo unico que se las puede sacar es la interrupcion del timer.
//
//   - ALTA (prioridad 10): hace un trabajo largo, en bloques, e imprime una
//     linea cada vez que termina un bloque.
//   - BAJA (prioridad 6): imprime una linea apenas consigue la CPU, hace un
//     trabajo corto y termina.
//
// Con aging, BAJA espera unas decisiones, su prioridad efectiva alcanza a la
// de ALTA y obtiene la CPU ANTES de que ALTA termine: su primera linea sale
// en el medio de los bloques de ALTA.
//
// Sin aging (make ... USE_PR=1 NO_AGING=1), BAJA no corre hasta que ALTA
// termina por completo: es la starvation contra la que se compara.
//
// Los hijos nacen con la prioridad del padre; el padre les asigna la real
// justo despues del fork (igual que priotest). Mientras no se las asigna, los
// hijos esperan cediendo la CPU, para no correr con la prioridad heredada.

#include <inc/lib.h>

#define PRIO_ALTA 10
#define PRIO_BAJA 6

#define BLOQUES 10
// Iteraciones de cada bloque de ALTA. Tiene que sumar bastante mas CPU que el
// peor caso de espera de BAJA (ver sched.c: unas 40 decisiones = 40 ticks),
// incluso en una maquina bastante mas rapida que la de desarrollo.
#define ITER_BLOQUE 80000000u
#define ITER_BAJA 1000000u

// Un proceso sigue vivo mientras su slot de envs[] (mapeado de solo lectura en
// UENVS) siga siendo suyo y no haya vuelto a la lista de libres.
static bool
alive(envid_t id)
{
	const volatile struct Env *e = &envs[ENVX(id)];

	return e->env_id == id && e->env_status != ENV_FREE;
}

// Trabajo puramente de CPU: sin syscalls, nunca cede la CPU voluntariamente.
static void
trabajo(unsigned n)
{
	volatile unsigned acumulado = 0;
	unsigned i;

	for (i = 0; i < n; i++)
		acumulado += i;
}

static void
esperar_asignacion(int heredada)
{
	while (sys_getpriority(0) == heredada)
		sys_yield();
}

static void
correr_alta(void)
{
	int b;

	for (b = 1; b <= BLOQUES; b++) {
		trabajo(ITER_BLOQUE);
		cprintf("[ALTA %08x] bloque %d/%d\n", thisenv->env_id, b, BLOQUES);
	}
	cprintf("[ALTA %08x] TERMINE\n", thisenv->env_id);
}

static void
correr_baja(void)
{
	cprintf("[BAJA %08x] obtuve la CPU\n", thisenv->env_id);
	trabajo(ITER_BAJA);
	cprintf("[BAJA %08x] TERMINE\n", thisenv->env_id);
}

void
umain(int argc, char **argv)
{
	int base = sys_getpriority(0);
	envid_t alta, baja;

	cprintf("agingtest: ALTA=%d BAJA=%d (por defecto %d)\n",
	        PRIO_ALTA,
	        PRIO_BAJA,
	        base);

	alta = fork();
	if (alta < 0)
		panic("agingtest: fork: %e", alta);
	if (alta == 0) {
		esperar_asignacion(base);
		correr_alta();
		exit();
	}
	sys_setpriority(alta, PRIO_ALTA);

	baja = fork();
	if (baja < 0)
		panic("agingtest: fork: %e", baja);
	if (baja == 0) {
		esperar_asignacion(base);
		correr_baja();
		exit();
	}
	sys_setpriority(baja, PRIO_BAJA);

	// El padre se saca de en medio: baja su propia prioridad (esta
	// permitido) y duerme en vez de competir por la CPU.
	sys_setpriority(0, 0);
	while (alive(alta) || alive(baja))
		sys_sleep(5);

	cprintf("agingtest: fin\n");
}
// *-*-*-*-*-*-*-*-*-*-*-*-*-*-*-
