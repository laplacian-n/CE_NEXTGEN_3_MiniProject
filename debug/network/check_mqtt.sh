#!/usr/bin/env bash
# MQTT connection check for Linux and macOS: finds Python 3 (installs it if missing), runs mqtt_check.py,
# then prints where Python is and the commands to run the checks yourself.
# Run:  bash check_mqtt.sh [url] [-u USERNAME]
#   any arguments go straight to mqtt_check.py (see the top of mqtt_check.py)
#   --yes  install Python without asking (must come first)
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
YES=0
if [ "${1:-}" = "--yes" ] || [ "${1:-}" = "-y" ]; then YES=1; shift; fi

if [ -t 1 ]; then G=$'\e[32m'; R=$'\e[31m'; N=$'\e[0m'; else G=; R=; N=; fi
ok()   { echo "  ${G}[OK]${N}   $*"; }
bad()  { echo "  ${R}[FAIL]${N} $*"; }
info() { echo "         $*"; }
ask() {
  [ "$YES" = 1 ] && return 0
  local a; read -r -p "  $1 [Y/n] " a </dev/tty || return 1
  case "$a" in ""|y*|Y*) return 0;; *) return 1;; esac
}

find_python() {
  PY=""
  local c p
  for c in python3 python /usr/local/bin/python3 /opt/homebrew/bin/python3; do
    command -v "$c" >/dev/null 2>&1 || continue
    p="$("$c" -c 'import sys; print(sys.executable if sys.version_info >= (3, 7) else "")' 2>/dev/null)"
    if [ -n "$p" ]; then PY="$p"; return 0; fi
  done
  return 1
}

install_python() {
  if [ "$(uname)" = Darwin ]; then
    local brew; brew="$(command -v brew || ls /opt/homebrew/bin/brew /usr/local/bin/brew 2>/dev/null | head -1)"
    if [ -n "$brew" ]; then "$brew" install python
    else xcode-select --install; info "Finish the Command Line Tools install that just opened, then run this check again."; fi
  elif command -v apt-get >/dev/null; then sudo apt-get update && sudo apt-get install -y python3
  elif command -v dnf >/dev/null;     then sudo dnf install -y python3
  elif command -v pacman >/dev/null;  then sudo pacman -S --noconfirm python
  elif command -v zypper >/dev/null;  then sudo zypper install -y python3
  else bad "no known package manager found"
  fi
}

echo "MQTT connection check"
echo
echo "Python"
if ! find_python; then
  bad "Python 3 is not installed"
  if ask "Install Python 3 now?"; then install_python; fi
  if ! find_python; then
    info "Install Python 3 from https://www.python.org/downloads/ (or your package manager), then run this check again."
    exit 1
  fi
fi
ok "$PY ($("$PY" -c 'import platform; print(platform.python_version())'))"
echo

"$PY" "$HERE/mqtt_check.py" "$@"
rc=$?

echo
echo "Run the checks yourself (copy one line):"
info "\"$PY\" \"$HERE/mqtt_check.py\" mqtt://mqtt.ce-esl.com:1883 -u USERNAME"
info "\"$PY\" \"$HERE/mqtt_check.py\" wss://mqtt.ce-esl.com:443/mqtt -u USERNAME"
info "\"$PY\" \"$HERE/mqtt_listen.py\" 'CENEXTGEN3/GROUPNAME/#' -u USERNAME"
exit $rc
