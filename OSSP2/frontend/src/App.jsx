/*
 * App.jsx — ForgeShell web UI.
 *
 * Layout: header (identity + CO badges + connection state)
 *         main: xterm terminal  |  live jobs panel
 *         footer: quick command chips
 */
import { useShellSession } from "./hooks/useShellSession.js";
import Terminal from "./components/Terminal.jsx";
import JobsPanel from "./components/JobsPanel.jsx";

const CO_BADGES = [
  ["CO-1", "user space / syscalls"],
  ["CO-2", "fork · execvp · waitpid"],
  ["CO-3", "signals · job control"],
  ["CO-5", "pipes · redirection"],
  ["CO-6", "concurrency · reaping"],
];

export default function App() {
  const {
    termRef,
    connected,
    session,
    exitInfo,
    jobs,
    sendInput,
    sendResize,
    refreshJobs,
    sendSignal,
    lastSignal,
  } = useShellSession();

  const run = (cmd) => sendInput(cmd);

  return (
    <div className="flex h-full flex-col">
      {/* ---------------- header ---------------- */}
      <header className="flex items-center justify-between gap-4 border-b border-[#1a2231] bg-[#0b1019] px-4 py-2.5">
        <div className="flex items-center gap-3">
          <div className="flex h-8 w-8 items-center justify-center rounded-md bg-gradient-to-br from-sky-500 to-indigo-600 font-bold text-white">
            &gt;_
          </div>
          <div>
            <h1 className="text-sm font-semibold tracking-wide text-[#e6edf5]">
              ForgeShell
            </h1>
            <p className="text-[11px] text-[#5f6f85]">
              Mini Linux Shell with Background Job Management · OSSP
            </p>
          </div>
        </div>

        <div className="hidden items-center gap-1.5 lg:flex">
          {CO_BADGES.map(([co, label]) => (
            <span
              key={co}
              title={label}
              className="rounded border border-[#243148] bg-[#101827] px-2 py-0.5 text-[10px] font-medium text-[#7f93ad]"
            >
              {co}
            </span>
          ))}
        </div>

        <div className="flex items-center gap-3 text-[11px]">
          {session !== null && (
            <span className="text-[#5f6f85]">session #{session}</span>
          )}
          <span className="flex items-center gap-1.5">
            <span
              className={`h-2 w-2 rounded-full ${
                connected ? "bg-emerald-400" : "bg-rose-400"
              }`}
            />
            <span className={connected ? "text-emerald-300" : "text-rose-300"}>
              {connected ? "shell connected" : "disconnected"}
            </span>
          </span>
        </div>
      </header>

      {/* ---------------- main ---------------- */}
      <main className="flex min-h-0 flex-1">
        <section className="min-w-0 flex-1">
          <Terminal
            termRef={termRef}
            connected={connected}
            exitInfo={exitInfo}
            onInput={sendInput}
            onResize={sendResize}
          />
        </section>
        <JobsPanel
          jobs={jobs}
          connected={connected}
          onSend={sendInput}
          onRefresh={refreshJobs}
          onSignal={sendSignal}
          lastSignal={lastSignal}
        />
      </main>

      {/* ---------------- footer: quick chips ---------------- */}
      <footer className="flex flex-wrap items-center gap-2 border-t border-[#1a2231] bg-[#0b1019] px-4 py-2 text-[11px]">
        <span className="text-[#5f6f85]">try:</span>
        {[
          ["jobs -l", "jobs -l\n"],
          ["sleep 60 &", "sleep 60 &\n"],
          ["ps -ef | head", "ps -ef | head\n"],
          ["echo hi > /tmp/a.txt", "echo hi > /tmp/a.txt\n"],
          ["help", "help\n"],
        ].map(([label, cmd]) => (
          <button
            key={label}
            onClick={() => run(cmd)}
            disabled={!connected}
            className="rounded border border-[#2a3547] bg-[#101827] px-2 py-1 font-mono text-[11px] text-[#9fb0c6] hover:border-sky-500/60 hover:text-sky-300 disabled:opacity-40"
          >
            {label}
          </button>
        ))}
        <span className="ml-auto text-[#47566b]">
          everything runs in a real <code>forgeshell</code> process (C, PTY)
        </span>
      </footer>
    </div>
  );
}
