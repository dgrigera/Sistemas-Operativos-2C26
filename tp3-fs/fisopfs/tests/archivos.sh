#!/bin/bash
# Pruebas de archivos y hard links. Ejecutar desde cualquier lado.
cd "$(dirname "$0")/.." || exit 1

FS_BIN=./fisopfs
MNT=$(mktemp -d)
DISK=/tmp/fisopfs_test_$$.fisopfs
TMPBIN=/tmp/fisopfs_bin_$$
PASS=0
FAIL=0

cleanup() {
    fusermount -u "$MNT" 2>/dev/null
    rmdir "$MNT" 2>/dev/null
    rm -f "$DISK" "$TMPBIN"
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

$FS_BIN "$MNT" --filedisk "$DISK" -o attr_timeout=0 || { echo "no se pudo montar"; exit 1; }
sleep 1

# --- creación ---
touch "$MNT/a.txt"
check "touch crea el archivo" "si" "$([ -f "$MNT/a.txt" ] && echo si || echo no)"
check "archivo nuevo vacío" "0" "$(stat -c %s "$MNT/a.txt" 2>/dev/null)"
check "archivo nuevo tiene 1 link" "1" "$(stat -c %h "$MNT/a.txt" 2>/dev/null)"

# --- escritura y lectura ---
echo "hola" > "$MNT/a.txt"
check "echo > escribe y cat lee" "hola" "$(cat "$MNT/a.txt" 2>/dev/null)"

echo "mundo" >> "$MNT/a.txt"
check "append (>>)" $'hola\nmundo' "$(cat "$MNT/a.txt" 2>/dev/null)"

echo "x" > "$MNT/a.txt"
check "trunc (>) pisa el contenido" "x" "$(cat "$MNT/a.txt" 2>/dev/null)"
check "tamaño tras trunc" "2" "$(stat -c %s "$MNT/a.txt" 2>/dev/null)"

# --- binario ---
head -c 1000 /dev/urandom > "$TMPBIN"
cp "$TMPBIN" "$MNT/bin" 2>/dev/null
check "contenido binario idéntico" "iguales" "$(cmp -s "$TMPBIN" "$MNT/bin" && echo iguales || echo distintos)"
printf 'a\0b\0c' > "$MNT/nul"
check "bytes nulos conservan el tamaño" "5" "$(stat -c %s "$MNT/nul" 2>/dev/null)"

# --- fechas ---
M1=$(stat -c %Y "$MNT/a.txt" 2>/dev/null)
sleep 1
echo "otro" > "$MNT/a.txt"
M2=$(stat -c %Y "$MNT/a.txt" 2>/dev/null)
check "mtime se actualiza al escribir" "si" "$([ -n "$M1" ] && [ -n "$M2" ] && [ "$M2" -gt "$M1" ] && echo si || echo no)"

# --- hard links ---
echo "compartido" > "$MNT/a.txt"
ln "$MNT/a.txt" "$MNT/b.txt" 2>/dev/null
check "ln crea el segundo nombre" "compartido" "$(cat "$MNT/b.txt" 2>/dev/null)"
check "nlink pasa a 2" "2" "$(stat -c %h "$MNT/a.txt" 2>/dev/null)"

echo "desde b" >> "$MNT/b.txt"
check "escribir por un nombre se ve en el otro" $'compartido\ndesde b' "$(cat "$MNT/a.txt" 2>/dev/null)"

rm "$MNT/a.txt" 2>/dev/null
check "tras rm de a, b sigue existiendo" $'compartido\ndesde b' "$(cat "$MNT/b.txt" 2>/dev/null)"
check "nlink baja a 1" "1" "$(stat -c %h "$MNT/b.txt" 2>/dev/null)"

rm "$MNT/b.txt" 2>/dev/null
check "tras rm del último nombre ya no existe" "no" "$([ -e "$MNT/b.txt" ] && echo si || echo no)"

# --- errores ---
rm "$MNT/noexiste" 2>/dev/null
check "rm de archivo inexistente falla" "1" "$?"
cat "$MNT/noexiste" 2>/dev/null
check "cat de archivo inexistente falla" "1" "$?"
ln "$MNT/noexiste" "$MNT/otro" 2>/dev/null
check "ln de origen inexistente falla" "1" "$?"

echo
echo "Resultado: $PASS OK, $FAIL FAIL"
[ "$FAIL" -eq 0 ]
