#!/usr/bin/env python3
"""
test_shell.py — integration tests for ForgeShell.

Drives the shell through a REAL pseudo-terminal (pty.fork) because job
control (Ctrl+Z / Ctrl+C / background jobs) only works on a tty.

Synchronization trick: the shell redraws its prompt while typing, so a
plain "$ " regex is ambiguous.  After each command we send a unique
marker command (`echo __syncN__`); seeing its OUTPUT proves the shell
has completely finished the previous command and is waiting again.

Run:  python3 test_shell.py
"""
import json
import os
import pty
import re
import select
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, "forgeshell")
JOBS_FILE = "/tmp/forgeshell_test_jobs.json"

PASSED = 0
FAILED = 0


class Shell:
    def __init__(self):
        env = dict(os.environ)
        env["TERM"] = "xterm-256color"
        env["FORGESHELL_JOBS_FILE"] = JOBS_FILE
        env["FORGESHELL_DEBUG"] = os.path.join(HERE, ".fsdbg.log")
        self.pid, self.fd = pty.fork()
        if self.pid == 0:          # child: the shell under test
            os.chdir(os.path.expanduser("~"))
            os.execve(BIN, [BIN], env)
            os._exit(127)
        self.buf = ""
        self.sync_n = 0

    def expect(self, pattern, timeout=8):
        deadline = time.time() + timeout
        while time.time() < deadline:
            r, _, _ = select.select([self.fd], [], [], 0.1)
            if r:
                try:
                    data = os.read(self.fd, 4096)
                except OSError:
                    break
                if not data:
                    break
                self.buf += data.decode("utf-8", errors="replace")
            if re.search(pattern, self.buf):
                out, self.buf = self.buf, ""
                return out
        raise TimeoutError(
            f"pattern {pattern!r} not seen within {timeout}s\n"
            f"--- last output ---\n{self.buf[-800:]}")

    def sendline(self, s):
        os.write(self.fd, (s + "\n").encode())

    def sync(self, timeout=8):
        """Wait until the shell is provably idle at its prompt.
        The marker must be seen *at the start of an output line*, which
        typed text can never produce — no premature matches."""
        self.sync_n += 1
        marker = f"__sync{self.sync_n}__"
        self.buf = ""
        self.sendline(f"echo {marker}")
        self.expect(r"\r\n" + re.escape(marker) + r"\r\n", timeout)

    def cmd(self, line, pattern=None, timeout=8):
        """Run a command; optionally match its output, then wait for idle.
        Without a pattern, returns everything printed up to the sync
        marker (i.e. the command's full output)."""
        self.buf = ""
        self.sendline(line)
        if pattern:
            out = self.expect(pattern, timeout)
            self.sync(timeout)
            return out
        self.sync_n += 1
        marker = f"__sync{self.sync_n}__"
        self.sendline(f"echo {marker}")
        return self.expect(r"\r\n" + re.escape(marker) + r"\r\n", timeout)

    def close(self):
        try:
            os.close(self.fd)
        except OSError:
            pass


def check(name, cond, detail=""):
    global PASSED, FAILED
    if cond:
        PASSED += 1
        print(f"  PASS  {name}")
    else:
        FAILED += 1
        print(f"  FAIL  {name}   {detail}")


def jobs_json():
    try:
        with open(JOBS_FILE) as f:
            return json.load(f)
    except Exception:
        return None


def main():
    if not os.path.exists(BIN):
        print("build the shell first:  make")
        return 1
    if os.name != "posix":
        print("these tests need a POSIX system (Linux / WSL)")
        return 1

    s = Shell()

    print("\n== CO-1: basic interactive execution ==")
    out = s.expect(r"Type 'help' for built-in commands")
    s.sync()
    check("banner + prompt shown", "ForgeShell" in out)

    out = s.cmd("echo hello forgeshell", r"hello forgeshell\r\n")
    check("echo builtin executes", "hello forgeshell" in out)

    out = s.cmd("definitely-not-a-command-xyz",
                r"forgeshell: definitely-not-a-command-xyz: ")
    check("unknown command error", "No such file or directory" in out)

    out = s.cmd("uname -s", r"\r\nLinux\r\n")
    check("external program via fork/execvp", "Linux" in out)

    print("\n== CO-5: redirection and pipes ==")
    s.cmd("echo redir-test > /tmp/fs_redir.txt")
    out = s.cmd("cat /tmp/fs_redir.txt", r"redir-test\r\n")
    check("output redirection > and re-read", "redir-test" in out)

    out = s.cmd("ls /etc/hostname | wc -l", r"\r\n\s*1\r\n")
    check("pipeline |", re.search(r"\b1\b", out) is not None)

    out = s.cmd("ls /nonexistent-path-abc 2>&1 | wc -l", r"\r\n\s*1\r\n")
    check("stderr redirect 2>&1 into pipe", re.search(r"\b1\b", out) is not None)

    out = s.cmd("cat < /tmp/fs_redir.txt", r"redir-test\r\n")
    check("input redirection <", "redir-test" in out)

    print("\n== CO-2/CO-3: background jobs, jobs table, signals ==")
    out = s.cmd("sleep 60 &", r"\[1\] \d+\r\n")
    check("background launch prints [1] pid", re.search(r"\[1\] \d+", out) is not None)

    out = s.cmd("sleep 61 &", r"\[2\] \d+\r\n")
    check("second background job gets id 2", re.search(r"\[2\] \d+", out) is not None)

    out = s.cmd("jobs -l", r"sleep 61")
    check("jobs -l lists both running jobs",
          "sleep 60" in out and "sleep 61" in out and "Running" in out)

    data = jobs_json()
    check("JSON status file reflects REAL job table",
          data is not None and len(data) == 2 and
          all(j["state"] == "Running" for j in data),
          str(data))

    out = s.cmd("kill -TERM %1", r"\[1\]\+ Done\s+sleep 60")
    check("kill %1 terminates job, Done notification", "sleep 60" in out)

    out = s.cmd("jobs")
    check("only job 2 remains", "sleep 61" in out and "sleep 60" not in out)

    s.cmd("kill -TERM %2")          # clean up before the wait test

    print("\n== Ctrl+Z suspend, bg, fg resume ==")
    s.buf = ""
    s.sendline("sleep 62")
    time.sleep(0.8)
    os.write(s.fd, b"\x1a")                        # Ctrl+Z
    out = s.expect(r"\[1\]\+ Stopped\s+sleep 62")
    s.sync()
    check("Ctrl+Z stops foreground job", "Stopped" in out and "sleep 62" in out)

    out = s.cmd("bg", r"\[1\]\+ sleep 62 &")
    check("bg resumes job in background", "sleep 62" in out)

    data = jobs_json()
    check("JSON shows resumed job as Running",
          data is not None and any(j["command"] == "sleep 62" and
                                   j["state"] == "Running" for j in data),
          str(data))

    s.buf = ""
    s.sendline("fg %1")
    out = s.expect(r"\r\nsleep 62\r\n")   # fg prints the job's command
    time.sleep(0.8)
    os.write(s.fd, b"\x03")                        # Ctrl+C
    time.sleep(0.5)
    s.sync()               # times out if the shell is still waiting
    check("fg brings job back, Ctrl+C kills it", "sleep 62" in out)

    out = s.cmd("jobs")
    check("no jobs remain after kill", "sleep 6" not in out)

    print("\n== CO-6: reaping, no zombies ==")
    s.cmd("sleep 0.3 &")
    s.cmd("wait")
    out = s.cmd("jobs")
    check("finished bg job fully reaped (jobs table empty)",
          "sleep 0.3" not in out)
    data = jobs_json()
    check("JSON status file also empty after wait", data == [], str(data))

    print("\n== misc: cd, history (arrow up), env expansion, help, exit ==")
    s.cmd("cd /tmp")
    out = s.cmd("pwd", r"/tmp\r\n")
    check("cd builtin changes directory", "/tmp" in out)

    out = s.cmd("cd /no-such-directory-xyz", r"cd: /no-such-directory-xyz")
    check("cd error message", "No such file" in out)

    s.cmd("echo history-marker", r"history-marker\r\n")
    s.buf = ""
    time.sleep(0.3)                                 # let the prompt settle
    # NOTE: the sync() helper's own `echo __syncN__` line is the newest
    # history entry, so we need TWO up-arrows to reach "echo
    # history-marker".  (If this ever breaks, the helper changed.)
    os.write(s.fd, b"\x1b[A\x1b[A")                # up, up
    time.sleep(0.4)
    os.write(s.fd, b"\r")                          # run the recalled line
    # the OUTPUT of the re-run command (typed text can never match this)
    out = s.expect(r"\r\nhistory-marker\r\n", 8)
    s.sync()
    check("history: arrow-up recalls previous line", "history-marker" in out)

    user = os.environ.get("USER", "")
    out = s.cmd("echo $USER", re.escape(user) + r"\r\n") if user else s.cmd("echo $USER")
    check("env expansion $USER", user and user in out)

    out = s.cmd("help", r"built-in commands")
    check("help builtin lists commands", "fg" in out and "jobs" in out)

    s.buf = ""
    s.sendline("exit")
    _, status = os.waitpid(s.pid, 0)
    check("exit builtin terminates shell cleanly", os.WIFEXITED(status))
    s.close()

    print(f"\n{PASSED} passed, {FAILED} failed")
    return 0 if FAILED == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
