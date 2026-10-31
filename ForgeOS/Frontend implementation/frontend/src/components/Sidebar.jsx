import React from "react";
import { Terminal, Home, HardDrive, Folder } from "lucide-react";

export default function Sidebar({ currentPath, quickFolders, onNavigate }) {
  const isWorkspace = currentPath === "/home/faisal/linux_file_system_explorer";
  const isHome = currentPath === "/home/faisal";
  const isRoot = currentPath === "/";

  return (
    <div className="sidebar">
      <div className="sidebar-section">
        <div className="sidebar-title">Quick Access</div>
        <div
          className={`sidebar-item ${isWorkspace ? "active" : ""}`}
          onClick={() => onNavigate("/home/faisal/linux_file_system_explorer")}
        >
          <Terminal className="sidebar-icon" />
          <span>Project Workspace</span>
        </div>
        <div
          className={`sidebar-item ${isHome ? "active" : ""}`}
          onClick={() => onNavigate("/home/faisal")}
        >
          <Home className="sidebar-icon" />
          <span>Home (/home/faisal)</span>
        </div>
        <div
          className={`sidebar-item ${isRoot ? "active" : ""}`}
          onClick={() => onNavigate("/")}
        >
          <HardDrive className="sidebar-icon" />
          <span>Root Filesystem (/)</span>
        </div>
      </div>

      <div className="sidebar-section">
        <div className="sidebar-title">Subfolders</div>
        {quickFolders.length === 0 ? (
          <div style={{ padding: "6px 10px", fontSize: 12, color: "#888" }}>
            No subfolders
          </div>
        ) : (
          quickFolders.map((sf) => (
            <div
              key={sf.path}
              className={`sidebar-item ${currentPath === sf.path ? "active" : ""}`}
              onClick={() => onNavigate(sf.path)}
              title={sf.path}
            >
              <Folder size={16} className="icon-folder" />
              <span>{sf.name}</span>
            </div>
          ))
        )}
      </div>
    </div>
  );
}
