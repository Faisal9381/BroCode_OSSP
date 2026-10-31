import express from "express";
import cors from "cors";
import { execFile } from "child_process";
import path from "path";
import { fileURLToPath } from "url";
import fs from "fs";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const app = express();
const PORT = 5000;

// Path to compiled C adapter
const ADAPTER_BIN = path.resolve(__dirname, "../adapter");

app.use(cors());
app.use(express.json({ limit: "50mb" }));

// Helper to run C adapter binary safely without a shell
function runAdapter(args) {
  return new Promise((resolve, reject) => {
    // execFile does not invoke a shell, preventing arbitrary command injection
    execFile(ADAPTER_BIN, args, { maxBuffer: 10 * 1024 * 1024 }, (error, stdout, stderr) => {
      if (error) {
        return reject({ error, stderr: stderr || stdout });
      }
      resolve({ stdout, stderr });
    });
  });
}

// 0. Initial directory
app.get("/api/current-dir", (req, res) => {
  const initialDir = path.resolve(__dirname, "..");
  res.json({ currentDir: initialDir });
});

// 1. List directory contents
app.get("/api/list", async (req, res) => {
  const dirPath = req.query.path ? path.resolve(req.query.path) : path.resolve(__dirname, "..");
  try {
    const { stdout } = await runAdapter(["list-json", dirPath]);
    const items = JSON.parse(stdout);
    items.sort((a, b) => {
      if (a.isDirectory && !b.isDirectory) return -1;
      if (!a.isDirectory && b.isDirectory) return 1;
      return a.name.localeCompare(b.name, undefined, { sensitivity: "base" });
    });
    res.json({ path: dirPath, items });
  } catch (err) {
    console.error("List error:", err);
    res.status(500).json({
      error: "Failed to list directory contents",
      details: err.stderr || err.message || String(err),
    });
  }
});

// 2. Read file contents
app.get("/api/read", async (req, res) => {
  const filePath = req.query.path;
  if (!filePath) {
    return res.status(400).json({ error: "File path is required" });
  }
  const targetPath = path.resolve(filePath);

  try {
    const { stdout } = await runAdapter(["read-file", targetPath]);
    
    // Extract contents from file_ops.c read_file() output
    let content = stdout;
    const startTag = "--- File Contents ---";
    const endTag = "---------------------";
    const startIndex = stdout.indexOf(startTag);
    const endIndex = stdout.lastIndexOf(endTag);

    if (startIndex !== -1 && endIndex !== -1 && endIndex > startIndex) {
      content = stdout.substring(startIndex + startTag.length, endIndex);
      if (content.startsWith("\n")) content = content.substring(1);
      if (content.endsWith("\n")) content = content.substring(0, content.length - 1);
    }

    res.json({ path: targetPath, content, raw: stdout });
  } catch (err) {
    console.error("Read error:", err);
    res.status(500).json({
      error: "Failed to read file",
      details: err.stderr || err.message || String(err),
    });
  }
});

// 3. Save / Update file contents
app.post("/api/save-file", (req, res) => {
  const { filePath, content } = req.body;
  if (!filePath || content === undefined) {
    return res.status(400).json({ error: "File path and content are required" });
  }
  const targetPath = path.resolve(filePath);

  const child = execFile(ADAPTER_BIN, ["update-file", targetPath], (error, stdout, stderr) => {
    if (error) {
      console.error("Save file error:", error, stderr);
      return res.status(500).json({
        error: "Failed to save file",
        details: stderr || stdout || error.message,
      });
    }
    res.json({ message: "File saved successfully", path: targetPath, output: stdout });
  });

  child.stdin.write(content);
  child.stdin.end();
});

// 4. Create a file
app.post("/api/create-file", async (req, res) => {
  const { dirPath, name } = req.body;
  if (!dirPath || !name) {
    return res.status(400).json({ error: "Directory path and file name are required" });
  }
  const targetPath = path.resolve(dirPath, name);

  try {
    const { stdout } = await runAdapter(["create-file", targetPath]);
    res.json({ message: "File created successfully", path: targetPath, output: stdout });
  } catch (err) {
    console.error("Create file error:", err);
    res.status(500).json({
      error: "Failed to create file",
      details: err.stderr || err.message || String(err),
    });
  }
});

// 5. Create a directory
app.post("/api/create-dir", async (req, res) => {
  const { dirPath, name } = req.body;
  if (!dirPath || !name) {
    return res.status(400).json({ error: "Directory path and folder name are required" });
  }
  const targetPath = path.resolve(dirPath, name);

  try {
    const { stdout } = await runAdapter(["create-dir", targetPath]);
    res.json({ message: "Directory created successfully", path: targetPath, output: stdout });
  } catch (err) {
    console.error("Create directory error:", err);
    res.status(500).json({
      error: "Failed to create directory",
      details: err.stderr || err.message || String(err),
    });
  }
});

// 6. Rename a file or directory
app.post("/api/rename", async (req, res) => {
  const { oldPath, newPath } = req.body;
  if (!oldPath || !newPath) {
    return res.status(400).json({ error: "Old path and new path are required" });
  }
  const resolvedOld = path.resolve(oldPath);
  const resolvedNew = path.resolve(newPath);

  try {
    const { stdout } = await runAdapter(["rename", resolvedOld, resolvedNew]);
    res.json({ message: "Renamed successfully", oldPath: resolvedOld, newPath: resolvedNew, output: stdout });
  } catch (err) {
    console.error("Rename error:", err);
    res.status(500).json({
      error: "Failed to rename",
      details: err.stderr || err.message || String(err),
    });
  }
});

// 7. Delete a file or directory
app.post("/api/delete", async (req, res) => {
  const { targetPath, isDirectory } = req.body;
  if (!targetPath) {
    return res.status(400).json({ error: "Target path is required" });
  }
  const resolvedPath = path.resolve(targetPath);

  try {
    const cmd = isDirectory ? "delete-dir" : "delete-file";
    const { stdout } = await runAdapter([cmd, resolvedPath]);
    res.json({ message: "Deleted successfully", path: resolvedPath, output: stdout });
  } catch (err) {
    console.error("Delete error:", err);
    res.status(500).json({
      error: "Failed to delete item",
      details: err.stderr || err.message || String(err),
    });
  }
});

// 8. Get File/Directory Info
app.get("/api/info", async (req, res) => {
  const targetPath = req.query.path;
  if (!targetPath) {
    return res.status(400).json({ error: "Path is required" });
  }
  const resolvedPath = path.resolve(targetPath);

  try {
    const { stdout } = await runAdapter(["info", resolvedPath]);
    res.json({ path: resolvedPath, info: stdout });
  } catch (err) {
    console.error("Info error:", err);
    res.status(500).json({
      error: "Failed to get info",
      details: err.stderr || err.message || String(err),
    });
  }
});

// Static frontend serving if dist exists
const distPath = path.resolve(__dirname, "../frontend/dist");
if (fs.existsSync(distPath)) {
  app.use(express.static(distPath));
  app.get("*", (req, res, next) => {
    if (req.path.startsWith("/api")) return next();
    res.sendFile(path.join(distPath, "index.html"));
  });
}

app.listen(PORT, "0.0.0.0", () => {
  console.log(`Linux File Explorer backend running at http://localhost:${PORT}`);
});
