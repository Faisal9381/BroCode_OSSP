# ForgeShell backend

Node.js bridge between the browser and the real C shell.

```
browser  <--WebSocket-->  node-pty  <--pty-->  shell/forgeshell (C binary)
```

- `server.js` — Express HTTP server + WebSocket endpoint (`/ws`).
- Each WebSocket connection spawns **one real `forgeshell` process**
  attached to a pseudo-terminal (node-pty). Keystrokes go to the shell,
  shell output comes back as JSON frames `{type:"output", data}`.
- The shell publishes its real job table to `FORGESHELL_JOBS_FILE`
  (a JSON file). The server watches that file (200 ms) and forwards
  `{type:"jobs", jobs:[...]}` to the UI. **No process information is
  simulated in the browser.**

## REST endpoints

| Route         | Meaning                                        |
|---------------|------------------------------------------------|
| `GET /api/health` | liveness + whether the shell binary is built |
| `GET /api/jobs`   | latest real job snapshot of live sessions     |

## Run

```bash
npm install          # once
npm start            # http://localhost:3000
```

Environment variables:

| Var              | Default              | Meaning                     |
|------------------|----------------------|-----------------------------|
| `PORT`           | `3000`               | HTTP/WebSocket port         |
| `HOST`           | `0.0.0.0`            | bind address                |
| `FORGESHELL_BIN` | `../shell/forgeshell`| path of the C shell binary  |

Run the backend **inside WSL (Ubuntu)** — the shell binary is a Linux
ELF executable and needs a Linux PTY.
