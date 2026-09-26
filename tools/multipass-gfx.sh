#!/usr/bin/env bash
# =============================================================================
# Greenhouse OS - run the OS inside a Multipass VM
# =============================================================================
#
# Multipass gives us a throwaway Ubuntu VM, and QEMU runs inside it. That is one
# more level of virtualisation than the test suite uses, so it is the place to
# see the OS behave like a real machine rather than like a test: a persistent
# guest you can attach a VNC viewer to, poke at with the mouse, and leave running.
#
# The host only ever talks to the guest over the QEMU monitor socket, because
# the OS takes keyboard input from IRQ1 rather than the serial line. Typing is
# therefore `sendkey` per character, exactly like the tests do.
#
#   ./tools/multipass-gfx.sh setup     create the VM, install QEMU, copy images
#   ./tools/multipass-gfx.sh run       boot the OS in the guest (headless + VNC)
#   ./tools/multipass-gfx.sh stop      shut the guest QEMU down
#   ./tools/multipass-gfx.sh status    is it running, and what does it say
#   ./tools/multipass-gfx.sh serial    tail the serial log
#   ./tools/multipass-gfx.sh key CMD   type a command at the shell prompt
#   ./tools/multipass-gfx.sh mon CMD   send a raw monitor command
#   ./tools/multipass-gfx.sh grab FILE capture a screendump to the host
#   ./tools/multipass-gfx.sh vnc       print the VNC address to connect to
#   ./tools/multipass-gfx.sh shell     a shell inside the VM
#
# `run` leaves the display on VNC :1 in the guest. Connect from the host with
#   ssh -N -L 5901:127.0.0.1:5901 -p <ssh-port> ubuntu@127.0.0.1
# or point a VNC viewer straight at the VM's IP on port 5901; `vnc` prints both.
# =============================================================================

set -uo pipefail

NAME="${GREENHOUSE_MP_NAME:-greenhouse}"
GUEST_DIR="${GREENHOUSE_MP_DIR:-/home/ubuntu/gfx}"
MON_SOCK="/tmp/gfx-mon.sock"
SERIAL_LOG="/tmp/gfx-serial.log"
VNC_DISPLAY=1
VNC_PORT=$((5900 + VNC_DISPLAY))
# TCG on purpose: nested KVM is not always available inside the Multipass VM, and
# the OS is small enough that emulation is fast enough for real use.
ACCEL="${GREENHOUSE_MP_ACCEL:-tcg}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

# The snap install keeps its binaries out of the default PATH.
export PATH="$PATH:/var/lib/snapd/snap/bin"

need_multipass() {
    if ! command -v multipass >/dev/null 2>&1; then
        echo "multipass not found (looked in PATH and /var/lib/snapd/snap/bin)" >&2
        exit 1
    fi
}

mp() { multipass "$@" 2>&1; }

in_guest() { multipass exec "$NAME" -- bash -lc "$1" 2>&1; }

require_running() {
    if ! mp list | grep -qE "^$NAME[[:space:]]+Running"; then
        echo "instance '$NAME' is not running (multipass list to check)" >&2
        exit 1
    fi
}

# --- setup -------------------------------------------------------------------

do_setup() {
    need_multipass
    if ! mp list | grep -q "^$NAME"; then
        echo "==> creating instance '$NAME' (first run downloads an image)"
        mp launch --name "$NAME" --cpus 2 --memory 2G --disk 10G 24.04 || exit 1
    else
        mp start "$NAME" >/dev/null
    fi

    echo "==> waiting for cloud-init"
    for _ in $(seq 1 60); do
        if in_guest 'cloud-init status --wait >/dev/null 2>&1; echo ready' | grep -q ready; then
            break
        fi
        sleep 5
    done

    echo "==> installing qemu-system-x86 in the guest"
    in_guest 'command -v qemu-system-x86_64 >/dev/null 2>&1 || (sudo apt-get update -qq && sudo DEBIAN_FRONTEND=noninteractive apt-get install -y -qq qemu-system-x86)' >/dev/null
    in_guest 'command -v qemu-system-x86_64' | grep -q qemu-system || {
        echo "qemu-system-x86_64 is still missing in the guest" >&2
        exit 1
    }

    echo "==> copying images and tools to $GUEST_DIR"
    in_guest "mkdir -p '$GUEST_DIR'"
    local artefacts=("$REPO_DIR/greenhouse.iso")
    [[ -f "$REPO_DIR/disk.img" ]] && artefacts+=("$REPO_DIR/disk.img")
    mp transfer "${artefacts[@]}" "$NAME:$GUEST_DIR/" >/dev/null || exit 1
    mp transfer "$SCRIPT_DIR/qmon.py" "$NAME:$GUEST_DIR/qmon.py" >/dev/null || exit 1

    echo "==> ready. Next: $0 run"
}

# --- run / stop --------------------------------------------------------------

do_run() {
    require_running
    if in_guest 'pgrep -f "[n]ame greenhouse-gfx" >/dev/null'; then
        echo "guest QEMU is already running ($0 stop first)"
        return 0
    fi

    if ! in_guest "test -f '$GUEST_DIR/greenhouse.iso'"; then
        echo "no ISO in the guest yet, run '$0 setup'" >&2
        exit 1
    fi

    # Build the argument list here and hand it over with printf %q, so the guest
    # shell sees one already-quoted word per argument.
    local args=(
        -machine "accel=$ACCEL"
        -boot d
        -cdrom greenhouse.iso
    )
    if in_guest "test -f '$GUEST_DIR/disk.img'"; then
        args+=(-drive "file=disk.img,format=raw,index=0,media=disk,if=ide")
    fi
    args+=(
        -m 256M
        -vga std
        -display none
        -vnc "0.0.0.0:$VNC_DISPLAY"
        -monitor "unix:$MON_SOCK,server,nowait"
        -serial "file:$SERIAL_LOG"
        -name greenhouse-gfx
    )

    echo "==> booting Greenhouse OS in '$NAME' (accel=$ACCEL, VNC :$VNC_DISPLAY)"
    in_guest "cd '$GUEST_DIR' && rm -f '$SERIAL_LOG' '$MON_SOCK' /tmp/gfx-qemu.log && \
              nohup qemu-system-x86_64 $(printf '%q ' "${args[@]}") \
              >/tmp/gfx-qemu.log 2>&1 &"

    for _ in $(seq 1 40); do
        sleep 1
        if in_guest "test -S $MON_SOCK && echo up" | grep -q up; then
            break
        fi
    done

    if ! in_guest "test -S $MON_SOCK && echo up" | grep -q up; then
        echo "guest QEMU did not come up; its stderr says:" >&2
        in_guest 'cat /tmp/gfx-qemu.log' >&2
        exit 1
    fi

    sleep 6
    do_status
    echo
    "$0" vnc
}

do_stop() {
    require_running
    echo "==> asking the guest QEMU to quit"
    in_guest "python3 '$GUEST_DIR/qmon.py' $MON_SOCK --raw 'quit'" >/dev/null 2>&1
    sleep 2
    in_guest "pkill -f '[n]ame greenhouse-gfx'" >/dev/null 2>&1
    echo "stopped"
}

# --- observing ---------------------------------------------------------------

do_status() {
    require_running
    if in_guest 'pgrep -f "[n]ame greenhouse-gfx" >/dev/null && echo yes' | grep -q yes; then
        echo "guest QEMU: running"
    else
        echo "guest QEMU: not running ($0 run)"
    fi
    local vm_ip
    vm_ip="$(mp list | awk -v n="$NAME" '$1 == n {print $3}')"
    echo "instance ip: ${vm_ip:-unknown}   vnc: ${vm_ip:-<ip>}:$VNC_PORT"
    local ssh_port
    ssh_port="$(mp info "$NAME" | awk '/SSH port/ {print $NF}')"
    echo "ssh tunnel: ssh -N -L $VNC_PORT:127.0.0.1:$VNC_PORT -p ${ssh_port:-2222} ubuntu@127.0.0.1"
    echo
    echo "--- serial tail"
    in_guest "tail -n 20 '$SERIAL_LOG' 2>/dev/null || echo '(no serial log yet)'"
}

do_serial() {
    require_running
    in_guest "tail -n '${1:-40}' '$SERIAL_LOG' 2>/dev/null || echo '(no serial log yet)'"
}

do_key() {
    require_running
    if [[ $# -eq 0 ]]; then
        echo "usage: $0 key <text to type>" >&2
        exit 1
    fi
    in_guest "python3 '$GUEST_DIR/qmon.py' $MON_SOCK --key \"$*\""
    sleep 0.4
}

do_mon() {
    require_running
    if [[ $# -eq 0 ]]; then
        echo "usage: $0 mon <monitor command>" >&2
        exit 1
    fi
    in_guest "python3 '$GUEST_DIR/qmon.py' $MON_SOCK --raw \"$*\""
}

do_grab() {
    require_running
    local dest="${1:-greenhouse.ppm}"
    local base remote stage
    base="$(basename "$dest")"
    remote="/tmp/$base"
    # The snap build of multipass runs confined, so it cannot see the host's
    # /tmp: a transfer aimed there would land in the snap's private tmp and look
    # like it worked. Stage through the home directory and move it ourselves.
    stage="$HOME/.greenhouse-grab-$base"

    in_guest "python3 '$GUEST_DIR/qmon.py' $MON_SOCK --raw 'screendump $remote'" >/dev/null
    sleep 1
    in_guest "test -f '$remote' || { echo 'the guest produced no screendump' >&2; exit 1; }"
    if ! mp transfer "$NAME:$remote" "$stage" >/dev/null; then
        echo "transfer of $remote out of '$NAME' failed" >&2
        exit 1
    fi
    in_guest "rm -f '$remote'"
    mkdir -p "$(dirname "$dest")" 2>/dev/null
    mv "$stage" "$dest"
    echo "wrote $dest"
}

do_vnc() {
    local vm_ip ssh_port
    vm_ip="$(mp list | awk -v n="$NAME" '$1 == n {print $3}')"
    ssh_port="$(mp info "$NAME" | awk '/SSH port/ {print $NF}')"
    cat <<EOF
VNC inside the guest is display :$VNC_DISPLAY (port $VNC_PORT), bound to all of
the guest's interfaces, so a viewer can go straight at:

    ${vm_ip:-<vm-ip>}:$VNC_PORT

If your viewer refuses that address, tunnel it over the instance's SSH port:

    ssh -N -L $VNC_PORT:127.0.0.1:$VNC_PORT -p ${ssh_port:-2222} ubuntu@127.0.0.1
    # then connect a viewer to 127.0.0.1:$VNC_PORT

The keyboard reaches the guest over VNC, which is what the shell reads.
EOF
}

do_shell() {
    require_running
    multipass shell "$NAME"
}

# --- dispatch ----------------------------------------------------------------

case "${1:-}" in
    setup)  shift; do_setup "$@" ;;
    run)    shift; do_run "$@" ;;
    stop)   shift; do_stop "$@" ;;
    status) shift; do_status "$@" ;;
    serial) shift; do_serial "$@" ;;
    key)    shift; do_key "$@" ;;
    mon)    shift; do_mon "$@" ;;
    grab)   shift; do_grab "$@" ;;
    vnc)    shift; do_vnc "$@" ;;
    shell)  shift; do_shell "$@" ;;
    *)
        sed -n '2,30p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
        exit 1
        ;;
esac
