#!/bin/bash
# Pruebas de directorios, anidamiento, st_nlink y timestamps.
cd "$(dirname "$0")/.." || exit 1

FS_BIN=./fisopfs
MNT=$(mktemp -d)
DISK=/tmp/fisopfs_dir_test_$$.fisopfs
PASS=0
FAIL=0

cleanup() {
    fusermount -u "$MNT" 2>/dev/null
    rmdir "$MNT" 2>/dev/null
    rm -f "$DISK"
}
trap cleanup EXIT

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

# --- creación básica y stats ---
mkdir "$MNT/dir1"
check "mkdir crea el directorio" "directory" "$(stat -c %F "$MNT/dir1" 2>/dev/null)"
check "directorio nuevo tiene nlink 2" "2" "$(stat -c %h "$MNT/dir1" 2>/dev/null)"
check "directorio hereda uid:gid actual" "$(id -u):$(id -g)" "$(stat -c "%u:%g" "$MNT/dir1" 2>/dev/null)"
check "raiz pasa a nlink 3 tras crear dir1" "3" "$(stat -c %h "$MNT" 2>/dev/null)"

# --- anidamiento multinivel y st_nlink ---
mkdir "$MNT/dir1/sub1"
mkdir "$MNT/dir1/sub2"
mkdir "$MNT/dir1/sub1/nieto"
echo "contenido" > "$MNT/dir1/sub1/nieto/archivo.txt"

check "archivo en nivel 3 accesible" "contenido" "$(cat "$MNT/dir1/sub1/nieto/archivo.txt" 2>/dev/null)"
check "dir1 tiene nlink 4 (2 + sub1 + sub2)" "4" "$(stat -c %h "$MNT/dir1" 2>/dev/null)"
check "sub1 tiene nlink 3 (2 + nieto)" "3" "$(stat -c %h "$MNT/dir1/sub1" 2>/dev/null)"
check "nieto tiene nlink 2 (el archivo no suma link al dir)" "2" "$(stat -c %h "$MNT/dir1/sub1/nieto" 2>/dev/null)"

# --- readdir con . y .. ---
LS_OUT=$(ls -la "$MNT/dir1/sub1" 2>/dev/null)
check "readdir incluye entrada ." "si" "$(echo "$LS_OUT" | grep -q ' \.$' && echo si || echo no)"
check "readdir incluye entrada .." "si" "$(echo "$LS_OUT" | grep -q ' \.\.$' && echo si || echo no)"
check "readdir incluye subdirectorio nieto" "si" "$(echo "$LS_OUT" | grep -q ' nieto$' && echo si || echo no)"

# --- fechas (atime, mtime, ctime) ---
A1=$(stat -c %X "$MNT/dir1" 2>/dev/null)
M1=$(stat -c %Y "$MNT/dir1" 2>/dev/null)
sleep 1
ls "$MNT/dir1" >/dev/null 2>&1
A2=$(stat -c %X "$MNT/dir1" 2>/dev/null)
check "atime de directorio se actualiza con readdir" "si" "$([ "$A2" -gt "$A1" ] && echo si || echo no)"

mkdir "$MNT/dir1/sub3"
M2=$(stat -c %Y "$MNT/dir1" 2>/dev/null)
check "mtime del padre se actualiza al crear subdirectorio" "si" "$([ "$M2" -gt "$M1" ] && echo si || echo no)"

sleep 1
touch "$MNT/dir1"
M3=$(stat -c %Y "$MNT/dir1" 2>/dev/null)
check "mtime de directorio se actualiza con touch (utimens)" "si" "$([ "$M3" -gt "$M2" ] && echo si || echo no)"

# --- errores esperados ---
mkdir "$MNT/dir1" 2>/dev/null
check "mkdir sobre existente falla (EEXIST)" "1" "$?"

rmdir "$MNT/dir1/sub1/nieto" 2>/dev/null
check "rmdir sobre directorio no vacio falla (ENOTEMPTY)" "1" "$?"

rmdir "$MNT/dir1/sub1/nieto/archivo.txt" 2>/dev/null
check "rmdir sobre archivo falla (ENOTDIR)" "1" "$?"

mkdir "$MNT/dir1/sub1/nieto/archivo.txt/hijo" 2>/dev/null
check "mkdir bajo un archivo falla (ENOTDIR)" "1" "$?"

mkdir "$MNT/noexiste/hijo" 2>/dev/null
check "mkdir con padre inexistente falla (ENOENT)" "1" "$?"

# --- borrado y decremento de st_nlink ---
rmdir "$MNT/dir1/sub3" 2>/dev/null
check "rmdir borra directorio vacio" "0" "$?"
check "dir1 vuelve a nlink 4 tras borrar sub3" "4" "$(stat -c %h "$MNT/dir1" 2>/dev/null)"

rm "$MNT/dir1/sub1/nieto/archivo.txt"
rmdir "$MNT/dir1/sub1/nieto"
check "sub1 vuelve a nlink 2 tras borrar nieto" "2" "$(stat -c %h "$MNT/dir1/sub1" 2>/dev/null)"

rmdir "$MNT/dir1/sub1"
rmdir "$MNT/dir1/sub2"
check "dir1 vuelve a nlink 2 sin subdirectorios" "2" "$(stat -c %h "$MNT/dir1" 2>/dev/null)"

rmdir "$MNT/dir1"
check "dir1 ya no existe tras rmdir" "no" "$([ -e "$MNT/dir1" ] && echo si || echo no)"
check "raiz vuelve a nlink 2" "2" "$(stat -c %h "$MNT" 2>/dev/null)"

echo
echo "Resultado: $PASS OK, $FAIL FAIL"
[ "$FAIL" -eq 0 ]