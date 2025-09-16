#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="$ROOT/otp"
TMPDIR="$(mktemp -d)"
PLA="$ROOT/test.txt"
CYP="$ROOT/test_output.txt"         
DEC="$ROOT/test_decrypted.txt"     

trap 'rm -rf "$TMPDIR"' EXIT


bold() { printf "\033[1m%s\033[0m" "$*"; }
green() { printf "\033[32m%s\033[0m" "$*"; }
red() { printf "\033[31m%s\033[0m" "$*"; }
yellow() { printf "\033[33m%s\033[0m" "$*"; }
hr() { printf '\n%s\n' "----------------------------------------"; }

step() { hr; echo "$(bold "$1")"; }
info() { echo "  • $1"; }

SEED=4212
A=84589
C=45989
M=217728

step "1) Подготовка входного файла"
if [ ! -f "$PLA" ]; then
  echo "$(red "FAIL") Не найден входной файл: $PLA" >&2
  echo "Создайте его и заполните текстом. Пример: echo 'hello' > $PLA" >&2
  exit 1
fi
info "Вход: $PLA"
info "Размер: $(wc -c < "$PLA") байт"
info "CRC32: $(cksum "$PLA" | awk '{print $1}')"

step "2) Шифрование (plaintext -> ciphertext)"
info "Команда: $BIN -i $PLA -o $CYP -x $SEED -a $A -c $C -m $M"
"$BIN" -i "$PLA" -o "$CYP" -x "$SEED" -a "$A" -c "$C" -m "$M"
info "Выход (шифротекст): $CYP"
info "Размер: $(wc -c < "$CYP") байт"
info "CRC32: $(cksum "$CYP" | awk '{print $1}')"

step "3) Дешифрование (ciphertext -> decrypted)"
info "Команда: $BIN -i $CYP -o $DEC -x $SEED -a $A -c $C -m $M"
"$BIN" -i "$CYP" -o "$DEC" -x "$SEED" -a "$A" -c "$C" -m "$M"
info "Выход (дешифрованный текст): $DEC"
info "Размер: $(wc -c < "$DEC") байт"
info "CRC32: $(cksum "$DEC" | awk '{print $1}')"

step "4) Проверка равенства файлов"
if diff -q "$PLA" "$DEC" >/dev/null; then
  echo "$(green "OK") Файлы идентичны (diff)"
else
  echo "$(red "FAIL") Файлы отличаются (diff)" >&2
  exit 1
fi

crc_src=$(cksum "$PLA" | awk '{print $1}')
crc_dec=$(cksum "$DEC" | awk '{print $1}')
if [ "$crc_src" = "$crc_dec" ]; then
  echo "$(green "OK") CRC32 совпадают: $crc_src"
else
  echo "$(red "FAIL") CRC32 различаются: src=$crc_src dec=$crc_dec" >&2
  exit 1
fi

hr
echo "$(bold "Итог:") $(green "Round-trip OK") — см. файлы: $PLA, $CYP, $DEC"


