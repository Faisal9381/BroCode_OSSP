/*
 * test_ws.js — smoke test for the ForgeShell backend.
 *
 * Connects over WebSocket, drives the real shell and checks:
 *   1. banner + prompt arrive
 *   2. a command round-trips (echo)
 *   3. a background job shows up in the REAL job table (/api/jobs)
 *   4. kill via the shell removes it
 *
 * Run (server must be running):  node test_ws.js
 */
const WebSocket = require("ws");
const http = require("http");

const URL = process.env.WS_URL || "ws://localhost:3000/ws";
let buf = "";
let jobsMsg = null;

function expect(re, ms = 8000) {
  return new Promise((resolve, reject) => {
    const t0 = Date.now();
    const iv = setInterval(() => {
      if (re.test(buf)) {
        clearInterval(iv);
        resolve();
      } else if (Date.now() - t0 > ms) {
        clearInterval(iv);
        reject(new Error(`timeout waiting for ${re}\n--- buffer tail ---\n${buf.slice(-600)}`));
      }
    }, 50);
  });
}

function getJson(path) {
  return new Promise((resolve, reject) => {
    http.get({ host: "localhost", port: 3000, path }, (res) => {
      let d = "";
      res.on("data", (c) => (d += c));
      res.on("end", () => resolve(JSON.parse(d)));
    }).on("error", reject);
  });
}

async function main() {
  const health = await getJson("/api/health");
  console.log("health:", health);
  if (!health.ok) throw new Error("health check failed");

  const ws = new WebSocket(URL);
  await new Promise((res, rej) => {
    ws.once("open", res);
    ws.once("error", rej);
  });

  ws.on("message", (m) => {
    const msg = JSON.parse(m.toString());
    if (msg.type === "output") buf += msg.data;
    if (msg.type === "jobs") jobsMsg = msg.jobs;
  });

  const send = (s) => ws.send(JSON.stringify({ type: "input", data: s }));

  await expect(/ForgeShell/, 10000);
  console.log("PASS  banner received");

  send("echo ws-roundtrip-ok\n");
  await expect(/ws-roundtrip-ok/);
  console.log("PASS  echo round-trip");

  send("sleep 45 &\n");
  await expect(/\[1\] \d+/);
  console.log("PASS  background job launched");

  // the shell publishes its job table; server forwards it
  await expect(/"command":"sleep 45"/, 5000).catch(() => {});
  let jobs = await getJson("/api/jobs");
  const live = jobs.some((s) =>
    s.jobs.some((j) => j.command.includes("sleep 45"))
  );
  if (!live) {
    // give the file watcher a moment
    await new Promise((r) => setTimeout(r, 1000));
    jobs = await getJson("/api/jobs");
  }
  const ok = jobs.some((s) =>
    s.jobs.some((j) => j.command.includes("sleep 45") && j.state === "Running")
  );
  if (!ok) throw new Error("job table does not show sleep 45: " + JSON.stringify(jobs));
  console.log("PASS  real job table exposed via /api/jobs");

  /* --- signal pad: real kill(-pgid, SIGINT) on the fg process group --- */
  send("sleep 70\n");            // foreground job this time
  await new Promise((r) => setTimeout(r, 900));
  let ack = null;
  const onMsg = (m) => {
    const msg = JSON.parse(m.toString());
    if (msg.type === "signal-ok" || msg.type === "signal-error") ack = msg;
  };
  ws.on("message", onMsg);
  ws.send(JSON.stringify({ type: "signal", signal: "INT" }));
  await new Promise((r) => setTimeout(r, 1200));
  ws.off("message", onMsg);
  if (!ack || ack.type !== "signal-ok")
    throw new Error("signal pad did not ack: " + JSON.stringify(ack));
  if (ack.jid !== 2)
    throw new Error(
      `pad signaled job ${ack.jid}, expected foreground job 2: ${JSON.stringify(ack)}`
    );
  console.log(`PASS  signal pad: ${ack.signal} delivered to job ${ack.jid}`);
  send("echo after-sigint-ok\n");   // proves the fg job died and we're back
  await expect(/after-sigint-ok/);
  console.log("PASS  fg job interrupted, shell back at prompt");

  // the pad must have hit ONLY the foreground job: sleep 45 survives
  await new Promise((r) => setTimeout(r, 600));
  const afterSig = await getJson("/api/jobs");
  if (!afterSig.some((s) => s.jobs.some((j) => j.command.includes("sleep 45"))))
    throw new Error("background sleep 45 died — pad signaled the wrong job");
  console.log("PASS  pad targeted fg job only; background job untouched");

  send("kill -TERM %1\n");
  await expect(/\[1\]\+ Done/);
  console.log("PASS  kill via shell, Done notification");

  await new Promise((r) => setTimeout(r, 800));
  jobs = await getJson("/api/jobs");
  const gone = !jobs.some((s) =>
    s.jobs.some((j) => j.command.includes("sleep 45"))
  );
  if (!gone) throw new Error("job still in table after kill: " + JSON.stringify(jobs));
  console.log("PASS  job removed from table after kill");

  ws.close();
  console.log("\nbackend smoke test: all passed");
  process.exit(0);
}

main().catch((e) => {
  console.error("FAIL ", e.message);
  process.exit(1);
});
