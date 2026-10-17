/*
 * SignalPad.jsx — deliver REAL kernel signals to a job's process group.
 *
 * Clicking a button does not fake anything: the backend calls
 * kill(-pgid, SIGINT/SIGTSTP/SIGCONT/SIGTERM) on the process group the
 * C shell created with setpgid().  Targets come from the shell's own
 * job table (foreground job by default).
 */
const BUTTONS = [
  ["INT", "^C", "SIGINT — interrupt foreground job", "text-rose-300"],
  ["TSTP", "^Z", "SIGTSTP — suspend to a stopped job", "text-amber-300"],
  ["CONT", "▶", "SIGCONT — continue a stopped job", "text-emerald-300"],
  ["TERM", "✕", "SIGTERM — terminate the job", "text-rose-300"],
];

export default function SignalPad({
  connected,
  hasRunningJob,
  lastSignal,
  onSignal,
}) {
  return (
    <div className="border-b border-[#1a2231] px-4 py-3">
      <div className="flex items-center justify-between">
        <h3 className="text-xs font-semibold tracking-wide text-[#8496ad] uppercase">
          Signal pad
        </h3>
        <span className="text-[10px] text-[#47566b]">kill(2) · process group</span>
      </div>
      <div className="mt-2 grid grid-cols-4 gap-1.5">
        {BUTTONS.map(([sig, glyph, title, color]) => (
          <button
            key={sig}
            disabled={!connected || !hasRunningJob}
            title={title}
            onClick={() => onSignal(sig)}
            className={`flex flex-col items-center rounded border border-[#2a3547] bg-[#101827] px-1 py-1.5 transition hover:border-sky-500/60 disabled:opacity-40 ${color}`}
          >
            <span className="text-sm leading-none">{glyph}</span>
            <span className="mt-1 text-[9px] font-semibold tracking-wider">
              {sig}
            </span>
          </button>
        ))}
      </div>
      {lastSignal && (
        <p
          className={`mt-2 truncate text-[10px] ${
            lastSignal.kind === "ok" ? "text-emerald-400/90" : "text-rose-400/90"
          }`}
          title={lastSignal.text}
        >
          {lastSignal.kind === "ok" ? "✓" : "✗"} {lastSignal.text}
        </p>
      )}
    </div>
  );
}
