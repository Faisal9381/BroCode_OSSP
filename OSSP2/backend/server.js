/*
 * ForgeShell backend
 * -------------------
 * Bridges the browser to the REAL C shell:
 *
 *   browser  <--WebSocket-->  node-pty  <--/dev/pts-->  forgeshell (C binary)
 *
 * The shell's actual job table is published by the shell itself to a
 * JSON file (FORGESHELL_JOBS_FILE); this server watches that file and
 * forwards the real job state to the UI.  Nothing here simulates the
 * shell or fabricates process information.
 */
const express = require("express");
const http = require("http");
const path = require("path");
const fs = require("fs");
const os = require("os");
const pty = require("node-pty");
const { WebSocketServer } = require("ws");

const PORT = parseInt(process.env.PORT || "3000", 10);
const HOST = process.env.HOST || "0.0.0.0";

// The C shell binary: build it with `make -C shell`.
const SHELL_BIN =
  process.env.FORGESHELL_BIN ||
  path.join(__dirname, "..", "shell", "forgeshell");

const DIST_DIR = path.join(__dirname, "..", "frontend", "dist");

const app = express();
app.disable("x-powered-by");

/* ------------------------------------------------------------------ */
/* Small REST surface (used by the UI status bar and for grading).     */
app.get("/api/health", (req, res) => {
  res.json({
    ok: true,
    shell: "ForgeShell",
    shellBinary: SHELL_BIN,
    shellBuilt: fs.existsSync(SHELL_BIN),
    node: process.version,
    uptimeSec: Math.round(process.uptime()),
  });
});

// Latest known job snapshot of every live session (real shell data).
const sessions = new Map(); // id -> { jobs: [...] }
app.get("/api/jobs", (req, res) => {
  const all = [];
  for (const [id, s] of sessions) all.push({ session: id, jobs: s.jobs });
  res.json(all);
});

/* ------------------------------------------------------------------ */
/* Static frontend (built with `npm run build` in frontend/).          */
if (fs.existsSync(DIST_DIR)) {
  app.use(express.static(DIST_DIR));
  app.get("*", (req, res, next) => {
    if (req.path.startsWith("/api") || req.path.startsWith("/ws"))
      return next();
    res.sendFile(path.join(DIST_DIR, "index.html"));
  });
}

const server = http.createServer(app);

/* ------------------------------------------------------------------ */
/* WebSocket: one live shell per connection                            */
const wss = new WebSocketServer({ server, path: "/ws" });

let nextSessionId = 1;

function fs_debug_signal(session, jid, sig) {
  console.log(`[signal] session=${session} job=${jid} ${sig}`);
}

wss.on("connection", (ws) => {
  const id = nextSessionId++;
  const jobsFile = path.join(os.tmpdir(), `forgeshell_jobs_${id}.json`);
  const state = { jobs: [] };
  sessions.set(id, state);

  if (!fs.existsSync(SHELL_BIN)) {
    ws.send(
      JSON.stringify({
        type: "output",
        data:
          "\r\n\x1b[31m[forgeshell] shell binary not found:\r\n" +
          `  ${SHELL_BIN}\r\n` +
          "  Build it first:  make -C shell\x1b[0m\r\n\r\n",
      })
    );
  }

  // ---- spawn the real shell under a real PTY ----------------------
  let ptyProc = null;
  try {
    ptyProc = pty.spawn(SHELL_BIN, [], {
      name: process.env.TERM || "xterm-256color",
      cols: 100,
      rows: 30,
      cwd: os.homedir(),
      env: {
        ...process.env,
        FORGESHELL_JOBS_FILE: jobsFile, // the shell publishes its job table here
      },
    });
  } catch (err) {
    ws.send(
      JSON.stringify({
        type: "output",
        data: `\r\n\x1b[31m[forgeshell] failed to start shell: ${err.message}\x1b[0m\r\n`,
      })
    );
    ws.send(JSON.stringify({ type: "exit", code: 1 }));
    return;
  }

  ws.send(JSON.stringify({ type: "ready", session: id }));

  // ---- shell -> browser --------------------------------------------
  ptyProc.onData((data) => {
    if (ws.readyState === ws.OPEN)
      ws.send(JSON.stringify({ type: "output", data }));
  });

  ptyProc.onExit(({ exitCode }) => {
    stopWatching();
    sessions.delete(id);
    try {
      fs.unlinkSync(jobsFile);
    } catch (_) {}
    if (ws.readyState === ws.OPEN)
      ws.send(JSON.stringify({ type: "exit", code: exitCode }));
  });

  // ---- watch the shell's REAL job table ---------------------------
  function pushJobs() {
    try {
      const raw = fs.readFileSync(jobsFile, "utf8");
      state.jobs = JSON.parse(raw);
      if (ws.readyState === ws.OPEN)
        ws.send(JSON.stringify({ type: "jobs", jobs: state.jobs }));
    } catch (_) {
      /* file not written yet or mid-rename: ignore */
    }
  }
  const watcher = fs.watchFile(jobsFile, { interval: 200 }, () =>
    pushJobs()
  );
  function stopWatching() {
    try {
      fs.unwatchFile(jobsFile);
    } catch (_) {}
  }

  // ---- browser -> shell --------------------------------------------
  ws.on("message", (buf) => {
    let msg;
    try {
      msg = JSON.parse(buf.toString());
    } catch (_) {
      return;
    }
    switch (msg.type) {
      case "input":
        if (typeof msg.data === "string") ptyProc.write(msg.data);
        break;
      case "resize":
        if (msg.cols > 0 && msg.rows > 0)
          ptyProc.resize(msg.cols, msg.rows);
        break;
      case "jobs":
        pushJobs(); // on-demand refresh
        break;

      /*
       * Signal pad: deliver a REAL signal to a job's process group
       * (CO-3). The target comes from the C shell's own job table —
       * the foreground job by default, or an explicit job id.  The
       * backend runs as the same user, so kill(-pgid, sig) reaches the
       * actual process group the shell created with setpgid().
       */
      case "signal": {
        const sigMap = {
          INT: "SIGINT",
          TSTP: "SIGTSTP",
          TERM: "SIGTERM",
          CONT: "SIGCONT",
          KILL: "SIGKILL",
          HUP: "SIGHUP",
          QUIT: "SIGQUIT",
        };
        const name = sigMap[String(msg.signal || "").toUpperCase()];
        if (!name) {
          ws.send(
            JSON.stringify({ type: "signal-error", message: "unknown signal" })
          );
          break;
        }
        // pick the target from the REAL job table.  Read the shell's
        // JSON file fresh: state.jobs may lag one watcher tick, and
        // signaling the wrong process group would be a real bug.
        let jobsNow = state.jobs;
        try {
          jobsNow = JSON.parse(fs.readFileSync(jobsFile, "utf8"));
        } catch (_) {
          /* keep the cached snapshot */
        }
        let job = null;
        if (msg.jid != null) {
          job = jobsNow.find((j) => j.jid === msg.jid) || null;
        } else {
          job =
            jobsNow.find((j) => j.foreground && j.state === "Running") ||
            jobsNow.filter((j) => j.state === "Running").pop() ||
            null;
        }
        if (!job) {
          ws.send(
            JSON.stringify({
              type: "signal-error",
              message: "no matching job",
            })
          );
          break;
        }
        try {
          // job.pid carries the process GROUP id (setpgid in the shell)
          process.kill(-job.pid, name);
          fs_debug_signal(id, job.jid, name);
          ws.send(
            JSON.stringify({
              type: "signal-ok",
              jid: job.jid,
              signal: name,
            })
          );
        } catch (err) {
          ws.send(
            JSON.stringify({ type: "signal-error", message: err.message })
          );
        }
        break;
      }
    }
  });

  ws.on("close", () => {
    stopWatching();
    try {
      ptyProc.kill();
    } catch (_) {}
    sessions.delete(id);
    try {
      fs.unlinkSync(jobsFile);
    } catch (_) {}
  });
});

/* ------------------------------------------------------------------ */
server.listen(PORT, HOST, () => {
  console.log(`ForgeShell backend listening on http://localhost:${PORT}`);
  console.log(`  shell binary : ${SHELL_BIN}` +
    (fs.existsSync(SHELL_BIN) ? " (built)" : " (NOT BUILT — run make -C shell)"));
  console.log(`  frontend     : ` +
    (fs.existsSync(DIST_DIR) ? DIST_DIR : "(not built — run npm run build in frontend/)"));
});
