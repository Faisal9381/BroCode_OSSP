/*
 * JobsPanel.jsx — live view of the C shell's REAL job table.
 *
 * Data source: the shell writes its job table (jids, pids, state) to a
 * JSON file on every change; the backend forwards it over the WebSocket.
 * Buttons do not fake anything: they type the shell's own commands
 * (`fg %1`, `bg %1`, `kill -TERM %1`) into the terminal.
 */
import SignalPad from "./SignalPad.jsx";

const STATE_STYLE = {
  Running: "bg-emerald-500/15 text-emerald-300 border-emerald-500/40",
  Stopped: "bg-amber-500/15 text-amber-300 border-amber-500/40",
  Done: "bg-sky-500/15 text-sky-300 border-sky-500/40",
};

function JobCard({ job, onSend, connected }) {
  const pct = `%${job.jid}`;
  const chip =
    STATE_STYLE[job.state] || "bg-zinc-500/15 text-zinc-300 border-zinc-500/40";

  const btn =
    "rounded px-2 py-1 text-[11px] font-medium transition " +
    "border border-[#2a3547] bg-[#131a26] text-[#9fb0c6] " +
    "hover:border-sky-500/60 hover:text-sky-300 disabled:opacity-40";

  return (
    <div className="rounded-lg border border-[#1e2836] bg-[#0d131d] p-3">
      <div className="flex items-center justify-between gap-2">
        <span className="font-mono text-sm font-semibold text-[#e6edf5]">
          [{job.jid}]
        </span>
        <span
          className={`rounded border px-2 py-0.5 text-[10px] uppercase tracking-wide ${chip}`}
        >
          {job.state}
        </span>
      </div>
      <div className="mt-2 truncate font-mono text-xs text-[#9fb0c6]"
           title={job.command}>
        {job.command}
      </div>
      <div className="mt-1 text-[11px] text-[#5f6f85]">
        pid/pgid <span className="font-mono text-[#8496ad]">{job.pid}</span>
        {job.foreground ? " · foreground" : " · background"}
      </div>
      <div className="mt-2 flex gap-1.5">
        <button
          className={btn}
          disabled={!connected}
          onClick={() => onSend(`fg ${pct}\n`)}
          title={`fg ${pct} — bring to foreground`}
        >
          ▶ fg
        </button>
        <button
          className={btn}
          disabled={!connected || job.state !== "Stopped"}
          onClick={() => onSend(`bg ${pct}\n`)}
          title={`bg ${pct} — resume in background`}
        >
          ⏸ bg
        </button>
        <button
          className={btn}
          disabled={!connected}
          onClick={() => onSend(`kill -TERM ${pct}\n`)}
          title={`kill -TERM ${pct}`}
        >
          ✕ kill
        </button>
      </div>
    </div>
  );
}

export default function JobsPanel({
  jobs,
  connected,
  onSend,
  onRefresh,
  onSignal,
  lastSignal,
}) {
  const hasRunningJob = jobs.some((j) => j.state === "Running");
  return (
    <aside className="flex h-full w-80 shrink-0 flex-col border-l border-[#1a2231] bg-[#0b1019]">
      <div className="flex items-center justify-between border-b border-[#1a2231] px-4 py-3">
        <div>
          <h2 className="text-sm font-semibold text-[#e6edf5]">Jobs</h2>
          <p className="text-[11px] text-[#5f6f85]">
            real job table from the C shell
          </p>
        </div>
        <button
          onClick={onRefresh}
          className="rounded border border-[#2a3547] px-2 py-1 text-[11px] text-[#9fb0c6] hover:border-sky-500/60 hover:text-sky-300"
          title="Request a refresh from the shell"
        >
          ↻ refresh
        </button>
      </div>

      <SignalPad
        connected={connected}
        hasRunningJob={hasRunningJob}
        lastSignal={lastSignal}
        onSignal={onSignal}
      />

      <div className="thin-scroll flex-1 space-y-2 overflow-y-auto p-3">
        {jobs.length === 0 ? (
          <div className="rounded-lg border border-dashed border-[#223047] p-4 text-center">
            <p className="text-xs text-[#5f6f85]">No background jobs.</p>
            <p className="mt-2 text-[11px] text-[#47566b]">
              Try <code className="text-sky-400">sleep 30 &amp;</code> in the
              terminal.
            </p>
          </div>
        ) : (
          jobs.map((j) => (
            <JobCard key={j.jid} job={j} onSend={onSend} connected={connected} />
          ))
        )}
      </div>

      <div className="border-t border-[#1a2231] px-4 py-3 text-[11px] leading-relaxed text-[#5f6f85]">
        Buttons type real shell commands (<span className="font-mono">fg</span>,{" "}
        <span className="font-mono">bg</span>,{" "}
        <span className="font-mono">kill</span>) — the C shell performs the
        actual <code>killpg</code>/<code>SIGCONT</code> calls.
      </div>
    </aside>
  );
}
