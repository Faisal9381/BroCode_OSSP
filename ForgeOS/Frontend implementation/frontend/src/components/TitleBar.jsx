import React from "react";
import { Terminal } from "lucide-react";

export default function TitleBar() {
  return (
    <div className="titlebar">
      <div className="titlebar-left">
        <Terminal className="titlebar-icon" />
        <span className="titlebar-title">
          Linux File Explorer
          <span className="titlebar-badge">WSL Ubuntu 24.04</span>
        </span>
      </div>
      <div className="titlebar-right">
        <span style={{ fontSize: 12, color: "#666" }}>
          C Explorer Bridge &bull; GCC 13.3
        </span>
      </div>
    </div>
  );
}
