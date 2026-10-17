/*
 * useShellSession.js — WebSocket session to the ForgeShell backend.
 *
 * Everything typed here is forwarded to a REAL `forgeshell` process
 * running under a PTY on the server; everything shown in the jobs panel
 * comes from the shell's own job table (published by the C code).
 */
import { useCallback, useEffect, useRef, useState } from "react";

export function useShellSession() {
  const wsRef = useRef(null);
  const termRef = useRef(null); // xterm instance (set by Terminal.jsx)
  const retryRef = useRef(null);
  const [connected, setConnected] = useState(false);
  const [session, setSession] = useState(null);
  const [exitInfo, setExitInfo] = useState(null);
  const [jobs, setJobs] = useState([]);
  const [lastSignal, setLastSignal] = useState(null); // {kind,text,at}

  /* write shell output into the terminal (buffered if not ready yet) */
  const writeOut = useCallback((data) => {
    const t = termRef.current;
    if (t) t.write(data);
  }, []);

  /* send keystrokes to the shell */
  const sendInput = useCallback((data) => {
    const ws = wsRef.current;
    if (ws && ws.readyState === WebSocket.OPEN)
      ws.send(JSON.stringify({ type: "input", data }));
  }, []);

  const sendResize = useCallback((cols, rows) => {
    const ws = wsRef.current;
    if (ws && ws.readyState === WebSocket.OPEN)
      ws.send(JSON.stringify({ type: "resize", cols, rows }));
  }, []);

  const refreshJobs = useCallback(() => {
    const ws = wsRef.current;
    if (ws && ws.readyState === WebSocket.OPEN)
      ws.send(JSON.stringify({ type: "jobs" }));
  }, []);

  /* ask the backend to deliver a REAL signal to a job's process group */
  const sendSignal = useCallback((signal, jid) => {
    const ws = wsRef.current;
    if (ws && ws.readyState === WebSocket.OPEN)
      ws.send(JSON.stringify({ type: "signal", signal, jid }));
  }, []);

  useEffect(() => {
    let dead = false;

    function connect() {
      if (dead) return;
      const proto = window.location.protocol === "https:" ? "wss" : "ws";
      const ws = new WebSocket(`${proto}://${window.location.host}/ws`);
      wsRef.current = ws;

      ws.onopen = () => {
        setConnected(true);
        setExitInfo(null);
        // advertise the terminal size we actually have
        const t = termRef.current;
        if (t) sendResize(t.cols, t.rows);
      };

      ws.onmessage = (ev) => {
        let msg;
        try {
          msg = JSON.parse(ev.data);
        } catch {
          return;
        }
        switch (msg.type) {
          case "ready":
            setSession(msg.session);
            break;
          case "output":
            writeOut(msg.data);
            break;
          case "jobs":
            setJobs(msg.jobs || []);
            break;
          case "exit":
            setExitInfo(msg.code);
            setJobs([]);
            break;
          case "signal-ok":
            setLastSignal({
              kind: "ok",
              text: `${msg.signal} → job ${msg.jid} (process group)`,
              at: Date.now(),
            });
            break;
          case "signal-error":
            setLastSignal({
              kind: "error",
              text: msg.message || "signal failed",
              at: Date.now(),
            });
            break;
          default:
            break;
        }
      };

      ws.onclose = () => {
        setConnected(false);
        wsRef.current = null;
        if (!dead) {
          writeOut(
            "\r\n\x1b[33m[connection lost — reconnecting…]\x1b[0m\r\n"
          );
          retryRef.current = setTimeout(connect, 1500);
        }
      };

      ws.onerror = () => ws.close();
    }

    connect();
    return () => {
      dead = true;
      if (retryRef.current) clearTimeout(retryRef.current);
      if (wsRef.current) wsRef.current.close();
    };
  }, [sendResize, writeOut]);

  return {
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
  };
}
