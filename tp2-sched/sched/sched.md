# sched

## Sesión de GDB para _context switch_

QEMU en una terminal con `make qemu-nox-gdb USE_RR=1` y GDB en otra con `make gdb`, con `kern/init.c` ejecutando solamente `user_hello`

### Contenido del stack al inicio de la llamada `context_switch`

```
(gdb) b context_switch
Breakpoint 1 at 0xf010418b: file kern/switch.S, line 24.
(gdb) c
Continuing.
The target architecture is assumed to be i386
=> 0xf010418b <context_switch>: mov    0x4(%esp),%esp

Breakpoint 1, context_switch () at kern/switch.S:24
24              movl 4(%esp), %esp
(gdb) info registers
eax            0x3bb000 3911680
ecx            0xf03bb000       -264523776
edx            0x1f8    504
ebx            0xf02e6000       -265396224
esp            0xf011ffbc       0xf011ffbc
ebp            0xf011ffd8       0xf011ffd8
esi            0xf02e6000       -265396224
edi            0x2a6000 2777088
eip            0xf010418b       0xf010418b <context_switch>
eflags         0x12     [ AF ]
cs             0x8      8
ss             0x10     16
ds             0x10     16
es             0x10     16
fs             0x23     35
gs             0x23     35
(gdb) x/2wx $esp
0xf011ffbc:     0xf010347c      0xf02e6000
(gdb) x/17wx *(unsigned*)($esp+4)
0xf02e6000:     0x00000000      0x00000000      0x00000000      0x00000000
0xf02e6010:     0x00000000      0x00000000      0x00000000      0x00000000
0xf02e6020:     0x00000023      0x00000023      0x00000000      0x00000000
0xf02e6030:     0x00800020      0x0000001b      0x00000200      0xeebfe000
0xf02e6040:     0x00000023
```
Al entrar a `context_switch` el stack del kernel tiene arriba de todo la direccion de retorno (`0xf010347c` dentro de `env_run`) y despues el argumento `tf` (`0xf02e6000`) que es un puntero al trapframe del proceso. La direccion de retorno no se usa nunca porque la funcion no vuelve. El trapframe tiene 17 palabras: los 8 registros generales en 0 porque el proceso recien arranca, `es` y `ds` en `0x23`, `trapno` y `err` en 0 y los 5 valores que usa `iret`: `eip=0x800020` (donde arranca `hello`), `cs=0x1b` (codigo de usuario, ring 3), `eflags=0x200` (solo la flag IF, o sea interrupciones habilitadas), `esp=0xeebfe000` (el tope del stack de usuario) y `ss=0x23`. Los registros de ahora (`cs=0x8`, `ss=0x10`) muestran que seguimos en el kernel, en ring 0.

### Contenido del stack instrucción a instrucción

**Paso 1:** `movl 4(%esp),%esp`

```
(gdb) display/i $eip
1: x/i $eip
=> 0xf010418b <context_switch>: mov    0x4(%esp),%esp
(gdb) display/17wx $esp
2: x/17xw $esp
0xf011ffbc:     0xf010347c      0xf02e6000      0xf01059c8      0xf011ffd8
0xf011ffcc:     0xf0103468      0x00009500      0x00000000      0xf011fff8
0xf011ffdc:     0xf0100242      0xf02e6000      0x00000000      0x00042008
0xf011ffec:     0x00000000      0x00000000      0x00000000      0x00000000
0xf011fffc:     0xf0100047
(gdb) stepi
=> 0xf010418f <context_switch+4>:       popa   
context_switch () at kern/switch.S:34
34              popal
1: x/i $eip
=> 0xf010418f <context_switch+4>:       popa   
2: x/17xw $esp
0xf02e6000:     0x00000000      0x00000000      0x00000000      0x00000000
0xf02e6010:     0x00000000      0x00000000      0x00000000      0x00000000
0xf02e6020:     0x00000023      0x00000023      0x00000000      0x00000000
0xf02e6030:     0x00800020      0x0000001b      0x00000200      0xeebfe000
0xf02e6040:     0x00000023
(gdb) info registers
eax            0x3bb000 3911680
ecx            0xf03bb000       -264523776
edx            0x1f8    504
ebx            0xf02e6000       -265396224
esp            0xf02e6000       0xf02e6000
ebp            0xf011ffd8       0xf011ffd8
esi            0xf02e6000       -265396224
edi            0x2a6000 2777088
eip            0xf010418f       0xf010418f <context_switch+4>
eflags         0x12     [ AF ]
cs             0x8      8
ss             0x10     16
ds             0x10     16
es             0x10     16
fs             0x23     35
gs             0x23     35
```
`esp` pasa de `0xf011ffbc` (stack del kernel) a `0xf02e6000`, que es la direccion del trapframe. Desde ahora el trapframe se usa como si fuera el stack y las instrucciones que siguen lo van desapilando. Los demas registros no cambian.

**Paso 2:** `popal`

```
(gdb) stepi
=> 0xf0104190 <context_switch+5>:       pop    %es
context_switch () at kern/switch.S:40
40              popl %es
1: x/i $eip
=> 0xf0104190 <context_switch+5>:       pop    %es
2: x/17xw $esp
0xf02e6020:     0x00000023      0x00000023      0x00000000      0x00000000
0xf02e6030:     0x00800020      0x0000001b      0x00000200      0xeebfe000
0xf02e6040:     0x00000023      0xf02e6088      0x00001000      0x00000000
0xf02e6050:     0x00000000      0x00000003      0x00000001      0x00000000
0xf02e6060:     0xf03bb000
(gdb) info registers
eax            0x0      0
ecx            0x0      0
edx            0x0      0
ebx            0x0      0
esp            0xf02e6020       0xf02e6020
ebp            0x0      0x0
esi            0x0      0
edi            0x0      0
eip            0xf0104190       0xf0104190 <context_switch+5>
eflags         0x12     [ AF ]
cs             0x8      8
ss             0x10     16
ds             0x10     16
es             0x10     16
fs             0x23     35
gs             0x23     35
```
`popal` carga los 8 registros generales con lo que hay en el trapframe (todos 0) y sube `esp` 32 bytes hasta `0xf02e6020`. El `esp` que esta guardado ahi no se carga, `popal` lo descarta.

**Paso 3:** `popl %es`

```
(gdb) stepi
=> 0xf0104191 <context_switch+6>:       pop    %ds
41              popl %ds
1: x/i $eip
=> 0xf0104191 <context_switch+6>:       pop    %ds
2: x/17xw $esp
0xf02e6024:     0x00000023      0x00000000      0x00000000      0x00800020
0xf02e6034:     0x0000001b      0x00000200      0xeebfe000      0x00000023
0xf02e6044:     0xf02e6088      0x00001000      0x00000000      0x00000000
0xf02e6054:     0x00000003      0x00000001      0x00000000      0xf03bb000
0xf02e6064:     0x00000000
(gdb) info registers
eax            0x0      0
ecx            0x0      0
edx            0x0      0
ebx            0x0      0
esp            0xf02e6024       0xf02e6024
ebp            0x0      0x0
esi            0x0      0
edi            0x0      0
eip            0xf0104191       0xf0104191 <context_switch+6>
eflags         0x12     [ AF ]
cs             0x8      8
ss             0x10     16
ds             0x10     16
es             0x23     35
fs             0x23     35
gs             0x23     35
```
`popl %es` carga `es` con `0x23` (antes valia `0x10` el segmento de datos del kernel) y `esp` sube 4 bytes hasta `0xf02e6024`.

**Paso 4:** `popl %ds`

```
(gdb) stepi
=> 0xf0104192 <context_switch+7>:       add    $0x8,%esp
45              addl $0x8, %esp
1: x/i $eip
=> 0xf0104192 <context_switch+7>:       add    $0x8,%esp
2: x/17xw $esp
0xf02e6028:     0x00000000      0x00000000      0x00800020      0x0000001b
0xf02e6038:     0x00000200      0xeebfe000      0x00000023      0xf02e6088
0xf02e6048:     0x00001000      0x00000000      0x00000000      0x00000003
0xf02e6058:     0x00000001      0x00000000      0xf03bb000      0x00000000
0xf02e6068:     0x00000000
(gdb) info registers
eax            0x0      0
ecx            0x0      0
edx            0x0      0
ebx            0x0      0
esp            0xf02e6028       0xf02e6028
ebp            0x0      0x0
esi            0x0      0
edi            0x0      0
eip            0xf0104192       0xf0104192 <context_switch+7>
eflags         0x12     [ AF ]
cs             0x8      8
ss             0x10     16
ds             0x23     35
es             0x23     35
fs             0x23     35
gs             0x23     35
```
`popl %ds` hace lo mismo con `ds`, que pasa a `0x23` y `esp` queda en `0xf02e6028`.

**Paso 5:** `addl $8,%esp`

```
(gdb) stepi
=> 0xf0104195 <context_switch+10>:      iret   
52              iret1: x/i $eip
2: x/17xw $esp
0xf02e6030:     0x00800020      0x0000001b      0x00000200      0xeebfe000
0xf02e6040:     0x00000023      0xf02e6088      0x00001000      0x00000000
0xf02e6050:     0x00000000      0x00000003      0x00000001      0x00000000
0xf02e6060:     0xf03bb000      0x00000000      0x00000000      0x00000000
0xf02e6070:     0x00000000
(gdb) info registers
eax            0x0      0
ecx            0x0      0
edx            0x0      0
ebx            0x0      0
esp            0xf02e6030       0xf02e6030
ebp            0x0      0x0
esi            0x0      0
edi            0x0      0
eip            0xf0104195       0xf0104195 <context_switch+10>
eflags         0x96     [ PF AF SF ]
cs             0x8      8
ss             0x10     16
ds             0x23     35
es             0x23     35
fs             0x23     35
gs             0x23     35
```
`addl $8,%esp` se salta `trapno` y `err` que ya no sirven y deja `esp` en `0xf02e6030`. Ahora arriba de todo del stack esta justo lo que necesita `iret`: `eip`, `cs`, `eflags`, `esp` y `ss`. El `eflags` cambia a `0x96` porque la suma modifica los flags pero no importa porque `iret` lo pisa con el del trapframe.

### Cómo se modifican los registros instrucción a instrucción

Con el `movl` solo cambia `esp` que pasa a apuntar al trapframe. `popal` carga `eax`, `ecx`, `edx`, `ebx`, `ebp`, `esi` y `edi` con los valores del trapframe (todos 0). `popl %es` y `popl %ds` cargan `es` y `ds` con `0x23`. El `addl` solo mueve `esp` (y toca los flags). `fs` y `gs` no cambian en ningun momento porque ya valian `0x23`. Por ultimo `iret` cambia `eip`, `cs`, `eflags`, `esp` y `ss` todos juntos y es el que pasa a ring 3. En cada paso `eip` avanza una instruccion hasta llegar al `iret`.

**El iret y el pasaje a modo usuario**


```
(gdb) stepi
=> 0x800020:    cmp    $0xeebfe000,%esp
0x00800020 in ?? ()
1: x/i $eip
=> 0x800020:    cmp    $0xeebfe000,%esp
2: x/17xw $esp
0xeebfe000:     <error: Cannot access memory at address 0xeebfe000>
(gdb) info registers
eax            0x0      0
ecx            0x0      0
edx            0x0      0
ebx            0x0      0
esp            0xeebfe000       0xeebfe000
ebp            0x0      0x0
esi            0x0      0
edi            0x0      0
eip            0x800020 0x800020
eflags         0x202    [ IF ]
cs             0x1b     27
ss             0x23     35
ds             0x23     35
es             0x23     35
fs             0x23     35
gs             0x23     35
```
`iret` saca del stack `eip`, `cs`, `eflags`, `esp` y `ss` todo junto en una sola instruccion: `eip` pasa a `0x800020`, `cs` a `0x1b`, `ss` a `0x23`, `esp` a `0xeebfe000` y `eflags` a `0x202` (flag IF activa). Como los dos bits de abajo de `cs` valen `11` la CPU pasa a ring 3 y empieza a ejecutar codigo de usuario. GDB muestra `0x00800020 in ?? ()` porque no tiene los simbolos de `hello` y no puede leer `0xeebfe000` porque `esp` apunta justo arriba de la pagina del stack de usuario y todavia no se hizo ningun push.

### El camino inverso: desde una syscall en user space hasta `trap` en el kernel

**La instruccion de usuario que genera la interrupcion**

```
(gdb) b _alltraps if *(int*)$esp == 48
Breakpoint 1 at 0xf0104178: file kern/trapentry.S, line 101.
(gdb) c
Continuing.
The target architecture is assumed to be i386
=> 0xf0104178 <_alltraps>:      push   %ds

Breakpoint 1, _alltraps () at kern/trapentry.S:101
101             pushl %ds
(gdb) x/7wx $esp
0xefffffe4:     0x00000030      0x00000000      0x00800a2a      0x0000001b
0xeffffff4:     0x00000292      0xeebfdf98      0x00000023
(gdb) x/2i 0x800a28
   0x800a28:    int    $0x30
   0x800a2a:    cmpl   $0x0,-0x1c(%ebp)
```
El camino empieza en espacio de usuario con la instruccion `int $0x30` (en `0x800a28`) que es la que hace la syscall. El `eip` que apila el hardware, `0x800a2a`, es la direccion de la instruccion que sigue a la `int` o sea a donde se vuelve despues de la syscall. Lo que vemos en el stack es lo que apilo la CPU al tomar esa interrupcion.

**Entrada a `_alltraps`** (solo para la syscall, vector 48)

```
(gdb) undisplay
Delete all auto-display expressions? (y or n) y
(gdb) b _alltraps if *(int*)$esp == 48
Breakpoint 2 at 0xf0104178: file kern/trapentry.S, line 101.
(gdb) c
Continuing.
=> 0xf0104178 <_alltraps>:      push   %ds

Breakpoint 2, _alltraps () at kern/trapentry.S:101
101             pushl %ds
(gdb) x/7wx $esp
0xefffffe4:     0x00000030      0x00000000      0x00800a2a      0x0000001b
0xeffffff4:     0x00000292      0xeebfdf98      0x00000023
(gdb) info registers
eax            0x2      2
ecx            0x0      0
edx            0x0      0
ebx            0x0      0
esp            0xefffffe4       0xefffffe4
ebp            0xeebfdfc0       0xeebfdfc0
esi            0x0      0
edi            0x0      0
eip            0xf0104178       0xf0104178 <_alltraps>
eflags         0x92     [ AF SF ]
cs             0x8      8
ss             0x10     16
ds             0x23     35
es             0x23     35
fs             0x23     35
gs             0x23     35
```
Cuando hacemos la syscall entramos a `_alltraps` por la interrupcion 48. El hardware ya cambio de ring y de stack por eso `cs=0x8`, `ss=0x10` y el stack que vemos es el del kernel. Arriba de todo estan `trapno=0x30` y `err=0` (los pone la macro `TRAPHANDLER_NOEC`) y despues `eip`, `cs=0x1b`, `eflags`, `esp` y `ss=0x23` del proceso de usuario, que los apila el hardware. En `eax` esta el numero de la syscall (2, que es `SYS_getenvid` que es la primera que hace el programa).

**Después de `push %ds`**

```
(gdb) display/i $eip
3: x/i $eip
=> 0xf0104178 <_alltraps>:      push   %ds
(gdb) stepi
=> 0xf0104179 <_alltraps+1>:    push   %es
_alltraps () at kern/trapentry.S:102
102             pushl %es
3: x/i $eip
=> 0xf0104179 <_alltraps+1>:    push   %es
(gdb) info registers
eax            0x2      2
ecx            0x0      0
edx            0x0      0
ebx            0x0      0
esp            0xefffffe0       0xefffffe0
ebp            0xeebfdfc0       0xeebfdfc0
esi            0x0      0
edi            0x0      0
eip            0xf0104179       0xf0104179 <_alltraps+1>
eflags         0x92     [ AF SF ]
cs             0x8      8
ss             0x10     16
ds             0x23     35
es             0x23     35
fs             0x23     35
gs             0x23     35
```
`push %ds` baja `esp` 4 bytes (`0xefffffe0`) y deja el `ds` de usuario (`0x23`) arriba de todo. `ds` y `es` todavia son los de usuario.

**Antes de `call trap`**

```
(gdb) stepi 6
=> 0xf0104184 <_alltraps+12>:   call   0xf0103fb8 <trap>
_alltraps () at kern/trapentry.S:128
128             call trap
3: x/i $eip
=> 0xf0104184 <_alltraps+12>:   call   0xf0103fb8 <trap>
(gdb) info registers
eax            0x10     16
ecx            0x0      0
edx            0x0      0
ebx            0x0      0
esp            0xefffffb8       0xefffffb8
ebp            0xeebfdfc0       0xeebfdfc0
esi            0x0      0
edi            0x0      0
eip            0xf0104184       0xf0104184 <_alltraps+12>
eflags         0x92     [ AF SF ]
cs             0x8      8
ss             0x10     16
ds             0x10     16
es             0x10     16
fs             0x23     35
gs             0x23     35
(gdb) x/18wx $esp
0xefffffb8:     0xefffffbc      0x00000000      0x00000000      0xeebfdfc0
0xefffffc8:     0xefffffdc      0x00000000      0x00000000      0x00000000
0xefffffd8:     0x00000002      0x00000023      0x00000023      0x00000030
0xefffffe8:     0x00000000      0x00800a2a      0x0000001b      0x00000292
0xeffffff8:     0xeebfdf98      0x00000023
```
`_alltraps` apilo `ds`, `es` y los 8 registros generales (`pusha`) cargo `0x10` (`GD_KD`) en `ds` y `es` y apilo `esp` que es el puntero al trapframe y el argumento de `trap`. `eax` vale `0x10` porque se uso para cargar los segmentos pero el valor que tenia (2) quedo guardado en el trapframe. En el stack se ve primero el puntero `tf` (`0xefffffbc`) y despues las 17 palabras del `struct Trapframe` con todo el estado del proceso de usuario.

**En `trap`**

```
(gdb) b trap
Breakpoint 3 at 0xf0103fc0: file kern/trap.c, line 272.
(gdb) c
Continuing.
=> 0xf0103fc0 <trap+8>: cld    

Breakpoint 3, trap (tf=0xefffffbc) at kern/trap.c:272
272             asm volatile("cld" ::: "cc");
3: x/i $eip
=> 0xf0103fc0 <trap+8>: cld    
(gdb) bt
#0  trap (tf=0xefffffbc) at kern/trap.c:272
#1  0xf0104189 in _alltraps () at kern/trapentry.S:128
#2  0xefffffbc in ?? ()
Backtrace stopped: previous frame inner to this frame (corrupt stack?)
```
`trap` recibe `tf=0xefffffbc` que es el mismo puntero. El `bt` muestra que la llamo `_alltraps`. El tercer frame que muestra GDB no sirve porque `_alltraps` esta en assembler y GDB no puede seguir recorriendo el stack.

---

## Scheduler con _round robin_ y _timer_

### Suspensión y despertar de environments

Implementamos la lógica necesaria para que un environment pueda suspender su ejecución durante una cantidad determinada de ticks y volver a ejecutarse cuando se cumpla ese tiempo.

**Syscall `sys_sleep`.** Cuando un proceso solicita dormir durante `n` ticks, se calcula el momento en que debe despertarse a partir del contador global `ticks`. Ese instante se guarda en `env_sleep_until`, y el environment deja de estar disponible para ser elegido por el scheduler. Como la syscall suspende al proceso, el kernel debe devolver el control al mecanismo de planificación en lugar de continuar con su ejecución.

**Despertar de los procesos.** En cada interrupción del timer se actualiza el contador `ticks` y se revisan los environments que están dormidos. Cuando pasa el tiempo y se llega al limite definido, el proceso vuelve a estar diisponible para que lo puedan usar. De esta manera, el timer permite que los procesos se despierten sin necesidad de que otro proceso los active explícitamente.

**Inicialización de `env_sleep_until`.** El arreglo `envs` reutiliza las estructuras de los environments que terminaron. Por eso, al crear uno nuevo en `env_alloc`, es necesario inicializar `env_sleep_until`: de lo contrario, podría conservar el valor de una ejecución anterior y presentar un tiempo de despertar incorrecto.

**Generación de preemption con el timer.**

La preemption es la interrupción de un environment para poder ceder el procesador a otros environments una vez que se agotó el tiempo de uso dado. Para esto, el timer es el encargado de generar una interrupción de hardware periódica (IRQ_TIMER) mientras se ejecuta el environment. La CPU interrumpe el flujo normal y transfiere el control al kernel, que guarda el contexto necesario para poder retomar la ejecución posteriormente. La interrupción llega al handler de traps y se procesa en trap_dispatch. En el caso IRQ_TIMER, se actualizan los ticks y se despiertan los environments cuyo tiempo de espera terminó. Luego se llama a sched_yield, que busca otro environment ejecutable. Finalmente, mediante env_run y el mecanismo de cambio de contexto, el kernel restaura el contexto del environment elegido y vuelve a ejecutarlo.

Con preemption activada, el padre logra recuperar la CPU y mostrar `Killing the child...`, aunque el hijo ejecute un bucle infinito sin ceder voluntariamente el procesador. Esto demuestra que la interrupción periódica del timer permite al kernel invocar `sched_yield` y cambiar de environment.

Al comentar la llamada a `sched_yield` del caso `IRQ_TIMER`, el hijo queda ejecutándose indefinidamente y el padre no logra continuar. La comparación muestra que, en esta prueba, la preemption del timer es la que permite recuperar el control de la CPU sin cooperación del hijo.


### Momento en que sched_yield es llamado desde el handler de timer y no desde una syscall

Para lograr que el sched_yield sea llamado desde el timer: en init.c se reemplazo ENV_CREATE(user_hello, ENV_TYPE_USER) por ENV_CREATE(user_spin, ENV_TYPE_USER) que consiste en un bucle infinito que no realiza ninguna llamada al sistema lo que garantiza que el proceso solo pueda ceder la CPU al ser interrumpido forzosamente por el hardware.

Luego se desactivó el arranque manual (env_run) y se habilitó la llamada a sched_yield() para permitir que el ciclo natural del sistema operativo inicie.

Luego con la terminal dividida se ejecutaron los siguientes comandos:

**Primera Terminal**

```
root@798bc0689e83:/sched# make qemu-nox-gdb
+ cc kern/init.c
+ ld obj/kern/kernel
***
*** Now run 'make gdb'.
***
qemu-system-i386 -nographic -serial mon:stdio -gdb tcp:127.0.0.1:25000 -D qemu.log -smp 1,sockets=1,cores=1  -d guest_errors -kernel obj/kern/kernel -S
6828 decimal is 15254 octal!
Physical memory: 131072K available, base = 640K, extended = 130432K
check_page_free_list() succeeded!
check_page_alloc() succeeded!
check_page() succeeded!
check_kern_pgdir() succeeded!
check_page_free_list() succeeded!
check_page_installed_pgdir() succeeded!
SMP: CPU 0 found 1 CPU(s)
enabled interrupts: 1 2
[00000000] new env 00001000
I am the parent.  Forking the child...
[00001000] new env 00001001
I am the parent.  Running the child...
```

**Segunda Terminal**

```
root@798bc0689e83:/sched# make gdb
gdb -q -s obj/kern/kernel -ex 'target remote 127.0.0.1:25000' -n -x .gdbinit
Reading symbols from obj/kern/kernel...done.
Remote debugging using 127.0.0.1:25000
0x0000fff0 in ?? ()
(gdb) b sched_yield
Breakpoint 1 at 0xf0104448: file kern/sched.c, line 268.
(gdb) c
Continuing.
The target architecture is assumed to be i386
=> 0xf0104448 <sched_yield>:    push   %ebp

Breakpoint 1, sched_yield () at kern/sched.c:268
268     {
(gdb) bt
#0  sched_yield () at kern/sched.c:268
#1  0xf0100239 in i386_init () at kern/init.c:92
#2  0xf0100047 in relocated () at kern/entry.S:86
(gdb) c
Continuing.
=> 0xf0104448 <sched_yield>:    push   %ebp

Breakpoint 1, sched_yield () at kern/sched.c:268
268     {
(gdb) bt
#0  sched_yield () at kern/sched.c:268
#1  0xf0103fe3 in trap_dispatch (tf=0xf02e6000) at kern/trap.c:253
#2  0xf0104135 in trap (tf=0xefffffbc) at kern/trap.c:315
#3  0xf01041fd in _alltraps () at kern/trapentry.S:128
#4  0xefffffbc in ?? ()
Backtrace stopped: previous frame inner to this frame (corrupt stack?)
```
Esto demuestra el salto limpio desde el manejador de interrupciones (trap_dispatch) hacia sched_yield, sin la intervención de ninguna syscall.

---

## Scheduler con _prioridades_

**Rango de prioridades.** Van de 0 a 15 (mayor numero = mas importante) y un proceso nuevo arranca con 8. Son numeros chicos y nunca negativos porque sys_getpriority devuelve la prioridad por el mismo lugar por donde devuelve los errores, que son negativos: si la prioridad pudiera ser negativa, no se podria distinguir una prioridad de un error.

**La funcion de aging.** Cada proceso tiene una prioridad base (la que le pone el usuario, y que nunca cambia) y un contador de cuantas veces el scheduler eligio a otro proceso mientras el esperaba listo. La prioridad con la que se decide es:

prioridad efectiva = prioridad base + (decisiones esperando / 8)

Es decir, cada 8 decisiones que un proceso espera sin que lo elijan, sube un nivel. Cuando lo eligen, el contador vuelve a cero y vuelve a su prioridad normal. Si dos procesos empatan, se desempata por turnos (round robin).

Elegi 8 porque tiene que cumplir dos cosas a la vez. Si fuera muy chico (por ejemplo 1), la prioridad casi no importaria, porque cualquiera alcanzaria enseguida al de arriba y el scheduler se pareceria a un round robin. Si fuera muy grande, un proceso de prioridad baja esperaria demasiado antes de tener su oportunidad. Con 8, la prioridad base manda en el corto plazo y la espera igual tiene un limite corto.

**Peor caso de espera.** Ojo: en este informe aparecen dos cuentas con la misma formula, y son dos preguntas distintas.

- El peor caso general es el que se pide aca: cuanto puede esperar como maximo un proceso de prioridad p hasta que lo elijan. Es 8 x (15 - p + 1) decisiones, mas una por cada otro proceso que compita con el. El peor de todos es p = 0: 8 x 16 = 128 decisiones mas la cantidad de otros procesos. Son las decisiones que necesita un proceso de prioridad 0 para superar a CUALQUIER otro, aunque tenga la prioridad base maxima (15).
- Mas abajo aparece otra cuenta, 8 x (10 - 6) = 32. Esa es solo lo que tarda un proceso de prioridad 6 en alcanzar a UN rival puntual de prioridad 10 (lo que pasa en nuestro test). Es la misma formula, pero cambia contra quien se compara.

Esto alcanza para descartar la starvation porque el numero es finito, sin importar cuantos procesos de prioridad alta haya. Pasada esa cantidad de decisiones, el proceso que espera supera a cualquiera que acaba de ser elegido. Los procesos que ya fueron elegidos vuelven a empezar de cero, asi que no pueden volver a pasarlo, y los procesos nuevos tampoco, porque nacen sin espera acumulada. Siempre llega su turno.

**Un proceso de baja prioridad obtiene CPU (con y sin aging).** Armamos un test (agingtest) con dos procesos que hacen cuentas sin parar y nunca ceden la CPU: ALTA (prioridad 10) y BAJA (prioridad 6).

Con aging: ALTA arranca, pero BAJA consigue la CPU en el tick 50, unas 32 decisiones despues, que es lo que predice la formula (8 x (10 - 6) = 32). BAJA corre y termina antes que ALTA, que termina en el tick 156. El historial del scheduler lo muestra:

  tick 1: [00001001] base 10, efectiva 10
  tick 50: [00001002] base 6, efectiva 10
  tick 50: [00001001] base 10, efectiva 10

En el tick 50 el proceso 1002 (BAJA) fue elegido con base 6 y efectiva 10: subio cuatro niveles esperando hasta empatar a ALTA.

Sin aging (compilando con NO_AGING=1): BAJA no corre hasta que ALTA termina todos sus bloques. Obtiene la CPU recien en el tick 157. Es la starvation.

Tambien lo muestra el contador de las estadisticas "decisiones en las que el aging cambio al proceso elegido": 3 con aging y 0 sin aging.

**Distribucion de CPU por nivel de prioridad.** En la corrida de agingtest con aging, las estadisticas muestran:

Ticks de CPU por nivel de prioridad base:
  prioridad 10: 155 ticks
  prioridad 8: 1 ticks

El nivel 10 (ALTA) se llevo casi toda la CPU: 155 de los 156 ticks que duro la corrida. El nivel 8 es el proceso padre, que solo se despierta un momento para vigilar. BAJA (nivel 6) no aparece porque su trabajo es tan corto que ningun tick del timer la encontro corriendo: obtuvo la CPU, pero por menos de un tick. En priotest con prioridades se ve el reparto entre varios niveles: prioridad 8: 2 ticks, prioridad 7: 1, prioridad 5: 1 y prioridad 3: 1.

**Herencia de prioridad en fork.** El hijo hereda exactamente la prioridad del padre. Razonamiento: el hijo nace con la misma importancia que el padre, y despues el padre puede ajustarsela con sys_setpriority. Si heredara solo una fraccion, cada generacion de hijos iria perdiendo peso sin ningun motivo. Reglas: un proceso no puede subir su propia prioridad (si no, todos se pondrian el maximo) pero si bajarla, y a sus hijos directos les puede poner cualquier valor valido.

**Comparacion entre USE_RR y USE_PR (mismo conjunto de procesos: priotest).** Corrimos priotest con las dos politicas. Estas son las estadisticas:

Con round robin (USE_RR=1):

  Llamadas al scheduler: 35
  Tiempo total de ejecucion: 4 ticks
  Ejecuciones por proceso:
    [00001000] 9 veces
    [00001001] 6 veces
    [00001002] 7 veces
    [00001003] 6 veces
    [00001004] 6 veces

Con prioridades (USE_PR=1):

  Llamadas al scheduler: 33
  Tiempo total de ejecucion: 5 ticks
  Ejecuciones por proceso:
    [00001000] 6 veces
    [00001001] 7 veces
    [00001002] 6 veces
    [00001003] 7 veces
    [00001004] 6 veces

Los totales son casi iguales (35 contra 33 llamadas, y 4 contra 5 ticks; un tick de diferencia es ruido de la corrida). Es esperable, porque el trabajo que hacen los procesos es el mismo. Lo que cambia es el orden en que terminan:

- Con round robin los cuatro hijos avanzan una iteracion por vuelta, sin importar su prioridad, y no terminan en ningun orden especial: el de prioridad 5 termina antes que el de 6.
- Con prioridades, los hijos terminan en orden descendente: primero el de 7, despues 6, 5 y 4 (con un poco de intercalado en los bordes, que es el aging actuando).

Las estadisticas de prioridades (historial de procesos elegidos, ticks por nivel y decisiones en las que el aging cambio el resultado) solo las reporta el scheduler por prioridades, porque en round robin no existe el concepto de nivel.

Ese poder de elegir tiene un costo: el scheduler por prioridades mira a todos los procesos en cada decision para encontrar al mejor, y por eso es mas lento cuando hay muchos. En el test primes tarda unos 13 segundos con prioridades contra unos 5 con round robin.

---

## Threads

### Diferencia entre el page directory en `sys_exofork` y `sys_thread_create`

El page directory es la tabla que usa un proceso para saber donde esta cada direccion en la memoria fisica. Si dos environments tienen el mismo page directory ven la misma memoria. Si tienen distintos cada uno ve la suya.

**En `sys_exofork`**

`sys_exofork` llama a `env_alloc` y esta llama a `env_setup_vm`. Esa funcion reserva una pagina nueva para el page directory del hijo y le copia la parte del kernel. La parte de usuario queda vacia. El `pp_ref` de la pagina empieza en 1 porque la usa un solo environment. Despues la memoria del hijo se arma desde espacio de usuario con `fork` o `dumbfork`. Como son dos page directory distintos si el hijo cambia una variable el padre no se entera. Con copy on write pasa lo mismo: al principio apuntan a las mismas paginas fisicas pero cuando uno escribe se queda con su copia.

**En `sys_thread_create`**

`sys_thread_create` tambien llama a `env_alloc` y `env_setup_vm` reserva un page directory nuevo. El thread no lo necesita asi que lo liberamos con `page_decref`. Despues hacemos que el `env_pgdir` del thread apunte al mismo que el del proceso que lo crea. No se copia ninguna pagina. Tambien le subimos el `pp_ref` porque ahora lo usa un environment mas. Es parecido a lo que pasa con los file descriptors cuando dos procesos abren el mismo archivo. Como la memoria es la misma si un thread cambia una variable global los otros la ven.

**Que es propio de cada thread y que se comparte**

Cada thread tiene sus propios registros (`env_tf`) y su propio stack. Tambien tiene su `env_id` y su estado. Comparte con el proceso el page directory y todo lo que esta ahi. Eso incluye el codigo y los datos. Tambien el heap y las variables globales. Tiene el mismo `env_parent_id` que el proceso.

**Liberación de environments (`env_free`).** 

Modificamos `env_free` para contemplar que varios threads pueden compartir el mismo directorio de páginas. Cuando se destruye un proceso principal, primero se buscan y destruyen los threads que comparten su espacio de direcciones. Luego, mediante el contador de referencias `pp_ref`, se verifica si el directorio de páginas es utilizado por otros environments: únicamente el último que lo utiliza libera las tablas de páginas y desmapea las páginas de usuario. Finalmente, cada environment libera su propia referencia al directorio de páginas y vuelve a la lista de environments disponibles.

---

### Condición de carrera al modificar la misma variable

Cuando dos threads modifican una variable global simultáneamente sin mecanismos de sincronización, se produce una condición de carrera. Esto ocurre porque las operaciones en C, se dividen en múltiples instrucciones de máquina.

**EJEMPLO DE CONDICIÓN DE CARRERA**

Ambos threads ejecutan shared_counter++ al mismo tiempo cuando la variable vale 0 esto significa que se realizan los siguientes tres pasos:
  * mov eax, [shared_counter] -> mueve el valor de shared_counter en memoria a un registro
  * inc eax -> incrementa el valor del registro
  * mov [shared_counter], eax -> escribe el nuevo valor en memoria

Si el scheduler intercala la ejecución justo en el medio, ocurre lo siguiente:
  * Thread A lee shared_counter y lo guarda en eax (es decir, eax = 0).
  * Thread B interrumpe y lee shared_counter también guardandolo en eax (eax = 0).
  * Thread B incrementa eax y escribe el valor (es decir, shared_counter = 1).
  * Thread A retoma su ejecución, incrementa su registro local (eax = 1) y escribe el valor (shared_counter = 1).

Lo que produce dos incrementos lógicos, pero el valor final de la variable es 1 en lugar de 2. Y esto se traduce en la perdidad de una modificación.

---

### Donde se ubican los stacks de los threads (`thread_create`)

Un thread comparte el espacio de direcciones con su proceso, pero no puede compartir la pila: cada thread tiene sus propias variables locales y sus propias direcciones de retorno. Por eso `thread_create` reserva un stack nuevo para cada thread con `sys_page_alloc`, dentro del espacio de direcciones del proceso, y despues llama a `sys_thread_create`.

**Donde se ponen.** Debajo del stack del thread principal, que ocupa `[0xeebfd000, 0xeebfe000)` (`USTACKTOP` vale `0xeebfe000`). Cada thread usa un "slot" de dos paginas (8 KB): una pagina de stack y una pagina de guarda que se deja sin mapear. Los slots se ocupan de arriba hacia abajo:

| Thread | Stack | `esp` inicial |
|---|---|---|
| 0 | `[0xeebfb000, 0xeebfc000)` | `0xeebfbff8` |
| 1 | `[0xeebf9000, 0xeebfa000)` | `0xeebf9ff8` |
| 2 | `[0xeebf7000, 0xeebf8000)` | `0xeebf7ff8` |

Entre dos stacks siempre queda una pagina sin mapear: `[0xeebfc000, 0xeebfd000)` entre el thread 0 y el stack principal, `[0xeebfa000, 0xeebfb000)` entre el 1 y el 0, y asi.

**Por que ahi.**

- Es una zona libre. Arriba estan el stack principal, el stack de excepciones y `UTOP`; abajo, muy lejos, el codigo del programa (`UTEXT`, `0x00800000`).
- Los stacks crecen hacia abajo, asi que cada thread nuevo se pone un slot mas abajo que el anterior. La direccion se calcula con un contador (`next_slot`), sin buscar paginas libres, y los rangos de dos threads nunca se solapan.
- La pagina de guarda sirve para detectar errores. Si un thread usa mas stack del que tiene, escribe en una pagina sin mapear y el kernel lo ve como un page fault. Sin guarda, se pisaria en silencio el stack del thread vecino.

**Que se escribe en el stack.** `sys_thread_create` solo fija `eip` (la funcion donde empieza el thread) y `esp` (el tope del stack); no escribe nada en la pila. Por eso `thread_create` la deja armada como si `func` hubiera sido llamada con `call`: en `[esp]` la direccion de retorno (`exit`) y en `[esp + 4]` el argumento `arg`. Cuando `func` termina con `ret`, salta a `exit()` y el thread se destruye solo; y `func` recibe `arg` como primer argumento, porque en i386 los argumentos viajan por el stack.

**Limitaciones.**

- El contador `next_slot` es una variable global del proceso, o sea que la comparten todos los threads: dos threads que llamen a `thread_create` a la vez podrian elegir el mismo slot. La consigna no pide sincronizacion, asi que no se resolvio.
- Los stacks de threads que terminaron no se reutilizan (un thread no puede liberar la pila en la que esta corriendo), por eso hay un tope de 256 threads por proceso, que ocupan unos 2 MB debajo del stack principal.
- Cada stack tiene una sola pagina (4 KB), poco para funciones con muchas variables locales o recursion profunda.

**Prueba.** Con `threadtest` se crean 3 threads (envids `1001`, `1002` y `1003`) que incrementan 5 veces cada uno una variable global compartida. Con `USE_RR=1` el contador final da 15 de 15, ningun thread produce page faults, y cada uno termina al retornar de `func` (aparece `exiting gracefully` y su `free env`). Con `USE_PR=1` el resultado fue el mismo.

---

### Costo de creación de un Proceso fork vs un Thread

La diferencia de rendimiento y uso de memoria radica en cómo se maneja el espacio de direcciones virtuales (env_pgdir).

**Creación de un proceso fork:**

  * Se asigna una página física nueva exclusivamente para el Page Directory del proceso hijo.
  * El kernel recorre todo el user space del proceso padre e inicializa los mapeos para el hijo además de asignar múltiples páginas nuevas para almacenar las Page Tables del hijo.
  * Si la implementación utiliza Copy-on-Write (COW), las páginas de datos físicas no se copian inmediatamente (se mapean como solo lectura). Si es un fork simple (dumbfork), se realiza una copia física completa (memcpy) de todas las páginas de código, datos y pila.

**Creación de un thread:**

  * No se copian paginas. El thread simplemente recibe un puntero al mismo Page Directory de su proceso creador. Al compartir el mismo directorio, comparten automáticamente todas las Page Tables y las páginas de datos físicas asociadas.
  * La única memoria que se debe asignar e inicializar para un nuevo thread es un espacio independiente para su pila (stack), de modo que pueda tener sus propias variables locales y registro de llamadas sin pisarse con los demás threads.

Por todo esto, crear un thread es una operación mucho más rápida para el sistema operativo.

## Uso de IA

Usamos la IA para:
- Solucionar los problemas que tuvimos con clang format
- Solucionar errores con Docker, QEMU y GDB porque estuvimos mucho tiempo tratando de resolverlos y verificar los comandos de las dos terminales para no equivocarnos
- Interpretar las salidas de GDB
- Darle formato al sched.md para que se vea mas prolijo

---

