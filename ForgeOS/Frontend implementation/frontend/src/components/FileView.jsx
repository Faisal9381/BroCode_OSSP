import React from "react";
import { Folder, File, FileText, FileCode, Binary } from "lucide-react";

export default function FileView({
  items,
  loading,
  searchQuery,
  selectedItem,
  onItemClick,
  onItemDoubleClick,
  viewMode,
}) {
  const formatSize = (bytes) => {
    if (bytes === 0) return "0 B";
    if (bytes < 1024) return `${bytes} B`;
    if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`;
    return `${(bytes / (1024 * 1024)).toFixed(1)} MB`;
  };

  const getFileIcon = (item, isLarge = false) => {
    const size = isLarge ? 36 : 18;
    if (item.isDirectory) {
      return <Folder size={size} className="icon-folder" />;
    }
    const ext = item.name.split(".").pop().toLowerCase();
    if (["c", "h", "cpp", "js", "html", "css", "json", "py", "sh"].includes(ext) || item.name === "Makefile") {
      return <FileCode size={size} className="icon-code" />;
    }
    if (["txt", "md", "log", "conf"].includes(ext)) {
      return <FileText size={size} className="icon-file" />;
    }
    if (item.permissions && (item.permissions.includes("7") || item.permissions.includes("5"))) {
      return <Binary size={size} className="icon-bin" />;
    }
    return <File size={size} className="icon-file" />;
  };

  if (loading) {
    return (
      <div className="empty-state">
        <div className="spinner" style={{ width: 28, height: 28 }} />
        <span>Reading directory contents with C dir_ops...</span>
      </div>
    );
  }

  if (items.length === 0) {
    return (
      <div className="empty-state">
        <Folder size={48} color="#ccc" />
        <span>
          {searchQuery ? "No files or folders match your search." : "This folder is empty."}
        </span>
      </div>
    );
  }

  if (viewMode === "grid") {
    return (
      <div className="file-grid">
        {items.map((item) => {
          const isSelected = selectedItem?.path === item.path;
          return (
            <div
              key={item.path}
              className={`grid-card ${isSelected ? "selected" : ""}`}
              onClick={() => onItemClick(item)}
              onDoubleClick={() => onItemDoubleClick(item)}
              title={item.name}
            >
              <div className="grid-icon-wrap">{getFileIcon(item, true)}</div>
              <div className="grid-name">{item.name}</div>
              <div className="grid-meta">
                {item.isDirectory ? "Folder" : formatSize(item.size)}
              </div>
            </div>
          );
        })}
      </div>
    );
  }

  return (
    <table className="file-table">
      <thead>
        <tr>
          <th>Name</th>
          <th style={{ width: 140 }}>Type</th>
          <th style={{ width: 120 }}>Size</th>
          <th style={{ width: 110 }}>Permissions</th>
        </tr>
      </thead>
      <tbody>
        {items.map((item) => {
          const isSelected = selectedItem?.path === item.path;
          return (
            <tr
              key={item.path}
              className={`file-row ${isSelected ? "selected" : ""}`}
              onClick={() => onItemClick(item)}
              onDoubleClick={() => onItemDoubleClick(item)}
            >
              <td className="file-name-cell">
                {getFileIcon(item)}
                <span>{item.name}</span>
              </td>
              <td style={{ color: "#666" }}>{item.isDirectory ? "Folder" : "File"}</td>
              <td style={{ color: "#666" }}>
                {item.isDirectory ? "--" : formatSize(item.size)}
              </td>
              <td style={{ fontFamily: "monospace", color: "#666" }}>
                {item.permissions || "644"}
              </td>
            </tr>
          );
        })}
      </tbody>
    </table>
  );
}
