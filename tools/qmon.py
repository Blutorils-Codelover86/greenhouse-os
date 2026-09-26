#!/usr/bin/env python3
"""QEMU monitor client for the Greenhouse OS guest.

The OS reads the keyboard through IRQ1, so a serial line cannot type into the
shell: input has to go through the monitor's `sendkey`. This speaks to the
monitor socket that `tools/multipass-gfx.sh` sets up inside the Multipass VM.

  qmon.py <socket>                      read commands from stdin, one per line
  qmon.py <socket> --key "gfx 640 480"  type a string into the guest keyboard
  qmon.py <socket> --key "esc"          press one key
  qmon.py <socket> --raw "info status"  send one command verbatim
"""
import argparse
import os
import re
import socket
import sys
import time

# Characters QEMU's `sendkey` cannot express as a single letter, mapped to the
# key names it understands.
KEY_NAMES = {
    ' ': 'spc',
    '\n': 'ret',
    '\t': 'tab',
    '.': 'dot',
    ',': 'comma',
    '/': 'slash',
    ';': 'semicolon',
    '-': 'minus',
    '=': 'equal',
    '[': 'bracket_left',
    ']': 'bracket_right',
    '\\': 'backslash',
    '`': 'grave_accent',
    "'": 'apostrophe',
}

ANSI = re.compile(r'\x1b\[[0-9;]*[A-Za-z]|\x1b\][^\x07]*\x07|\x1b.')


class Monitor:
    def __init__(self, path, timeout=1.2):
        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.sock.connect(path)
        self.sock.settimeout(0.3)
        self.timeout = timeout
        self._drain(0.6)  # the banner and first prompt

    def _drain(self, wait):
        """Collect whatever the monitor has to say within `wait` seconds."""
        out = b''
        end = time.time() + wait
        while time.time() < end:
            try:
                chunk = self.sock.recv(8192)
            except socket.timeout:
                continue
            if not chunk:
                break
            out += chunk
            end = max(end, time.time() + 0.15)
        return out.decode('utf-8', errors='replace')

    def command(self, text, wait=None):
        self.sock.sendall((text + '\n').encode())
        reply = self._drain(self.timeout if wait is None else wait)
        # The monitor runs a line editor and echoes every character back as it
        # arrives, so the reply starts with the command typed over and over.
        # Drop that leading echo line before the output is worth reading.
        lines = reply.splitlines()
        if lines and text.strip() and lines[0].replace('\r', '').endswith(text.strip()):
            lines = lines[1:]
        return clean('\n'.join(lines))

    def type_text(self, text, delay=0.03):
        for ch in text:
            name = KEY_NAMES.get(ch)
            if name is None:
                name = 'shift-' + ch if ch.isupper() else ch
            self.command('sendkey ' + name)
            time.sleep(delay)
        return ''

    def close(self):
        try:
            self.sock.close()
        except OSError:
            pass


def clean(text):
    """Strip the monitor's echo and cursor movement, keep the real output."""
    text = ANSI.sub('', text)
    lines = []
    for line in text.splitlines():
        line = line.replace('\r', '').rstrip()
        if not line:
            continue
        if line.startswith('(qemu)') and len(line) <= 6:
            continue
        lines.append(line)
    return '\n'.join(lines)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('socket', help='path to the QEMU monitor unix socket')
    ap.add_argument('--key', metavar='TEXT', help='type TEXT into the guest keyboard')
    ap.add_argument('--raw', metavar='CMD', help='send one monitor command verbatim')
    ap.add_argument('--delay', type=float, default=0.03, help='per-key delay in seconds')
    ap.add_argument('--wait', type=float, default=1.2, help='seconds to wait for a reply')
    args = ap.parse_args()

    if not os.path.exists(args.socket):
        print(f'no monitor socket at {args.socket}', file=sys.stderr)
        return 1

    mon = Monitor(args.socket, timeout=args.wait)
    try:
        if args.key is not None:
            mon.type_text(args.key, args.delay)
            return 0
        if args.raw is not None:
            reply = mon.command(args.raw)
            if reply:
                print(reply)
            return 0

        for line in sys.stdin:
            line = line.strip()
            if not line:
                continue
            reply = mon.command(line)
            if reply:
                print(reply)
            sys.stdout.flush()
    finally:
        mon.close()
    return 0


if __name__ == '__main__':
    sys.exit(main())
