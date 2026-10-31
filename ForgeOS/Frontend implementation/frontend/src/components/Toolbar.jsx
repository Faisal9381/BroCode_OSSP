import React from "react";
import { Plus, FolderPlus, Edit3, Trash2, Eye, Info, List, LayoutGrid, FileEdit } from "lucide-react";

export default function Toolbar({
  onNewFile,
  onNewFolder,
  selectedItem,
  onRename,
  onDelete,
  onReadFile,
  onEditFile,
  onInfo,
  viewMode,
  setViewMode,
}) {
  return (
    <div className="action-toolbar">
      <div className="toolbar-group">
        <button className="toolbar-btn primary" onClick={onNewFile}>
          <Plus size={15} />
          <span>Create File</span>
        </button>
        <button className="toolbar-btn" onClick={onNewFolder}>
          <FolderPlus size={15} color="#ffb900" />
          <span>Create Folder</span>
        </button>

        <div className="toolbar-divider" />

        <button
          className="toolbar-btn"
          disabled={!selectedItem || selectedItem.isDirectory}
          onClick={onEditFile}
        >
          <FileEdit size={15} color="#0078d4" />
          <span>Edit File</span>
        </button>

        <button
          className="toolbar-btn"
          disabled={!selectedItem || selectedItem.isDirectory}
          onClick={onReadFile}
        >
          <Eye size={15} />
          <span>Open/Read File</span>
        </button>

        <button
          className="toolbar-btn"
          disabled={!selectedItem}
          onClick={onRename}
        >
          <Edit3 size={15} />
          <span>Rename</span>
        </button>

        <button
          className="toolbar-btn danger"
          disabled={!selectedItem}
          onClick={onDelete}
        >
          <Trash2 size={15} />
          <span>Delete</span>
        </button>

        <button
          className="toolbar-btn"
          disabled={!selectedItem}
          onClick={onInfo}
        >
          <Info size={15} />
          <span>Properties</span>
        </button>
      </div>

      <div className="toolbar-group">
        <button
          className={`nav-btn ${viewMode === "list" ? "active" : ""}`}
          title="List View"
          onClick={() => setViewMode("list")}
        >
          <List size={16} />
        </button>
        <button
          className={`nav-btn ${viewMode === "grid" ? "active" : ""}`}
          title="Grid View"
          onClick={() => setViewMode("grid")}
        >
          <LayoutGrid size={16} />
        </button>
      </div>
    </div>
  );
}
