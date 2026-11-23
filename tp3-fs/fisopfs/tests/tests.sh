#!/bin/bash
set -u

MNT=$(mktemp -d)
DISK=$(mktemp -u).fisopfs
fail=0

./fisopfs -f --filedisk "$DISK" "$MNT" &
PID=$!
sleep 1

check() {
	if [ "$2" == "$3" ]; then
		echo "[OK]   $1"
	else
		echo "[FAIL] $1: esperaba '$2', obtuve '$3'"
		fail=1
	fi
}

check "raiz es directorio" "directory" "$(stat -c %F "$MNT")"
check "raiz permisos 755" "755" "$(stat -c %a "$MNT")"
check "path inexistente da error" "" "$(stat -c %F "$MNT/nope" 2>/dev/null)"

fusermount -u "$MNT"
wait $PID 2>/dev/null
rm -rf "$MNT" "$DISK"

exit $fail