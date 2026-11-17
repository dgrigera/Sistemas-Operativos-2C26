#include <inc/assert.h>
#include <inc/x86.h>
#include <kern/spinlock.h>
#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/monitor.h>
#include <kern/trap.h>

static uint32_t total_sched_calls = 0;

void sched_halt(void);

// *-*-*-*-*-*-*-*-*-*-*-*-*-*-*-
// chris
// ---------------------------------------------------------------------
// Estadisticas del scheduler (se imprimen en sched_halt)
// ---------------------------------------------------------------------

// Cantidad total de veces que se llamo a sched_yield().
static uint32_t sched_calls = 0;

// Cuantas veces se ejecuto cada proceso. No alcanza con leer env_runs al
// final: cuando un proceso termina, env_free lo borra y su slot del arreglo
// 'envs' se recicla, asi que cuando todos terminaron ya no queda ningun
// env_runs para leer. Por eso llevamos esta tabla aparte, indexada por
// env_id (que es unico: la parte alta del id cambia cada vez que se
// recicla el slot).
struct EnvStat {
	envid_t id;
	uint32_t runs;
};
static struct EnvStat env_stats[NENV];
static int env_stats_count = 0;

// Procesos que no entraron en la tabla por estar llena (solo pasaria si
// se crearon mas de NENV procesos distintos en toda la corrida).
static uint32_t env_stats_lost = 0;

// Indice (dentro de 'envs') del ultimo proceso que elegimos para correr.
// Round robin arranca la busqueda justo despues de este. Es global porque
// curenv no sirve para esto: es NULL cuando el proceso que corria acaba de
// terminar o cuando la CPU estaba idle.
static int last_run = -1;

#ifdef SCHED_PRIORITIES
// ---------------------------------------------------------------------
// Part 3: scheduler por prioridades con aging
// ---------------------------------------------------------------------
//
// Prioridad efectiva de un proceso listo:
//
//     efectiva = env_priority + aging_bonus(env_wait_ticks)
//     aging_bonus(espera)  = espera / AGING_STEP      (division entera)
//
// env_priority es la prioridad BASE: la fija el usuario con sys_setpriority
// y el aging NUNCA la modifica. env_wait_ticks cuenta cuantas decisiones de
// scheduling lleva el proceso listo sin ser elegido (vuelve a 0 apenas lo
// eligen). O sea: cada AGING_STEP decisiones esperando, el proceso sube un
// nivel de prioridad efectiva.
//
// Por que AGING_STEP = 8: tiene que ser lo bastante grande como para que
// la prioridad base siga mandando en el corto plazo (un proceso de
// prioridad alta que hace pocas iteraciones no deberia ser alcanzado por
// uno inferior en medio de su trabajo: con 8, un hijo 1 nivel mas bajo
// recien lo empata tras 8 decisiones), y lo bastante chico como para que la
// espera maxima sea finita y corta (peor caso: ver mas abajo).
//
// PEOR CASO DE ESPERA. Sea X un proceso listo de prioridad base p, y sean
// n los otros procesos que compiten con el. Tras esperar
// T = AGING_STEP * (PRIORITY_MAX - p + 1) decisiones su prioridad efectiva
// es >= PRIORITY_MAX + 1, mayor que la base de CUALQUIER proceso (incluso
// uno recien elegido, que vuelve a tener espera 0). Desde ahi, un proceso Y
// solo puede ganarle si tambien envejecio por lo menos lo mismo, y en cuanto
// se lo elige vuelve a espera 0 y ya no puede volver a ganarle (le faltan
// otra vez >= T decisiones). Entonces cada uno de los n procesos puede
// elegirse como mucho UNA vez mas antes que X, y los procesos nuevos (que
// nacen con espera 0) no le ganan nunca. Resultado:
//
//     espera maxima de X <= T + n decisiones de scheduling
//
// Con los valores elegidos (PRIORITY_MAX = 15, AGING_STEP = 8) el peor caso
// es p = 0: T = 8 * 16 = 128 decisiones, mas n. Como el numero de
// decisiones es finito para todo p, ningun proceso listo sufre starvation.
#define AGING_STEP 8

// Es un macro y no una funcion porque el kernel se compila con -fno-inline y
// esto se evalua una vez por cada proceso candidato en cada decision.
#ifdef NO_AGING
// Solo para el informe (make ... NO_AGING=1): anula el bonus para mostrar la
// starvation que ocurre sin aging.
#define aging_bonus(espera) 0u
#else
#define aging_bonus(espera) ((espera) / AGING_STEP)
#endif

// Cuantas veces la decision del scheduler fue DISTINTA de la que hubiera
// tomado mirando solo la prioridad base, es decir: cuantas veces el aging
// fue decisivo. Las estadisticas que solo muestran env_priority se ven
// iguales con o sin aging; esta no.
static uint32_t aging_decisivo = 0;

// Ticks de timer acumulados por nivel de prioridad base: a que nivel
// pertenecia el proceso que estaba corriendo cada vez que salto el timer.
static uint32_t prio_ticks[PRIORITY_MAX + 1];

// Historial de los ultimos HISTORIAL_N procesos elegidos (buffer circular):
// que proceso, en que tick se lo eligio, y con que prioridad base y efectiva.
#define HISTORIAL_N 16
struct HistEntry {
	envid_t id;
	uint32_t tick;
	uint32_t base;
	uint32_t efectiva;
};
static struct HistEntry historial[HISTORIAL_N];
static uint32_t historial_total = 0;  // cuantas decisiones se registraron

static void
historial_registrar(struct Env *e, uint32_t efectiva)
{
	struct HistEntry *h;

	// Solo anotamos cuando EMPIEZA a correr un proceso distinto: si el
	// scheduler vuelve a elegir al mismo, no es un inicio nuevo, y el
	// historial se llenaria de repeticiones del que acapara la CPU.
	if (historial_total > 0 &&
	    historial[(historial_total - 1) % HISTORIAL_N].id == e->env_id)
		return;

	h = &historial[historial_total % HISTORIAL_N];
	h->id = e->env_id;
	h->tick = ticks;
	h->base = e->env_priority;
	h->efectiva = efectiva;
	historial_total++;
}
#endif  // SCHED_PRIORITIES

// Part 3: llamada desde el handler del timer, antes de sched_yield. Le suma
// un tick al nivel de prioridad del proceso que estaba corriendo. Si la CPU
// estaba ociosa (curenv == NULL) el tick no se le carga a nadie.
void
sched_account_tick(void)
{
#ifdef SCHED_PRIORITIES
	if (curenv && curenv->env_status == ENV_RUNNING &&
	    curenv->env_priority <= PRIORITY_MAX)
		prio_ticks[curenv->env_priority]++;
#endif
}

// Para cada slot de 'envs': indice+1 en env_stats de la ultima entrada
// registrada (0 = ninguna). Evita la busqueda lineal en cada decision.
static int stat_slot[NENV];

// Suma una ejecucion al proceso 'e' en la tabla de estadisticas.
static void
stats_record_run(struct Env *e)
{
	int slot = ENVX(e->env_id);
	int k = stat_slot[slot] - 1;

	if (k >= 0 && env_stats[k].id == e->env_id) {
		env_stats[k].runs++;
		return;
	}

	if (env_stats_count < NENV) {
		env_stats[env_stats_count].id = e->env_id;
		env_stats[env_stats_count].runs = 1;
		stat_slot[slot] = env_stats_count + 1;
		env_stats_count++;
	} else {
		env_stats_lost++;
	}
}

// Imprime las estadisticas acumuladas. Se llama desde sched_halt cuando ya
// no queda ningun proceso vivo.
static void
stats_print(void)
{
	int i;

	cprintf("\n--- Estadisticas del scheduler ---\n");
	cprintf("Llamadas al scheduler: %u\n", sched_calls);
	cprintf("Tiempo total de ejecucion: %u ticks\n", ticks);
	cprintf("Ejecuciones por proceso:\n");
	for (i = 0; i < env_stats_count; i++)
		cprintf("  [%08x] %u veces\n", env_stats[i].id, env_stats[i].runs);
	if (env_stats_lost > 0)
		cprintf("  (%u procesos sin registrar: tabla llena)\n",
		        env_stats_lost);

#ifdef SCHED_PRIORITIES
	cprintf("\n--- Estadisticas de prioridades ---\n");
#ifdef NO_AGING
	cprintf("Aging: DESACTIVADO (NO_AGING)\n");
#else
	cprintf("Aging: +1 de prioridad efectiva cada %d decisiones "
	        "esperando\n",
	        AGING_STEP);
#endif
	cprintf("Decisiones en las que el aging cambio al proceso elegido: "
	        "%u\n",
	        aging_decisivo);

	{
		uint32_t n = historial_total < HISTORIAL_N ? historial_total
		                                           : HISTORIAL_N;
		uint32_t desde = historial_total - n;
		uint32_t k;

		cprintf("Historial de los ultimos %u procesos elegidos "
		        "(tick de inicio, base/efectiva):\n",
		        n);
		for (k = 0; k < n; k++) {
			struct HistEntry *h =
			        &historial[(desde + k) % HISTORIAL_N];

			cprintf("  tick %u: [%08x] base %u, efectiva %u\n",
			        h->tick,
			        h->id,
			        h->base,
			        h->efectiva);
		}
	}

	cprintf("Ticks de CPU por nivel de prioridad base:\n");
	for (i = PRIORITY_MAX; i >= PRIORITY_MIN; i--)
		if (prio_ticks[i] > 0)
			cprintf("  prioridad %d: %u ticks\n", i, prio_ticks[i]);
#endif
}

// Elige a 'e' como proximo proceso a correr: deja registrada la decision
// (para las estadisticas y para el round robin) y le da la CPU. No retorna.
static void run_chosen(struct Env *e) __attribute__((noreturn));

static void
run_chosen(struct Env *e)
{
	last_run = ENVX(e->env_id);
	stats_record_run(e);
	env_run(e);
}
// *-*-*-*-*-*-*-*-*-*-*-*-*-*-*-

// Wake up environments that were sleeping and whose env_sleep_until <= ticks.
// The global counter 'ticks' is declared in kern/trap.h.
// Call from trap_dispatch after incrementing ticks and before sched_yield.
void
sched_wakeup_sleeping(void)
{
	for (int i = 0; i < NENV; i++) {
		if (envs[i].env_status == ENV_NOT_RUNNABLE &&
		    envs[i].env_sleep_until != 0 &&
		    envs[i].env_sleep_until <= ticks) {
			envs[i].env_status = ENV_RUNNABLE;
			envs[i].env_sleep_until = 0;
		}
	}
}

// Choose a user environment to run and run it.
void
sched_yield(void)
{
	total_sched_calls++;


#ifdef SCHED_ROUND_ROBIN
	// Implement round-robin scheduling with timer preemption.
	//
	// Walk 'envs' circularly looking for an ENV_RUNNABLE environment,
	// starting right after the last environment run on this CPU. Switch
	// to the first one found.
	//
	// If no env is runnable but the environment that was running on this
	// CPU is still ENV_RUNNING, it's fine to pick it again.
	//
	// Never pick an environment that's currently running on another CPU
	// (env_status == ENV_RUNNING). If none is available, fall through
	// to sched_halt.

	int start = 0;
	int i;
	int index;

	// Comenzamos justo después del último entorno ejecutado.
	if (curenv != NULL)
		start = ENVX(curenv->env_id) + 1;

	// Recorremos circularmente todos los entornos.
	for (i = 0; i < NENV; i++) {
		index = (start + i) % NENV;

		// Elegimos el primer entorno que esté listo para ejecutarse.
		if (envs[index].env_status == ENV_RUNNABLE)
			env_run(&envs[index]);
	}

	// Si no hay otros entornos listos, continuamos con el actual si todavía está ejecutándose
	if (curenv != NULL && curenv->env_status == ENV_RUNNING)
		env_run(curenv);

#endif

#ifdef SCHED_PRIORITIES
	// Implement priority scheduling with anti-starvation aging.
	//
	// Select the ENV_RUNNABLE environment with the highest env_priority.
	// Increment env_wait_ticks for the environments that are not selected.
	// Apply aging: if an environment has been waiting too long, temporarily
	// increase its effective priority to avoid starvation.
	//
	// If the selected environment is the same as curenv and it's still
	// ENV_RUNNING, it's fine to run it again.

	// Your code here - Priorities
	// *-*-*-*-*-*-*-*-*-*-*-*-*-*-*-
	// chris
	// Elegimos al proceso de mayor prioridad EFECTIVA entre los candidatos.
	// Recorremos 'envs' en orden circular desde el ultimo elegido y solo
	// reemplazamos al mejor si el nuevo es ESTRICTAMENTE mejor: entre
	// procesos con la misma prioridad efectiva gana el primero que
	// encontramos, o sea que desempatamos con round robin.
	//
	// Los candidatos son los ENV_RUNNABLE y tambien el proceso que venia
	// corriendo en esta CPU (curenv) si sigue ENV_RUNNING: tiene que
	// competir, si no un proceso de prioridad alta que consume CPU seria
	// desplazado en cada tick por cualquiera de prioridad mas baja. Como
	// curenv es el ultimo que miramos (es last_run) y gana solo si es
	// estrictamente mejor, cuando empata con otro le cede el turno.
	struct Env *elegido = NULL;
	struct Env *elegido_sin_aging = NULL;
	uint32_t efectiva_elegido = 0;
	int n = envs_alto;  // solo los slots que alguna vez se usaron
	int idx = last_run + 1;
	int i;

	// Una sola pasada, circular desde last_run + 1 y solo por los slots
	// que alguna vez se usaron (envs_alto): primes crea cientos de
	// procesos que se pasan mensajes sin parar, lo que son millones de
	// decisiones, y recorrer los NENV slots por decision lo dejaba
	// fuera del timeout de los tests.
	if (idx >= n)
		idx = 0;

	for (i = 0; i < n; i++, idx++) {
		struct Env *e;
		uint32_t efectiva;

		if (idx >= n)
			idx = 0;
		e = &envs[idx];

		if (e->env_status != ENV_RUNNABLE &&
		    !(e == curenv && e->env_status == ENV_RUNNING))
			continue;

		efectiva = e->env_priority + aging_bonus(e->env_wait_ticks);

		if (!elegido || efectiva > efectiva_elegido) {
			elegido = e;
			efectiva_elegido = efectiva;
		}

		// Quien hubiera ganado SIN aging (solo con la prioridad base),
		// para contar cuando el aging cambio la decision.
		if (!elegido_sin_aging ||
		    e->env_priority > elegido_sin_aging->env_priority)
			elegido_sin_aging = e;

		// Aging: sumamos una decision de espera a todo el que quedo
		// listo, asumiendo que no va a ser elegido. Si resulta ser el
		// ganador se le pisa con 0 mas abajo. A curenv (RUNNING) no se
		// le suma nada: venia corriendo, no esperando.
		if (e->env_status == ENV_RUNNABLE)
			e->env_wait_ticks++;
	}

	if (elegido) {
		if (elegido != elegido_sin_aging)
			aging_decisivo++;

		historial_registrar(elegido, efectiva_elegido);

		// El elegido empieza de cero: ya no esta esperando.
		elegido->env_wait_ticks = 0;
		run_chosen(elegido);
	}
	// *-*-*-*-*-*-*-*-*-*-*-*-*-*-*-
#endif

	// TODO: remove after implementing the scheduler
	// Without scheduler, keep runing the last environment while it exists
	// *-*-*-*-*-*-*-*-*-*-*-*-*-*-*-
	// chris
	// Se saco el env_run(curenv) que estaba aca: con el scheduler ya
	// implementado, volvia a correr a curenv aunque se hubiera dormido
	// (ENV_NOT_RUNNABLE). Ahora cada sched_yield (RR o prioridades) ya
	// vuelve a elegir a curenv por su cuenta cuando corresponde.
	// *-*-*-*-*-*-*-*-*-*-*-*-*-*-*-

	// sched_halt never returns
	sched_halt();
}

// Halt this CPU when there is nothing to do. Wait until the
// timer interrupt wakes it up. This function never returns.
//
void
sched_halt(void)
{
	int i;

	// For debugging and testing purposes, if there are no runnable
	// environments in the system, then drop into the kernel monitor.
	//
	// Part 2: careful, a sleeping process (sys_sleep) is ENV_NOT_RUNNABLE,
	// so it doesn't count as "runnable" in this check. What happens, then,
	// if every process is sleeping at the same time? Review this condition
	// to distinguish "nothing to do right now" (should wait for the timer)
	// from "no process is left alive" (only then should statistics be
	// printed and the kernel halted for good).
	for (i = 0; i < NENV; i++) {
		if ((envs[i].env_status == ENV_RUNNABLE ||
		     envs[i].env_status == ENV_RUNNING ||
		     envs[i].env_status == ENV_NOT_RUNNABLE ||
		     envs[i].env_status == ENV_DYING))
			break;
	}
	// *-*-*-*-*-*-*-*-*-*-*-*-*-*-*-
	// chris
	// Un proceso dormido (ENV_NOT_RUNNABLE con env_sleep_until != 0) cuenta
	// como vivo: no hay nada para correr ahora, pero el timer lo va a
	// despertar. Por eso en ese caso NO caemos al monitor: seguimos hasta
	// el hlt de mas abajo, y la proxima interrupcion de timer lo despierta.
	// Un ENV_NOT_RUNNABLE con env_sleep_until == 0 (por ejemplo bloqueado
	// esperando IPC) no cuenta: si es lo unico que queda nadie lo va a
	// despertar.
	if (i == NENV) {
		int j;

		for (j = 0; j < NENV; j++) {
			if (envs[j].env_status == ENV_NOT_RUNNABLE &&
			    envs[j].env_sleep_until != 0) {
				i = j;
				break;
			}
		}
	}
	// *-*-*-*-*-*-*-*-*-*-*-*-*-*-*-
	if (i == NENV) {
		cprintf("No runnable environments in the system!\n");

		// Once the scheduler has finishied it's work, print statistics on performance.

		cprintf("\n===== Scheduler Statistics =====\n");
		cprintf("Total scheduler calls: %u\n", total_sched_calls);
		cprintf("Total timer ticks: %u\n", ticks);

		for (int j = 0; j < NENV; j++) {
			if (envs[j].env_runs == 0)
				continue;

			cprintf("Environment %08x:\n", envs[j].env_id);
			cprintf("  Runs: %u\n", envs[j].env_runs);
			cprintf("  Priority: %u\n", envs[j].env_priority);
			cprintf("  Wait ticks: %u\n", envs[j].env_wait_ticks);
		}

		while (1)
			monitor(NULL);
	}

	// Mark that no environment is running on this CPU
	curenv = NULL;
	lcr3(PADDR(kern_pgdir));

	// Mark that this CPU is in the HALT state, so that when
	// timer interupts come in, we know we should re-acquire the
	// big kernel lock
	xchg(&thiscpu->cpu_status, CPU_HALTED);

	// Release the big kernel lock as if we were "leaving" the kernel
	unlock_kernel();

	// Reset stack pointer, enable interrupts and then halt.
	asm volatile("movl $0, %%ebp\n"
	             "movl %0, %%esp\n"
	             "pushl $0\n"
	             "pushl $0\n"
	             "sti\n"
	             "1:\n"
	             "hlt\n"
	             "jmp 1b\n"
	             :
	             : "a"(thiscpu->cpu_ts.ts_esp0));
}
