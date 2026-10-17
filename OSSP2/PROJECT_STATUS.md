# ForgeShell — Project Implementation Status (Handoff)

**Date:** 2026-10-17
**Location:** `C:\Users\dell\OneDrive\Desktop\OSSP2` (WSL: `/mnt/c/Users/dell/OneDrive/Desktop/OSSP2`)
**Runtime:** WSL distro `Ubuntu-24.04`, Node v22.23.3, GCC, Python 3
**State:** Complete, fully tested, server running (`/api/health` → `{"ok":true,"shellBuilt":true}`)

---

## What this project is

A mini Linux shell ("ForgeShell") written in C with real background job
management, bridged to a browser UI through a Node backend. **Nothing is
simulated**: commands run via `fork()`/`execvp()` on a real PTY, and the
web jobs panel is fed by the C shell's own `waitpid()`-derived job table.

```
Browser (xterm.js, React)
   │  WebSocket  {input / output / jobs / signal}
   ▼
backend/server.js (Express + ws + node-pty)   — one forgeshell per WS connection
   │  PTY (/dev/pts/N)
   ▼
shell/forgeshell (C)  ──fork/execvp──►  real Linux processes
   │
   └── writes real job table to a JSON file (FORGESHELL_JOBS_FILE) ──► backend ──► UI
```

## File inventory (source lines)

| File | Lines | Role |
|---|---|---|
| `shell/main.c` | 426 | REPL, session/controlling-tty setup (`setsid`+`TIOCSCTTY`), raw-termios line editor (history, ↑/↓, Ctrl+A/E/U/K/W/C/D/L) |
| `shell/parser.c` | 264 | Quoting, `$VAR`/`${VAR}`/`~` expansion, pipes, `< > >> 2> 2>> 2>&1`, `&` |
| `shell/execute.c` | 291 | `fork`/`execvp`, pipelines with process groups, redirection (`open`/`dup2`/`close`), `waitpid(WUNTRACED)`, `tcsetpgrp` hand-off. **Publishes job JSON to the jobs file BEFORE waiting on fg jobs** (important fix, see below) |
| `shell/jobs.c` | 333 | Job table, SIGCHLD handler (`waitpid(WNOHANG\|WUNTRACED)`, no zombies), race-free `[1]+ Done` notifications via `pselect()`, JSON snapshot writer |
| `shell/builtins.c` | 265 | `cd` (incl. `cd -`) `pwd echo jobs [-l] fg bg kill wait help exit` |
| `shell/shell.h` + `*.h` | 160 | Shared types/prototypes |
| `shell/Makefile` | — | `make` → `./forgeshell`, `-Wall -Wextra`, zero warnings |
| `shell/test_shell.py` | — | **27 integration tests over a real PTY** (Ctrl+Z/bg/fg/Ctrl+C, kill, reaping, pipes, redirection, history, EOF/exit, job JSON) |
| `shell/run_tests.sh` | — | Wrapper to run the suite N times (stability) |
| `backend/server.js` | 269 | Express + ws + node-pty. REST `/api/health`, `/api/jobs`. WS messages: `input`, `resize`, `jobs`, `signal`. Signal path: re-reads the shell's jobs file **fresh at signal time**, then `process.kill(-pgid, SIG*)` |
| `backend/test_ws.js` | 141 | 9-check smoke test (round-trip, job table, kill+Done, signal pad: SIGINT to fg job, bg job untouched) |
| `frontend/src/App.jsx` | 129 | Layout: terminal left, jobs panel right |
| `frontend/src/components/Terminal.jsx` | 99 | xterm.js wiring |
| `frontend/src/components/JobsPanel.jsx` | 135 | Job cards + fg/bg/kill buttons (type real shell commands) |
| `frontend/src/components/SignalPad.jsx` | 58 | ^C INT / ^Z TSTP / ▶ CONT / ✕ TERM → backend `kill(-pgid,…)` |
| `frontend/src/hooks/useShellSession.js` | 143 | WebSocket session, auto-reconnect, `signal-ok`/`signal-error` acks |

Docs: `README.md` (exact run steps, syllabus demo table, troubleshooting), `backend/README.md`, `.gitignore`.
Build artifacts on disk: `shell/forgeshell` + `*.o`, `frontend/dist`, `backend/node_modules`, `backend/server.log`.

## How to run (inside WSL Ubuntu)

```bash
cd shell && make                                  # build (0 warnings)
cd ../backend && npm install                      # once
cd ../frontend && npm install && npm run build    # once / after UI changes
cd ../backend && npm start                        # → http://localhost:3000
```

Dev mode: backend `npm start` (3000) + frontend `npm run dev` (5173, proxies /ws + /api).
Config env vars: `PORT` (3000), `HOST` (0.0.0.0), `FORGESHELL_BIN` (../shell/forgeshell).

**Important WSL quirk:** backgrounding the server from a one-shot
`wsl.exe -- bash -lc '…'` call does NOT survive with `nohup … &`; use
`setsid node server.js > server.log 2>&1 < /dev/null &`. WSL's `/tmp`
has been unreliable (wiped mid-run) — put test logs in the project dir.

## Verification status (all green at handoff)

| Check | Result |
|---|---|
| `make clean && make` | exit 0, **0 warnings** |
| `python3 test_shell.py` (3 runs after last C change) | **27/27 passed** each run |
| `node test_ws.js` vs running server | **9/9 passed** (incl. signal pad targeting fg job, bg untouched) |
| Browser at localhost:3000 | round-trip, `[1] RUNNING` cards, kill → `[1]+ Done`, signal ack `✓ SIGTERM → job 1 (process group)`, 0 console errors |
| HTTP surface | `/`, `/api/health`, JS/CSS all 200 with correct content types |

## Key design decisions (do not regress)

1. **Job table is published by the C shell** to a JSON file
   (`FORGESHELL_JOBS_FILE`) on every state change; the backend forwards it.
   The browser never fabricates process data.
2. **Two independent signal paths:** (a) PTY bytes — panel buttons type
   `fg %1`/`bg %1`/`kill -TERM %1` into the shell; (b) the signal pad — the
   backend calls `process.kill(-pgid, SIG*)` on the job's real process
   group, targeted from the shell's job file.
3. **`execute.c` must publish the job file BEFORE `wait_for_job()`** —
   otherwise foreground jobs are invisible to the UI while running (real
   bug found and fixed).
4. **The backend re-reads the job file fresh at signal time** — its cached
   `state.jobs` can lag one watcher tick and signal the wrong job (real
   race, caught by a strict test assertion).
5. **Race-free `[1]+ Done` notifications:** SIGCHLD blocked + `pselect()`
   in the main loop (proven via `/proc/<pid>/SigBlk`).

## Bugs already found & fixed (history, for context)

- Child stopped itself via SIGTTOU before `tcsetpgrp` → fg hand-off broke.
- Builtins ignored redirection.
- `2>&1` leaked `1` as an argv entry.
- Lost `[1]+ Done` notifications — two distinct races, solved with `pselect()` + correct input mask.
- Wrong `dist` path in server.js.
- fg jobs never published while running (see decision 3).
- Backend signaling a stale job snapshot (see decision 4).

## Known limitations (by design / environment)

- Runs **only inside WSL** (Linux ELF binary + Linux PTY). Not Windows-native.
- One shell session per browser tab (multiple tabs = multiple independent sessions — works).
- The desktop app's preview-panel screenshot capture is flaky (compositing
  quirk); verify UI via accessibility snapshots / DOM reads instead.
- WSL instability on this machine: reboots kill the backend; restart with
  `cd backend && npm start`. OneDrive path adds I/O latency on `/mnt/c`.

## Incomplete / next steps

- **`start-forgeshell.bat` was requested but NOT created** (work interrupted
  mid-turn). Intended behavior: `.bat` at project root that (1) checks
  `/api/health`, (2) runs first-time builds if `shell/forgeshell`,
  `backend/node_modules`, `frontend/dist` are missing, (3) starts the
  server detached inside WSL via `setsid … &`, (4) polls health ~20s,
  (5) `start http://localhost:3000`. Must use `setsid`, not `nohup`.
- Other suggested ideas not yet done: double-click command execution in the
  terminal, multiple concurrent sessions with a session switcher, an
  strace/ltrace view, a `jobs -v` memory/CPU column, Windows-native port.
- `shell/.fsdbg.log` and `backend/server.log` are stray debug/runtime
  artifacts (safe to delete; consider gitignoring `.fsdbg.log`).
- `shell/*.o` and `shell/forgeshell` are build artifacts on disk; they are
  covered by `.gitignore` — confirm before committing.

## Git

**There is no git repository yet** (`git status` → "not a git repository").
All work is uncommitted files on disk. If version control is wanted, run
`git init` at the project root first; `.gitignore` is already in place and
covers `shell/*.o`, `shell/forgeshell`, `node_modules/`, `frontend/dist/`,
`*.log`, `.fsdbg.log`. Verify with `git status` before the first commit.
