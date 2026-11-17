# shell

## Búsqueda en $PATH

La syscall execve(2) es la que habla con el kernel para reemplazar el código, los datos y el stack del proceso actual por el programa nuevo que se le indique. Necesita que se le pase de forma explícita la ruta completa del binario, el array de argumentos y el array de variables de entorno.

En cambio la familia de exec(3) son funciones de la librería estándar de C (execl, execlp, execle, execv, execvp, execvpe) y no syscalls. Son wrappers que por dentro terminan llamando a execve(2) pero cada una agrega alguna comodidad antes de esa llamada final. Algunas buscan el binario en las rutas de $PATH en vez de necesitar la ruta completa (las que tienen p), algunas reciben los argumentos como array en vez de como lista separada por comas (las que tienen v) o al revés (las que tienen l), y algunas dejan pasar un entorno custom en vez de heredar automáticamente el del proceso que las llama (las que tienen e).

Si, exec(3) puede fallar. Por ejemplo si el comando no existe, no está en $PATH o no se tienen permisos de ejecución sobre el binario. Cuando falla, a diferencia del caso exitoso, execvp retorna en vez de reemplazar la imagen del proceso: devuelve -1 y setea errno indicando la causa del error.

En nuestra implementación execvp corre siempre dentro del proceso hijo así que si retorna significa que falló. Ante eso se imprime el error por stderr con fprintf mostrando el nombre del comando y el mensaje asociado a errno mediante strerror y luego se corta la ejecución del hijo con _exit(-1) para que no siga ejecutando código que en realidad pertenece a la shell. El proceso padre mientras tanto sigue con su flujo normal y se entera de que algo falló a través del código de salida que recupera con waitpid.

---


## Flujo estándar

2>&1 quiere decir que se manda el error estándar, stderr, al mismo lugar en donde en ese momento está yendo la salida estándar, stdout. O sea que se redirige lo que sale por el file descriptor 2 hacia donde sea que esté apuntando el file descriptor 1.

Si en la terminal se pone >out.txt 2>&1 cat out.txt el error se manda al mismo lugar que la salida estandar, que ya estaba redirigida al archivo.
Cuando se invierte el orden, es decir 2>&1 >out.txt cat out.txt, primero se procesa 2>&1 y el fd 2 queda apuntando a donde apuntaba el fd 1 en ese momento, que es la terminal. Después se procesa el >out.txt y el fd 1 pasa a apuntar al archivo, pero esto no afecta al fd 2 porque ya había quedado fijado antes. Entonces cuando finalmente se ejecuta cat, el fd 1 apunta a out.txt y el fd 2 quedó apuntando a la terminal.

Se probaron ambos casos en una terminal bash real vía WSL. Pasó exactamente lo mismo que lo explicado recién. Con >out.txt 2>&1 tanto la salida como el error terminaron dentro del archivo y con 2>&1 >out.txt el error se imprimió directamente en pantalla mientras que out.txt solo tuvo lo que el comando imprime cuando todo sale bien, sin el mensaje de error. 

Después probamos los dos casos en nuestra propia shell. Ahí el resultado fue distinto al de bash. Con >out.txt 2>&1 pasó lo mismo. El error y la salida terminaron los dos dentro del archivo. Pero con 2>&1 >out.txt no cambió nada. El archivo quedó exactamente igual que en el caso anterior con el error adentro también en vez de imprimirse en pantalla como pasa en bash.

Esto pasa porque en nuestra implementación el orden en que se escriben las redirecciones no importa. Cuando se parsea la línea cada redirección se guarda en un campo separado uno para la entrada uno para la salida y uno para el error sin importar en qué posición apareció cada una. Y despues al ejecutar el comando siempre se aplican en el mismo orden fijo primero la entrada despues la salida despues el error. Entonces no hay forma de que el orden en que las escribimos en la línea cambie el resultado como sí pasa en bash donde cada redirección se procesa en el momento exacto en que aparece.

---
## Tuberías múltiples

*Que ocurre con el exit code reportado por la shell si se ejecuta un pipe? Cambia en algo?*

Si, cambia: en bash el exit code de un pipe es el del ultimo comando (true | false -> $?=1, false | true -> $?=0). En la shell, en cambio, el exit code de un pipe nunca se reporta, pase lo que pase adentro. Esto pasa porque el proceso que arma el pipe, en exec_cmd() (exec.c), descarta el status de sus dos hijos con waitpid(pid, NULL, 0) y siempre termina con exit(0); ademas printstatus-info() (printstatus.c) directamente no imprime nada cuando el comando es de tipo PIPE.

Evidencia:

$ true | false
$
$ /bin/false
	Program: [/bin/false] exited, status: 1

El pipe no muestra nada, es simple. Para que fuera como bash habria que propagar el exit code real del ultimo comando en vez de siempre devolver 0.

*Que ocurre si, en un pipe, alguno de los comandos falla?*

El pipe sigue funcionando normal: el comando que si pudo correr se ejecuta igual, es decir recibiendo lo que le haya llegado, que puede ser nada; y ninguno de los dos frena al otro. Es igual que en bash:

$ ls /noexiste | wc -l
ls: cannot access '/noexiste': No such file or directory
0

La unica diferencia real es que en bash podes consultar el exit code del ultimo comando con $?, y en esta  shell no.


---

## Variables de entorno temporarias

Cada proceso tiene un array de strings que se llama environment, en donde cada uno de los strings es una definición del estilo nombre=valor. Cuando se hace un fork, el proceso hijo obtiene una copia del código, las variables, el heap, el stack y entre ellos, esta lista del proceso padre, lo que me permite poder hacer modificaciones sin afectar al otro proceso. 
Si las variables se setearan antes del fork, todavía habría una sola copia compartida y esa modificación quedaría pegada a la shell de forma permanente, afectando a todos los comandos ejecutados después. EN cambio al hacerlo en el hijo se modifica únicamente su copia, que desaparece junto con el proceso cuando este termina, permitiendo que las variables sean realmente temporales.

Las funciones de exec que tienen una e al final no tienen el mismo funcionamiento que setenv. En el caso del exec al cual le puedo mandar un tercer argumento (sea una lista o no), cuando se ejecute el exec se va a pisar la lista de variables de entorno que existia antes y se va a crear una nueva que contenga ese argumento extra que se le pasó al comando. Para el caso de setenv solo se agrega la nueva variable que se quiere al final de la lista y, por lo tanto, se mantienen las variables base como lo son PATH o HOME.

La posible implementación podría ser utilizar la variable environ (variable global en donde se guardan todas las variables de entorno, como el array environment mencionado anteriormente) y recorrerla hasta toparnos con un NULL para saber cuantas variables se tienen actualmente. A partir de eso reservar un espacio más grande de memoria (teniendo en cuenta esa cantidad, la cantidad de variables que se quieren agregar, y el  espacio para el null). Una vez hecha esa reserva de espacio, copiar el contenido de environ al nuevo arreglo, agregar las nuevas variables y el null al final y pasar ese arreglo ya completo al comando exec.
---

## Pseudo-variables

El propósito de la pseudo-variable '$?' es almacenar el código de estado de salida del último comando que se ejecutó en la shell. En los sistemas UNIX sirve para avisar si un programa finalizó correctamente o no.
Si el valor de '?' es 0 significa que el programa terminó sin errores y un valor distinto de 0(como por ejemplo, 1) quiere decir que el comando falló que se encontró un error.

Luego otras 3 variables mágicas estándar son:

	- '$$'(PID del proceso actual): su proposito es expandir al identificador de proceso (ProcessID) de la shell actual que esta ejecutando el comando o script. 
	Ejemplo de uso: $ echo "El PID de esta terminal es: $$". O directamente: $ echo $$.

	
	- '$!'(PID del último proceso en segundo plano): su proposito es contener el ProcessID del trabajo ejecutado más recientemente en segundo plano. Es útil para rastrear procesos asíncronos.
	Ejemplo de uso: Si quiero ejecutar el proceso sleep (pausar el sistema) durante 5 segundos -> $ sleep 5 &
					(El ampersand final es para ejecutar el proceso en segundo plano pero sin congelar la terminal).
					Mientras tanto -> $ echo "El proceso sleep está corriendo en segundo plano con PID: $!".


	'$#' (Cantidad de argumentos posicionales): su proposito es expandir el número que representa la cantidad de argumentos que se le pasaron a un script de bash o a una función dentro del mismo.
	Ejemplo de uso: # Si tengo un script llamado script.sh que contiene "echo $#" -> $ ./script.sh arg1 arg2.

## Comandos built-in

El comando pwd podría implementarse sin ser built-in ya que no modifica ningún atributo del proceso que lo ejecuta (solo lee y muestra información) y ese resultado persiste sin importar si el proceso que lo generó ya terminó. Sin embargo, se implementa como built-in para que sea más eficiente, evita el costo de crear un proceso nuevo, hacer la busqueda y la espera del proceso hijo.
Sabiendo esto podemos decir que cd tiene que implementarse como un built in porque si el cambio es generado en un proceso hijo, el padre no tiene forma de poder saber ese cambio hecho y al terminarse el proceso hijo, se perdería ese cambio de directorio hecho.

---

## Procesos en segundo plano

Para poder detectar cuando termina un proceso que se esta ejecutando en segundo plano, se utilizo la señal 'SIGCHLD'.

Primero, se configuro un manejador para esta señal. Tambien se utilizaron las opciones 'SA_RESTART' y 'SA_ONSTACK': la primera permite que ciertas operaciones se reanuden automaticamente si son interrumpidas por una señal, y la segunda permite que el manejador utilice una pila separada.

Para diferenciar los procesos de primer plano de los de segundo plano, se utilizaron grupos de procesos. Los procesos de primer plano se separan del grupo de la shell, mientras que los de segundo plano permanecen en el. De esta manera, el manejador puede ocuparse unicamente de los procesos de segundo plano.

Ademas, la señal se bloquea mientras se crea el proceso y se define a que grupo pertenece. Esto evita que un proceso termine demasiado rapido y sea atendido por el manejador antes de que la shell pueda esperarlo correctamente.

El manejador utiliza 'WNOHANG' para realizar una espera no bloqueante. Esto significa que, si en ese momento no hay ningun proceso de segundo plano que haya terminado, el manejador continua sin quedarse esperando. La espera se realiza dentro de un ciclo porque pueden terminar varios procesos de segundo plano casi al mismo tiempo y una sola señal puede avisar por mas de uno.

### ¿Por que es necesario el uso de señales?

Las señales permiten que el sistema operativo avise a la shell cuando un proceso termina, sin que la shell tenga que estar preguntando constantemente si ya termino.

Sin señales, la shell tendria que revisar periodicamente el estado de los procesos de segundo plano, lo que consumiria recursos y podria hacer que la finalizacion se detectara con demora.

	
---

## Uso de IA

Usamos la IA para solucionar los problemas que tuvimos con clang format y para que nos haga una revisión del código y vea si teníamos errores antes de entregar y también para debuggear

---

