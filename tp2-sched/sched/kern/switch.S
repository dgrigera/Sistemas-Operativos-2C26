/*
 * context_switch(struct Trapframe *tf)
 *
 * Recibe un puntero a un struct Trapframe (inc/trap.h) y restaura la CPU
 * exactamente al estado que ese struct describe, terminando en modo
 * usuario (ring 3). No retorna nunca: la ultima instruccion es un iret,
 * que es quien efectivamente "salta" a userspace.
 *
 * Como es una funcion en assembler llamada desde C con la convencion cdecl
 * normal (ver el prototipo en kern/env.h, sin ningun atributo especial de
 * registros), el unico argumento -el puntero tf- llega en la pila, no en
 * un registro: en 4(%esp), ya que 0(%esp) es la direccion de retorno que
 * dejo el "call".
 *
 * La idea central: en vez de ir restaurando campo por campo con mov's
 * sueltos, aprovechamos que struct Trapframe tiene EXACTAMENTE el mismo
 * layout en memoria que la pila que esperan 'popal' e 'iret'. Entonces
 * alcanza con mover %esp para que apunte al Trapframe, y dejar que esas
 * dos instrucciones hagan el trabajo pesado.
 */

.globl context_switch;
context_switch:
        movl 4(%esp), %esp
        # A partir de aca, %esp "es" el Trapframe: cada pop subsiguiente
        # lee el siguiente campo del struct, en el mismo orden en que
        # estan declarados.

        # struct PushRegs tf_regs (32 bytes): edi, esi, ebp, oesp (se
        # descarta), ebx, edx, ecx, eax. 'popal' es la contraparte exacta
        # de 'pushal' (la instruccion que arma este mismo bloque del lado
        # del trap), asi que restaura los 7 registros de proposito
        # general que importan, en el orden correcto.
        popal

        # tf_es, tf_ds (4 bytes cada uno, con padding incluido: el campo
        # real ocupa los 16 bits bajos). 'popl' sobre un registro de
        # segmento avanza %esp en 4 bytes igual, consistente con el
        # padding del struct.
        popl %es
        popl %ds

        # tf_trapno, tf_err (4 bytes cada uno): no los necesitamos para
        # volver a userspace, los saltamos sin leerlos.
        addl $0x8, %esp

        # Lo que queda arriba de la pila en este punto es EXACTAMENTE lo
        # que 'iret' espera: tf_eip, tf_cs, tf_eflags, y como vamos a
        # cruzar de ring 0 a ring 3 (tf_cs va a tener RPL=3), tambien
        # tf_esp y tf_ss. 'iret' los va a poppear todos juntos de forma
        # atomica y saltar a ese eip en modo usuario.
        iret