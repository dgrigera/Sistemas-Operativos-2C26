#!/bin/bash
# pruebas de persistencia. se puede correr desde cualquier lado
cd "$(dirname "$0")/.." || exit 1

FS_BIN=./fisopfs
MNT=$(mktemp -d)
# archivos temporales para no ensuciar el repo
DISK=/tmp/fisopfs_persist_$$.fisopfs
DISK_ROTO=/tmp/fisopfs_roto_$$.fisopfs
DISK_REL=persist_rel_$$.fisopfs
TMPBIN=/tmp/fisopfs_persist_bin_$$
PASS=0
FAIL=0

# al terminar desmonto y borro todo lo que cree
cleanup() {
    fusermount -u "$MNT" 2>/dev/null
    rmdir "$MNT" 2>/dev/null
    rm -f "$DISK" "$DISK_ROTO" "$DISK_REL" "$TMPBIN"
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

# monta usando el archivo que le paso por parametro
montar() {
    $FS_BIN "$MNT" --filedisk "$1" -o attr_timeout=0 || { echo "no se pudo montar"; exit 1; }
    sleep 1
}

# desmonta y espera un poco a que termine de guardar
desmontar() {
    fusermount -u "$MNT"
    sleep 1
}

# --- guardar y recuperar ---
# creo un par de cosas antes de desmontar
montar "$DISK"
echo "hola persistencia" > "$MNT/hola.txt"
head -c 1000 /dev/urandom > "$TMPBIN"
cp "$TMPBIN" "$MNT/bin"
ln "$MNT/hola.txt" "$MNT/copia.txt" 2>/dev/null
M1=$(stat -c %Y "$MNT/hola.txt" 2>/dev/null)
desmontar
check "al desmontar se crea el archivo de persistencia" "si" "$([ -f "$DISK" ] && echo si || echo no)"

# vuelvo a montar y miro si sigue todo
montar "$DISK"
check "el contenido sobrevive al remontar" "hola persistencia" "$(cat "$MNT/hola.txt" 2>/dev/null)"
check "el contenido binario sobrevive" "iguales" "$(cmp -s "$TMPBIN" "$MNT/bin" && echo iguales || echo distintos)"
check "el mtime se conserva" "$M1" "$(stat -c %Y "$MNT/hola.txt" 2>/dev/null)"
check "el nlink se conserva" "2" "$(stat -c %h "$MNT/hola.txt" 2>/dev/null)"
check "el hard link sobrevive" "hola persistencia" "$(cat "$MNT/copia.txt" 2>/dev/null)"

# --- los borrados tambien se guardan ---
rm "$MNT/copia.txt" 2>/dev/null
desmontar
montar "$DISK"
check "borrar un link persiste" "1" "$(stat -c %h "$MNT/hola.txt" 2>/dev/null)"
rm "$MNT/hola.txt" 2>/dev/null
desmontar
montar "$DISK"
check "un archivo borrado sigue borrado" "no" "$([ -e "$MNT/hola.txt" ] && echo si || echo no)"
desmontar

# --- archivo de persistencia invalido ---
# le meto texto cualquiera para que no tenga el formato de fisopfs
echo "esto no es un fisopfs" > "$DISK_ROTO"
montar "$DISK_ROTO"
check "con archivo invalido monta igual" "directory" "$(stat -c %F "$MNT" 2>/dev/null)"
echo "nuevo" > "$MNT/nuevo.txt"
check "con archivo invalido arranca vacio y se puede escribir" "nuevo" "$(cat "$MNT/nuevo.txt" 2>/dev/null)"
desmontar

# --- ruta relativa en background ---
# fuse hace chdir a / asi que la ruta tiene que convertirse a absoluta
montar "$DISK_REL"
echo "relativo" > "$MNT/rel.txt"
desmontar
check "ruta relativa guarda en el directorio donde se lanzo" "si" "$([ -f "$DISK_REL" ] && echo si || echo no)"
montar "$DISK_REL"
check "ruta relativa recupera el contenido" "relativo" "$(cat "$MNT/rel.txt" 2>/dev/null)"

echo
echo "Resultado: $PASS OK, $FAIL FAIL"
[ "$FAIL" -eq 0 ]