#!/bin/bash
# pruebas de soft links. se puede correr desde cualquier lado
cd "$(dirname "$0")/.." || exit 1

FS_BIN=./fisopfs
MNT=$(mktemp -d)
# archivo de persistencia temporal para no ensuciar el repo
DISK=/tmp/fisopfs_softlinks_$$.fisopfs
PASS=0
FAIL=0

# al terminar desmonto y borro todo lo que cree
cleanup() {
    fusermount -u "$MNT" 2>/dev/null
    rmdir "$MNT" 2>/dev/null
    rm -f "$DISK"
}
trap cleanup EXIT

# check "descripcion" "esperado" "obtenido"
check() {
    if [ "$2" == "$3" ]; then
        echo "[OK]   $1"
        PASS=$((PASS + 1))
    else
        echo "[FAIL] $1"
        echo "       esperado: $2"
        echo "       obtenido: $3"
        FAIL=$((FAIL + 1))
    fi
}

montar() {
    $FS_BIN "$MNT" --filedisk "$DISK" -o attr_timeout=0 || { echo "no se pudo montar"; exit 1; }
    sleep 1
}

desmontar() {
    fusermount -u "$MNT"
    sleep 1
}

montar

# crear y leer 
echo "hola" > "$MNT/hola.txt"
ln -s hola.txt "$MNT/link" 2>/dev/null
check "ln -s crea el link" "symbolic link" "$(stat -c %F "$MNT/link" 2>/dev/null)"
check "readlink devuelve el destino" "hola.txt" "$(readlink "$MNT/link" 2>/dev/null)"
check "el tamaño del link es el largo del destino" "8" "$(stat -c %s "$MNT/link" 2>/dev/null)"
check "cat a traves del link lee el archivo" "hola" "$(cat "$MNT/link" 2>/dev/null)"

# escribir por el link
echo "chau" > "$MNT/link"
check "escribir por el link cambia el archivo" "chau" "$(cat "$MNT/hola.txt" 2>/dev/null)"

# errores
ln -s otro "$MNT/link" 2>/dev/null
check "ln -s sobre un nombre que existe falla" "1" "$?"
readlink "$MNT/hola.txt" >/dev/null 2>&1
check "readlink de un archivo comun falla" "1" "$?"
readlink "$MNT/noexiste" >/dev/null 2>&1
check "readlink de algo que no existe falla" "1" "$?"

# link roto
ln -s noexiste "$MNT/roto" 2>/dev/null
check "se puede crear un link a algo que no existe" "noexiste" "$(readlink "$MNT/roto" 2>/dev/null)"
cat "$MNT/roto" >/dev/null 2>&1
check "cat de un link roto falla" "1" "$?"

# persistencia
desmontar
montar
check "el link sobrevive al remontar" "hola.txt" "$(readlink "$MNT/link" 2>/dev/null)"
check "el link sigue siendo link" "symbolic link" "$(stat -c %F "$MNT/link" 2>/dev/null)"
check "se puede leer a traves del link despues de remontar" "chau" "$(cat "$MNT/link" 2>/dev/null)"

# borrar 
rm "$MNT/link" 2>/dev/null
check "rm borra el link" "no" "$([ -L "$MNT/link" ] && echo si || echo no)"
check "rm del link no borra el archivo" "chau" "$(cat "$MNT/hola.txt" 2>/dev/null)"

echo
echo "Resultado: $PASS OK, $FAIL FAIL"
[ "$FAIL" -eq 0 ]