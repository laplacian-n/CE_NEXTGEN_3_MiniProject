#!/usr/bin/env bash
# Node-RED check for Linux and macOS: finds Node.js, Node-RED and node-red-dashboard, installs what is missing,
# then prints where everything is and the commands that start Node-RED.
# Run:  bash check_nodered.sh [--yes] [--check-only]
#   --yes         answer yes to every question (install, add to PATH, start)
#   --check-only  only check, never install or change anything
set -u
MIN_NODE=18.5.0       # oldest Node.js that Node-RED 4 runs on
NODE_FOR_LATEST=22.9.0  # Node-RED 5 needs this; older Node.js gets Node-RED 4
PORT=1880
USER_DIR="$HOME/.node-red"
export npm_config_update_notifier=false npm_config_fund=false npm_config_audit=false
YES=0; CHECK_ONLY=0; PROBLEMS=0
for a in "$@"; do
  case "$a" in
    -y|--yes) YES=1 ;;
    --check-only) CHECK_ONLY=1 ;;
    -h|--help) sed -n '2,7p' "$0"; exit 0 ;;
  esac
done

if [ -t 1 ]; then G=$'\e[32m'; Y=$'\e[33m'; R=$'\e[31m'; N=$'\e[0m'; else G=; Y=; R=; N=; fi
ok()   { echo "  ${G}[OK]${N}   $*"; }
warn() { echo "  ${Y}[WARN]${N} $*"; }
bad()  { echo "  ${R}[FAIL]${N} $*"; PROBLEMS=$((PROBLEMS + 1)); }
info() { echo "         $*"; }
ask() {
  [ "$CHECK_ONLY" = 1 ] && return 1
  [ "$YES" = 1 ] && return 0
  local a; read -r -p "  $1 [Y/n] " a </dev/tty || return 1
  case "$a" in ""|y*|Y*) return 0;; *) return 1;; esac
}

# ver_ge 20.1.0 18.5.0 -> true when the first version is the same or newer
ver_ge() {
  local IFS=. i; local a=($1) b=($2)
  for i in 0 1 2; do
    [ "${a[i]:-0}" -gt "${b[i]:-0}" ] && return 0
    [ "${a[i]:-0}" -lt "${b[i]:-0}" ] && return 1
  done
  return 0
}

IS_MAC=0; [ "$(uname)" = Darwin ] && IS_MAC=1

find_node() {
  NODE="$(command -v node || command -v nodejs || true)"
  [ -n "$NODE" ] && return 0
  # nvm is only loaded in interactive shells, so a script does not see its node
  local nvm="${NVM_DIR:-$HOME/.nvm}/nvm.sh" p
  if [ -s "$nvm" ]; then
    . "$nvm" >/dev/null 2>&1
    NODE="$(command -v node || true)"
    [ -n "$NODE" ] && return 0
  fi
  # installed, but not on PATH
  for p in /usr/local/bin/node /opt/homebrew/bin/node /usr/bin/node "$HOME/.volta/bin/node" /snap/bin/node; do
    if [ -x "$p" ]; then NODE="$p"; export PATH="$(dirname "$p"):$PATH"; warn "Node.js is not on PATH, found $p"; return 0; fi
  done
  return 1
}

find_brew() { command -v brew || ls /opt/homebrew/bin/brew /usr/local/bin/brew 2>/dev/null | head -1; }

install_node() {
  if [ "$IS_MAC" = 1 ]; then
    local brew; brew="$(find_brew)"
    if [ -n "$brew" ]; then "$brew" install node || "$brew" upgrade node
    else bad "Homebrew is not installed"; info "Download the macOS installer (LTS .pkg) from https://nodejs.org and run it"; fi
  elif command -v apt-get >/dev/null; then sudo apt-get update && sudo apt-get install -y nodejs npm
  elif command -v dnf >/dev/null;     then sudo dnf install -y nodejs npm
  elif command -v pacman >/dev/null;  then sudo pacman -S --noconfirm nodejs npm
  elif command -v zypper >/dev/null;  then sudo zypper install -y nodejs npm
  else bad "no known package manager found"; info "Install the LTS version from https://nodejs.org"
  fi
}

too_old_help() {
  if [ "$IS_MAC" = 1 ]; then
    info "Upgrade with:  brew upgrade node   (or the LTS .pkg from https://nodejs.org)"
  elif command -v apt-get >/dev/null; then
    info "The Linux package is too old. Node-RED's own installer brings a new Node.js and Node-RED"
    info "(Debian, Ubuntu, Raspberry Pi OS):"
    info "  bash <(curl -sL https://github.com/node-red/linux-installers/releases/latest/download/update-nodejs-and-nodered-deb)"
  else
    info "Install a newer Node.js with nvm: https://github.com/nvm-sh/nvm  (then: nvm install --lts)"
  fi
}

pkg_version() {  # version from <dir>/package.json, empty when not installed
  [ -f "$1/package.json" ] && "$NODE" -p "require(process.argv[1]).version" "$1/package.json" 2>/dev/null
}

port_state() {  # 0 = Node-RED answers, 2 = another program uses the port, 1 = the port is free
  "$NODE" -e "fetch('http://127.0.0.1:$PORT/',{signal:AbortSignal.timeout(3000)}).then(r=>r.text())
    .then(t=>process.exit(/Node-RED/.test(t)?0:2)).catch(e=>process.exit(e.cause&&e.cause.code==='ECONNREFUSED'?1:2))" 2>/dev/null
}

echo "Node-RED check ($(uname -s))"
echo

# ---------------------------------------------------------------- 1) Node.js
echo "1) Node.js"
if ! find_node; then
  bad "Node.js is not installed"
  if ask "Install Node.js now?"; then install_node; fi
  find_node || { info "Install the LTS version from https://nodejs.org, then run this check again."; exit 1; }
  PROBLEMS=$((PROBLEMS - 1))
fi
NODE_VER="$("$NODE" -v | tr -d v)"
if ! ver_ge "$NODE_VER" "$MIN_NODE"; then
  bad "Node.js $NODE_VER is too old (Node-RED needs $MIN_NODE or newer)"
  too_old_help
  exit 1
fi
ok "$NODE (v$NODE_VER)"

NPM="$(dirname "$NODE")/npm"
[ -x "$NPM" ] || NPM="$(command -v npm || true)"
if [ -z "$NPM" ]; then
  bad "npm is not installed"
  if ask "Install npm now?"; then install_node; NPM="$(command -v npm || true)"; fi
  [ -n "$NPM" ] || { info "Install npm (it comes with Node.js from https://nodejs.org)"; exit 1; }
  PROBLEMS=$((PROBLEMS - 1))
fi
NPM_PREFIX="$("$NPM" prefix -g)"
NPM_ROOT="$("$NPM" root -g)"
ok "npm $("$NPM" -v), global packages in $NPM_ROOT"
# global installs need sudo when the npm folder belongs to root (e.g. /usr or /usr/local)
SUDO=""
d="$NPM_ROOT"; while [ ! -e "$d" ]; do d="$(dirname "$d")"; done
[ -w "$d" ] || SUDO="sudo"

# ---------------------------------------------------------------- 2) Node-RED
echo
echo "2) Node-RED"
if ver_ge "$NODE_VER" "$NODE_FOR_LATEST"; then WANT=node-red; else WANT=node-red@4; fi
RED_DIR="$NPM_ROOT/node-red"
RED_VER="$(pkg_version "$RED_DIR")"
OTHER_RED="$(command -v node-red || true)"
if [ -z "$RED_VER" ] && [ -n "$OTHER_RED" ]; then
  # installed some other way (Node-RED's Linux installer, snap, another npm) - use it as it is
  ok "Node-RED found at $OTHER_RED (not installed by this npm, version not checked)"
  RED_DIR=""
else
  if [ -n "$RED_VER" ] && [ "${RED_VER%%.*}" -ge 5 ] && ! ver_ge "$NODE_VER" "$NODE_FOR_LATEST"; then
    bad "Node-RED $RED_VER needs Node.js $NODE_FOR_LATEST or newer, this computer has $NODE_VER (this is why it does not start)"
    info "Fix: install Node-RED 4 (works with this Node.js), or upgrade Node.js and run this check again"
    RED_VER=""
  elif [ -z "$RED_VER" ]; then
    bad "Node-RED is not installed"
  fi
  if [ -z "$RED_VER" ]; then
    if ask "Install $WANT now (${SUDO:+sudo }npm install -g $WANT)?"; then
      # sudo drops the environment, so hand it PATH (for nvm) and the npm folder explicitly
      $SUDO env "PATH=$PATH" "$NPM" install -g --unsafe-perm --prefix "$NPM_PREFIX" "$WANT"
      RED_VER="$(pkg_version "$RED_DIR")"
      [ -n "$RED_VER" ] && PROBLEMS=$((PROBLEMS - 1))
    fi
    [ -n "$RED_VER" ] || { info "Install it by hand:  ${SUDO:+sudo }\"$NPM\" install -g --unsafe-perm $WANT"; exit 1; }
  fi
  ok "Node-RED $RED_VER in $RED_DIR"
fi
LAUNCHER="${OTHER_RED:-$NPM_PREFIX/bin/node-red}"
[ -n "$RED_DIR" ] && LAUNCHER="$NPM_PREFIX/bin/node-red"

ON_PATH=1
if [ -z "$(command -v node-red || true)" ]; then
  ON_PATH=0
  warn "the folder with node-red is not on PATH, so typing 'node-red' does not work"
  info "$(dirname "$LAUNCHER")"
  case "${SHELL:-}" in */zsh) RC="$HOME/.zshrc" ;; *) RC="$HOME/.bashrc" ;; esac
  if ask "Add it to PATH in $RC (for new terminals)?"; then
    echo "export PATH=\"$(dirname "$LAUNCHER"):\$PATH\"" >> "$RC"
    ON_PATH=1
    ok "added - open a new terminal for it to take effect"
  fi
fi
if [ "$IS_MAC" = 0 ] && command -v systemctl >/dev/null && systemctl list-unit-files nodered.service >/dev/null 2>&1 \
   && systemctl list-unit-files nodered.service | grep -q nodered; then
  info "Node-RED is also installed as a service: node-red-start / node-red-stop / node-red-log"
fi

# ---------------------------------------------------------------- 3) Dashboard
echo
echo "3) node-red-dashboard"
DASH_DIR="$USER_DIR/node_modules/node-red-dashboard"
DASH_VER="$(pkg_version "$DASH_DIR")"
if [ -z "$DASH_VER" ]; then
  bad "node-red-dashboard is not installed in $USER_DIR"
  if ask "Install node-red-dashboard now?"; then
    mkdir -p "$USER_DIR" && (cd "$USER_DIR" && "$NPM" install --omit=dev node-red-dashboard)
    DASH_VER="$(pkg_version "$DASH_DIR")"
    [ -n "$DASH_VER" ] && { PROBLEMS=$((PROBLEMS - 1)); info "(restart Node-RED if it is already running)"; }
  fi
  [ -n "$DASH_VER" ] || info "Or in Node-RED: menu > Manage palette > Install > node-red-dashboard"
fi
[ -n "$DASH_VER" ] && ok "node-red-dashboard $DASH_VER in $DASH_DIR"

# ---------------------------------------------------------------- 4) Port
echo
echo "4) Port $PORT"
port_state; STATE=$?
case $STATE in
  0) ok "Node-RED is already running at http://127.0.0.1:$PORT" ;;
  1) ok "port $PORT is free" ;;
  *) warn "another program is using port $PORT"
     info "close it, or start Node-RED on another port by adding  -p 1881  to the commands below" ;;
esac

# ---------------------------------------------------------------- Summary
echo
echo "Paths"
info "Node.js      $NODE"
info "Node-RED     ${RED_DIR:-$OTHER_RED}"
if [ -n "$DASH_VER" ]; then info "Dashboard    $DASH_DIR"; else info "Dashboard    (not installed)"; fi
info "User folder  $USER_DIR   (your flows and settings are saved here)"
echo
echo "Start Node-RED with any one of these (stop it with Ctrl+C):"
[ "$ON_PATH" = 1 ] && info "node-red"
info "\"$LAUNCHER\""
[ -n "$RED_DIR" ] && info "\"$NODE\" \"$RED_DIR/red.js\""
echo "Then open  http://127.0.0.1:$PORT  (editor)  and  http://127.0.0.1:$PORT/ui  (dashboard)"
echo

if [ "$PROBLEMS" -gt 0 ]; then
  bad "$PROBLEMS problem(s) left - see above"
  exit 1
fi
if [ "$STATE" = 1 ] && ask "Start Node-RED now in this terminal?"; then
  exec "$LAUNCHER"
fi
exit 0
