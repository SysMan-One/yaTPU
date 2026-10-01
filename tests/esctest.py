# ESC sequences split in time: drive the editor in a pty, ESC and the rest of a key's sequence sent with a gap
# between them, as a tunnel or a slow line delivers them.  Each result is compared with the same keys sent at once
# to the editor started directly.
#
# usage: LANG=C.UTF-8 python3 tests/esctest.py <path-to-tpu> <scratch-dir> [<yaVTERM-build-dir> | ssh | ssh:<host>]
#
# With a yaVTERM build directory (vtc, vtd in it) the split runs go through the tunnel: vtc in the pty -> vtd on a
# free local port -> the editor.  With "ssh" they go through OpenSSH: ssh -tt in the pty -> a private sshd (its
# own config, host and user keys, on a free local port, run as the current user) -> the editor; with "ssh:<host>"
# through an sshd already running there, the user's own keys and known_hosts (the editor at the same path).  Keys, pins and
# helper scripts live in the scratch directory, nothing outside it is touched.  All paths absolute.
import os, pty, select, signal, socket, struct, fcntl, termios, subprocess, sys, time
TPU = sys.argv[1]; W = sys.argv[2]; VT = sys.argv[3] if len(sys.argv) > 3 else None

def drain(m, t):
    end = time.time() + t
    while True:
        r, _, _ = select.select([m], [], [], max(0, end - time.time()))
        if not r: return
        try:
            if not os.read(m, 65536): return
        except OSError: return

def check(name, cond, extra=''):
    print(('PASS ' if cond else 'FAIL ') + name + ('' if cond else '  ' + extra), flush=True)
    return cond

def free_port():
    with socket.socket() as l_s:
        l_s.bind(('127.0.0.1', 0)); return l_s.getsockname()[1]

VTD = None
PROG = TPU
if VT == 'ssh':
    # A private sshd: host and user keys made here, AuthorizedKeysFile here, no system file read or changed
    port = free_port(); d = os.path.join(W, 'ssh'); os.makedirs(d, exist_ok=True); os.chmod(d, 0o700)
    for k in ('host_key', 'user_key'):
        if not os.path.exists(os.path.join(d, k)):
            subprocess.run(['ssh-keygen', '-q', '-t', 'ed25519', '-N', '', '-f', os.path.join(d, k)], check=True)
    with open(os.path.join(d, 'user_key.pub')) as f, open(os.path.join(d, 'authorized_keys'), 'w') as a: a.write(f.read())
    os.chmod(os.path.join(d, 'authorized_keys'), 0o600)
    with open(os.path.join(d, 'sshd_config'), 'w') as f:
        f.write(f'Port {port}\nListenAddress 127.0.0.1\nHostKey {d}/host_key\nAuthorizedKeysFile {d}/authorized_keys\n'
                f'PidFile {d}/sshd.pid\nPasswordAuthentication no\nKbdInteractiveAuthentication no\nUsePAM no\n'
                'StrictModes no\nPermitRootLogin prohibit-password\nPrintMotd no\nPrintLastLog no\nAcceptEnv LANG\n')
    VTD = subprocess.Popen(['/usr/sbin/sshd', '-D', '-e', '-f', os.path.join(d, 'sshd_config')],
            stdout=open(os.path.join(d, 'sshd.log'), 'w'), stderr=subprocess.STDOUT)
    end = time.time() + 5
    while time.time() < end:
        try:
            socket.create_connection(('127.0.0.1', port), 0.2).close(); break
        except OSError: time.sleep(0.1)
    SSH = f'ssh -tt -q -i {d}/user_key -p {port} -o UserKnownHostsFile={d}/known_hosts -o StrictHostKeyChecking=no -o BatchMode=yes 127.0.0.1'
elif VT and VT.startswith('ssh:'):
    SSH = f'ssh -tt -q -o BatchMode=yes {VT[4:]}'
if VT and VT.startswith('ssh'):
    wrapper = os.path.join(W, 'ssh_tpu')
    with open(wrapper, 'w') as f:
        f.write('#!/bin/bash\n'
                f'l_cmd="cd $(printf %q "$PWD") && exec env LANG=$(printf %q "${{LANG:-C.UTF-8}}") {TPU}"\n'
                'for a in "$@"; do l_cmd+=" $(printf %q "$a")"; done\n'
                f'exec {SSH} "$l_cmd"\n')
    os.chmod(wrapper, 0o755)
    PROG = wrapper
elif VT and not VT.startswith('ssh'):
    # The tunnel: vtd with a script as its backend; that script and the client wrapper share a run file (cwd, args)
    port = free_port()
    run, home = os.path.join(W, 'vt_next_run'), os.path.join(W, 'vt_home')
    os.makedirs(home, exist_ok=True)
    backend, wrapper = os.path.join(W, 'vt_backend.sh'), os.path.join(W, 'vt_tpu')
    with open(backend, 'w') as f:
        f.write(f'#!/bin/bash\n. {run}\ncd "$RUN_CWD" && exec {TPU} "${{RUN_ARGS[@]}}"\n')
    log = os.path.join(W, 'vt_vtd.log')
    VTD = subprocess.Popen([os.path.join(VT, 'vtd'), '-l', str(port), '-k', os.path.join(W, 'vt_hostkey'), '-e', backend],
            cwd=W, stdout=open(log, 'w'), stderr=subprocess.STDOUT, env=dict(os.environ, TERM='xterm'))
    pub = None; end = time.time() + 5
    while time.time() < end and not pub:
        time.sleep(0.1)
        if 'LISTENING' in open(log, errors='replace').read():	# Key file: private key, then public
            with open(os.path.join(W, 'vt_hostkey'), 'rb') as k: pub = k.read()[32:64].hex()
    if not pub:
        print('FAIL vtd did not start'); VTD.kill(); sys.exit(1)
    with open(wrapper, 'w') as f:
        f.write('#!/bin/bash\n'
                f"{{ printf 'RUN_CWD=%q\\n' \"$PWD\"; printf 'RUN_ARGS=('; for a in \"$@\"; do printf '%q ' \"$a\"; done; printf ')\\n'; }} > {run}\n"
                f'HOME={home} exec {os.path.join(VT, "vtc")} 127.0.0.1:{port} {pub}\n')
    os.chmod(backend, 0o755); os.chmod(wrapper, 0o755)
    PROG = wrapper

N = 0
def run(prog, intro, parts, tail=b'X'):
    """Start the editor on a new file, type intro, then parts (bytes, or float = a pause), then tail and EXIT;
    return the file content."""
    global N; N += 1; name = f'e{N}.txt'
    m, s = pty.openpty()
    fcntl.ioctl(s, termios.TIOCSWINSZ, struct.pack('HHHH', 24, 80, 0, 0))
    p = subprocess.Popen([prog, name], stdin=s, stdout=s, stderr=s, cwd=W, close_fds=True); os.close(s)
    drain(m, 0.8); os.write(m, intro); drain(m, 0.3)
    for x in parts:
        if isinstance(x, float): time.sleep(x)
        else: os.write(m, x)
    drain(m, 0.3)
    if p.poll() is None:
        os.write(m, tail); drain(m, 0.2)
    if p.poll() is None:
        os.write(m, b'\x1a'); drain(m, 0.8)
    try: p.wait(timeout=5)
    except subprocess.TimeoutExpired: p.kill(); p.wait()
    os.close(m)
    f = os.path.join(W, name)
    return open(f).read() if os.path.exists(f) else None

GAPS = (0.0, 0.03, 0.15, 0.2)
INTRO = b'abc\rdef\x1b[A'					# Two lines, the cursor on the first: room to move every way
failures = 0

# 1. Arrows, CSI and SS3 forms (normal and application cursor mode), ESC apart from the rest
for key, f in (('Up', 'A'), ('Down', 'B'), ('Right', 'C'), ('Left', 'D')):
    for form in '[O':
        seq = (form + f).encode()
        ref = run(TPU, INTRO, [b'\x1b' + seq])
        for gap in GAPS:
            got = run(PROG, INTRO, [b'\x1b', gap, seq])
            failures += not check(f'1 {key} ESC {form}{f} gap {int(gap * 1000)} ms', got == ref, f'{got!r}, expected {ref!r}')

# 2. Other keys that are ESC sequences: Home, End, Delete, KP- (DO in application keypad mode; the prompt is cancelled)
for key, seq, extra in (('Home', b'[H', b''), ('End', b'[F', b''), ('Delete', b'[3~', b''), ('KP-', b'Om', b'\x15\r')):
    ref = run(TPU, INTRO, [b'\x1b' + seq, extra])
    for gap in GAPS:
        got = run(PROG, INTRO, [b'\x1b', gap, seq, extra])
        failures += not check(f'2 {key} gap {int(gap * 1000)} ms', got == ref, f'{got!r}, expected {ref!r}')

# 3. Esc is GOLD: Esc and a letter typed fast or slow is GOLD-letter, not lost (GOLD-X = EXIT writes the file)
for name, parts in (('one write', [b'\x1bx']), ('pause 400 ms', [b'\x1b', 0.4, b'x'])):
    got = run(PROG, b'abc', parts, tail=b'')
    failures += not check(f'3 Esc x {name}', got == 'abc\n', repr(got))

# 4. Esc O (or [) and then a pause: GOLD-O and the next key, as typed by hand at any pace
got = run(PROG, b'abc', [b'\x1bO', 0.4, b'x'])
ref = run(TPU, b'abc', [b'\x1b', 0.4, b'O', 0.4, b'x'])
failures += not check('4 Esc O pause x', got == ref, f'{got!r}, expected {ref!r}')

# 5. The keypad through the link, whole and split: KP- DO, KP9 ERASE LINE, KP3, KP0, GOLD-KP0/1/2, KP/ GOLD, Insert
E = b'\x1b'
base = run(TPU, INTRO, [])						# No key: each one below must change the result
for name, seq, before, after in (('KP- DO top', b'Om', [], [b'top\r']), ('KP9 ERASE LINE', b'Oy', [], []),
                ('KP3 MOVE BY CHAR', b'Os', [], []),
                ('KP/ GOLD-X', b'Oo', [], [b'x']),
                ('KP0 LINE START', b'Op', [], []), ('GOLD-KP0 OPEN LINE', b'Op', [E + b'Oo'], []),
                ('GOLD-KP1 CHANGE CASE', b'Oq', [E + b'Oo'], []), ('GOLD-KP2 ERASE EOL', b'Or', [b'\x1b[D', E + b'Oo'], []),
                ('Insert pastes', b'[2~', [E + b'On', b'\x1b[C\x1b[C', b'\x1b[3~', b'\x1b[F'], [])):
    ref = run(TPU, INTRO, before + [E + seq] + after)
    failures += not check(f'5 {name} has an effect', ref != base, repr(ref))
    for gap in (0.0, 0.15):
        got = run(PROG, INTRO, before + [E, gap, seq] + after)
        failures += not check(f'5 {name} gap {int(gap * 1000)} ms', got == ref, f'{got!r}, expected {ref!r}')

if VTD:
    VTD.send_signal(signal.SIGTERM)
    try: VTD.wait(timeout=3)
    except subprocess.TimeoutExpired: VTD.kill()
print('failures:', failures)
sys.exit(1 if failures else 0)
