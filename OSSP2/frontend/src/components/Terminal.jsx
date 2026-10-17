/*
 * Terminal.jsx — xterm.js surface wired to the real shell session.
 */
import { useEffect, useRef } from "react";
import { Terminal as XTerm } from "@xterm/xterm";
import { FitAddon } from "@xterm/addon-fit";

export default function Terminal({
  termRef,
  connected,
  exitInfo,
  onInput,
  onResize,
}) {
  const containerRef = useRef(null);
  const fitRef = useRef(null);

  useEffect(() => {
    const term = new XTerm({
      cursorBlink: true,
      fontSize: 14,
      fontFamily: '"JetBrains Mono", "Fira Code", Menlo, Consolas, monospace',
      theme: {
        background: "#0a0e14",
        foreground: "#d5dde5",
        cursor: "#ffcc66",
        selectionBackground: "#263041",
        black: "#0a0e14",
        red: "#ff6b6b",
        green: "#9dd17a",
        yellow: "#ffcc66",
        blue: "#6cb6ff",
        magenta: "#d9a0f0",
        cyan: "#6cd5e0",
        white: "#d5dde5",
      },
      allowProposedApi: true,
    });
    const fit = new FitAddon();
    term.loadAddon(fit);
    term.open(containerRef.current);
    fitRef.current = fit;

    try {
      fit.fit();
    } catch {
      /* container may have zero size for a frame; observer retries */
    }

    termRef.current = term;

    const inputSub = term.onData((data) => onInput(data));

    const ro = new ResizeObserver(() => {
      try {
        fit.fit();
        onResize(term.cols, term.rows);
      } catch {
        /* ignore transient zero-size frames */
      }
    });
    ro.observe(containerRef.current);

    return () => {
      inputSub.dispose();
      ro.disconnect();
      term.dispose();
      termRef.current = null;
    };
    // create the terminal exactly once
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  // surface state messages inside the terminal
  useEffect(() => {
    if (exitInfo !== null && exitInfo !== undefined) {
      termRef.current?.write(
        `\r\n\x1b[31m[shell exited with code ${exitInfo}]\x1b[0m\r\n`
      );
    }
  }, [exitInfo, termRef]);

  useEffect(() => {
    if (!connected && termRef.current) {
      /* reconnect notice is written by the session hook */
    }
  }, [connected, termRef]);

  return (
    <div className="relative h-full w-full bg-[#0a0e14]">
      <div ref={containerRef} className="h-full w-full p-2" />
      {!connected && (
        <div className="pointer-events-none absolute top-3 right-4 rounded-md border border-amber-500/40 bg-amber-500/10 px-3 py-1 text-xs text-amber-300">
          connecting to shell…
        </div>
      )}
    </div>
  );
}
