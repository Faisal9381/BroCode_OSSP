# ForgeShell — Mini Linux Shell with Background Job Management

**Course:** Operating Systems and Systems Programming (OSSP)
**Stack:** C (GCC) · Node.js + Express + WebSocket · node-pty · React + Vite + Tailwind CSS + xterm.js
**Runs on:** Ubuntu (WSL)

ForgeShell is a real Linux shell written in C that executes real programs
(`fork()` / `execvp()` / `waitpid()`), manages real background jobs with
process groups and signals, and is accessible from a browser through a
node-pty bridge. **No output is simulated** — every command runs in an
actual `forgeshell` process attached to a real pseudo-terminal, and the
jobs panel is fed by the C shell's own job table.

```
Browser (xterm.js)
   │  WebSocket  {input / output / jobs}
   ▼
Node backend (Express + ws + node-pty)
   │  PTY (/dev/pts/N)
   ▼
forgeshell (C)  ──fork/execvp──►  real Linux processes
   │
   └── writes real job table to FORGESHELL_JOBS_FILE (JSON) ──► backend ──► UI
```

## Project layout

```
ForgeShell/
├── shell/                  # the C shell
│   ├── main.c              # REPL, job-control setup, line editor (raw termios)
│   ├── parser.c/.h         # quotes, $VAR/~ expansion, |, <, >, >>, 2>, 2>&1, &
│   ├── execute.c/.h        # fork/execvp, pipes, redirection, waitpid, tty hand-off
│   ├── builtins.c/.h       # cd pwd echo jobs fg bg kill wait help exit
│   ├── jobs.c/.h           # job table, SIGCHLD reaping, notifications, JSON snapshot
│   ├── shell.h  Makefile
│   └── test_shell.py       # 27 integration tests over a REAL pty
├── backend/
│   ├── server.js           # Express + WebSocket + node-pty
│   ├── test_ws.js          # backend smoke test (incl. signal-pad checks)
│   └── README.md
├── frontend/
│   ├── src/App.jsx  src/components/{Terminal,JobsPanel,SignalPad}.jsx
│   ├── src/hooks/useShellSession.js  src/styles/index.css
│   └── vite.config.js      # dev proxy /ws + /api → port 3000
└── README.md  .gitignore
```

## Exact execution instructions (WSL Ubuntu)

> Everything runs **inside WSL** (the shell is a Linux ELF binary and
> node-pty needs a Linux PTY). From Windows, WSL's `localhost` is
> reachable directly, so the browser opens `http://localhost:3000`.

### 0. Prerequisites

```bash
sudo apt update
sudo apt install -y gcc make python3 build-essential
# Node.js 18+ (NodeSource recommended):
curl -fsSL https://deb.nodesource.com/setup_22.x | sudo -E bash -
sudo apt install -y nodejs
```

### 1. Build the C shell

```bash
cd shell
make                # produces ./forgeshell   (-Wall -Wextra, zero warnings)
```

### 2. Run the shell tests (optional but recommended)

```bash
python3 test_shell.py      # 27 checks over a real pty (jobs, Ctrl+Z/C, pipes…)
```

### 3. Install backend + frontend dependencies

```bash
cd ../backend && npm install
cd ../frontend && npm install && npm run build
```

### 4. Start the server

```bash
cd ../backend && npm start          # → http://localhost:3000
```

Open **http://localhost:3000** in your browser. You get the live
terminal on the left and the real jobs panel on the right.

### Development mode (hot reload)

```bash
# terminal 1
cd backend && npm start               # port 3000 (shell + API)
# terminal 2
cd frontend && npm run dev            # port 5173, proxies /ws + /api → 3000
```
Then open http://localhost:5173.

### Configuration

| Variable          | Default               | Meaning                  |
|-------------------|-----------------------|--------------------------|
| `PORT`            | `3000`                | backend port             |
| `HOST`            | `0.0.0.0`             | bind address             |
| `FORGESHELL_BIN`  | `../shell/forgeshell` | path of the C binary     |

## Suggested live demo (maps to the syllabus)

Type these in the web terminal (or use the footer chips):

| Step | Command | What it demonstrates |
|------|---------|----------------------|
| 1 | `ps -ef \| head` | **CO-1/CO-2** real `fork/execvp` of system programs, pipelines |
| 2 | `sleep 60 &` | **CO-2/CO-6** background job; watch the **[1] RUNNING** card appear |
| 3 | `sleep 61 &` then `jobs -l` | **CO-3** job table with job ids, pids, states |
| 4 | press **⏸ bg / ▶ fg / ✕ kill** in the panel | **CO-3** panel types `fg %1`, `bg %1`, `kill -TERM %1` into the shell; the C code sends real `SIGCONT`/`SIGTERM` |
| 4b | start `vim`/`sleep` in the foreground, press **^C / ^Z / ▶ / ✕** on the **signal pad** | **CO-3** the backend calls `kill(-pgid, SIGINT/SIGTSTP/SIGCONT/SIGTERM)` on the job's real process group (targeted from the shell's own job table) |
| 5 | `vim` (or `top`), then **Ctrl+Z**, then `jobs`, `bg`, `fg` | **CO-3** suspension/resume with real terminal process groups |
| 6 | press **Ctrl+C** in the terminal | **CO-3** `SIGINT` delivered by the pty line discipline to the foreground process group |
| 7 | `echo hi > /tmp/a.txt` then `cat < /tmp/a.txt` | **CO-5** `open`/`dup2`/`close` redirection |
| 8 | start 3 background `sleep`s, `wait`, `jobs` | **CO-6** concurrent jobs, correct reaping, no zombies |

## Shell features

- Prompt `ForgeShell:user@host:~/dir$ ` with built-in line editing:
  arrows + history (↑/↓), `Ctrl+A/E/U/K/W/L`, `Ctrl+C`, `Ctrl+D` (EOF)
- Builtins (run in the shell's own process): `cd` (incl. `cd -`), `pwd`,
  `echo`, `jobs [-l]`, `fg [%N]`, `bg [%N]`, `kill [-SIG] %N|pid`,
  `wait`, `help`, `exit`
- External programs: `fork()` + `execvp()`, foreground `waitpid()`
- Background jobs with `&`, lowest-free job ids like bash
- Full job control: own session + controlling terminal (`setsid`/`TIOCSCTTY`),
  per-job process groups, `tcsetpgrp` hand-off, `SIGINT`/`SIGTSTP`/`SIGCONT`/
  `SIGTERM`/`SIGCHLD`
- `SIGCHLD` handler with `waitpid(WNOHANG|WUNTRACED)` reaping — no zombies;
  `pselect()` makes prompt notifications race-free (the `[1]+ Done …` line
  appears exactly when a job changes state)
- Pipelines `a | b | c`, redirections `< > >> 2> 2>> 2>&1`, quoting
  (`'…'`, `"…"`), `$VAR`/`${VAR}`/`~` expansion
- Publishes its **real** job table as JSON (`FORGESHELL_JOBS_FILE`) —
  foreground and background — while jobs run; the web UI reads this
  and nothing is fabricated
- The web **signal pad** delivers real signals to a job's process group
  via the backend (`kill(-pgid, …)`), targeted by the shell's own job
  table — a second, independent signal path besides the pty bytes

## Tests

| Test | Command | Coverage |
|------|---------|----------|
| Shell (27 checks) | `cd shell && python3 test_shell.py` | exec, pipes, redirection, bg jobs, jobs table, Ctrl+Z/bg/fg/Ctrl+C, kill, reaping, history, EOF/exit |
| Backend smoke | `cd backend && node test_ws.js` (server running) | WS round-trip, real job table over `/api/jobs`, kill + Done, signal pad (SIGINT to fg job, bg job untouched) |
| Browser | open `http://localhost:3000` | terminal round-trip, job cards, panel buttons |

## Troubleshooting

- **`shell binary not found` in the terminal** → run `make -C shell`.
- **`npm install` fails building `node-pty`** → `sudo apt install -y
  build-essential python3`.
- **Port 3000 busy** → `PORT=3001 npm start`.
- **White page / 404** → build the frontend: `cd frontend && npm run build`.
- **Opening from another machine** → backend binds `0.0.0.0`; for local
  use only, restrict with `HOST=127.0.0.1`.
