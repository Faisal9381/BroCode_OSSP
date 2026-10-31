import React, { useState, useEffect } from "react";
import { Plus, FolderPlus, Edit3, Trash2, FileText, Info, X, Copy, Check, Save, FileEdit, AlertCircle } from "lucide-react";

export function CreateFileModal({ isOpen, onClose, currentPath, onSubmit }) {
  const [name, setName] = useState("");

  if (!isOpen) return null;

  const handleSubmit = (e) => {
    e.preventDefault();
    if (name.trim()) {
      onSubmit(name.trim());
      setName("");
    }
  };

  return (
    <div className="modal-overlay" onClick={onClose}>
      <div className="modal-content" onClick={(e) => e.stopPropagation()}>
        <div className="modal-header">
          <div className="modal-title">
            <Plus size={18} color="#0078d4" />
            <span>Create New File</span>
          </div>
          <button className="modal-close" onClick={onClose}><X size={18} /></button>
        </div>
        <form onSubmit={handleSubmit}>
          <div className="modal-body">
            <p style={{ fontSize: 13, color: "#666" }}>
              Directory: <code>{currentPath}</code>
            </p>
            <label className="input-label">File Name</label>
            <input
              type="text"
              autoFocus
              className="text-input"
              placeholder="e.g. notes.txt, main.c"
              value={name}
              onChange={(e) => setName(e.target.value)}
            />
          </div>
          <div className="modal-footer">
            <button type="button" className="toolbar-btn" onClick={onClose}>Cancel</button>
            <button type="submit" className="toolbar-btn primary" disabled={!name.trim()}>Create File</button>
          </div>
        </form>
      </div>
    </div>
  );
}

export function CreateFolderModal({ isOpen, onClose, currentPath, onSubmit }) {
  const [name, setName] = useState("");

  if (!isOpen) return null;

  const handleSubmit = (e) => {
    e.preventDefault();
    if (name.trim()) {
      onSubmit(name.trim());
      setName("");
    }
  };

  return (
    <div className="modal-overlay" onClick={onClose}>
      <div className="modal-content" onClick={(e) => e.stopPropagation()}>
        <div className="modal-header">
          <div className="modal-title">
            <FolderPlus size={18} color="#ffb900" />
            <span>Create New Folder</span>
          </div>
          <button className="modal-close" onClick={onClose}><X size={18} /></button>
        </div>
        <form onSubmit={handleSubmit}>
          <div className="modal-body">
            <p style={{ fontSize: 13, color: "#666" }}>
              Directory: <code>{currentPath}</code>
            </p>
            <label className="input-label">Folder Name</label>
            <input
              type="text"
              autoFocus
              className="text-input"
              placeholder="e.g. docs, projects"
              value={name}
              onChange={(e) => setName(e.target.value)}
            />
          </div>
          <div className="modal-footer">
            <button type="button" className="toolbar-btn" onClick={onClose}>Cancel</button>
            <button type="submit" className="toolbar-btn primary" disabled={!name.trim()}>Create Folder</button>
          </div>
        </form>
      </div>
    </div>
  );
}

export function RenameModal({ isOpen, onClose, selectedItem, onSubmit }) {
  const [newName, setNewName] = useState(selectedItem?.name || "");

  if (!isOpen || !selectedItem) return null;

  const handleSubmit = (e) => {
    e.preventDefault();
    if (newName.trim() && newName.trim() !== selectedItem.name) {
      onSubmit(newName.trim());
    } else {
      onClose();
    }
  };

  return (
    <div className="modal-overlay" onClick={onClose}>
      <div className="modal-content" onClick={(e) => e.stopPropagation()}>
        <div className="modal-header">
          <div className="modal-title">
            <Edit3 size={18} color="#0078d4" />
            <span>Rename Item</span>
          </div>
          <button className="modal-close" onClick={onClose}><X size={18} /></button>
        </div>
        <form onSubmit={handleSubmit}>
          <div className="modal-body">
            <p style={{ fontSize: 13, color: "#666" }}>
              Current: <code>{selectedItem.name}</code>
            </p>
            <label className="input-label">New Name</label>
            <input
              type="text"
              autoFocus
              className="text-input"
              value={newName}
              onChange={(e) => setNewName(e.target.value)}
            />
          </div>
          <div className="modal-footer">
            <button type="button" className="toolbar-btn" onClick={onClose}>Cancel</button>
            <button type="submit" className="toolbar-btn primary" disabled={!newName.trim()}>Rename</button>
          </div>
        </form>
      </div>
    </div>
  );
}

export function DeleteModal({ isOpen, onClose, selectedItem, onConfirm }) {
  if (!isOpen || !selectedItem) return null;

  return (
    <div className="modal-overlay" onClick={onClose}>
      <div className="modal-content" onClick={(e) => e.stopPropagation()}>
        <div className="modal-header">
          <div className="modal-title">
            <Trash2 size={18} color="#c42b1c" />
            <span>Delete {selectedItem.isDirectory ? "Folder" : "File"}</span>
          </div>
          <button className="modal-close" onClick={onClose}><X size={18} /></button>
        </div>
        <div className="modal-body">
          <p style={{ fontSize: 14 }}>Are you sure you want to permanently delete:</p>
          <div
            style={{
              padding: "10px 14px",
              background: "#fff1f0",
              border: "1px solid #ffccc7",
              borderRadius: 4,
              fontWeight: 600,
              color: "#cf1322",
              wordBreak: "break-all",
            }}
          >
            {selectedItem.path}
          </div>
          {selectedItem.isDirectory && (
            <p style={{ fontSize: 12, color: "#888" }}>
              Note: Standard C <code>rmdir</code> requires directories to be empty.
            </p>
          )}
        </div>
        <div className="modal-footer">
          <button type="button" className="toolbar-btn" onClick={onClose}>Cancel</button>
          <button
            type="button"
            className="toolbar-btn danger"
            style={{ background: "#c42b1c", color: "#fff" }}
            onClick={onConfirm}
          >
            Confirm Delete
          </button>
        </div>
      </div>
    </div>
  );
}

export function ReadFileModal({ isOpen, onClose, selectedItem, content }) {
  const [copied, setCopied] = useState(false);

  if (!isOpen || !selectedItem) return null;

  const handleCopy = () => {
    navigator.clipboard.writeText(content);
    setCopied(true);
    setTimeout(() => setCopied(false), 2000);
  };

  return (
    <div className="modal-overlay" onClick={onClose}>
      <div className="modal-content large" onClick={(e) => e.stopPropagation()}>
        <div className="modal-header">
          <div className="modal-title">
            <FileText size={18} color="#0078d4" />
            <span>{selectedItem.name}</span>
          </div>
          <button className="modal-close" onClick={onClose}><X size={18} /></button>
        </div>
        <div className="modal-body" style={{ flex: 1 }}>
          <div
            style={{
              display: "flex",
              justifyContent: "space-between",
              alignItems: "center",
              fontSize: 12,
              color: "#666",
            }}
          >
            <span>Path: <code>{selectedItem.path}</code></span>
            <button className="toolbar-btn" style={{ height: 26, padding: "2px 8px" }} onClick={handleCopy}>
              {copied ? <Check size={14} color="green" /> : <Copy size={14} />}
              <span>{copied ? "Copied!" : "Copy"}</span>
            </button>
          </div>
          <pre className="file-viewer-pre">{content || "(File is empty)"}</pre>
        </div>
        <div className="modal-footer">
          <span style={{ marginRight: "auto", fontSize: 11, color: "#777", fontStyle: "italic" }}>
            Invoked C <code>read_file()</code> from <code>file_ops.c</code>
          </span>
          <button type="button" className="toolbar-btn primary" onClick={onClose}>Close</button>
        </div>
      </div>
    </div>
  );
}

export function EditFileModal({ isOpen, onClose, selectedItem, initialContent, onSave }) {
  const [content, setContent] = useState("");
  const [savedBaseline, setSavedBaseline] = useState("");
  const [saving, setSaving] = useState(false);
  const [showDiscardConfirm, setShowDiscardConfirm] = useState(false);

  useEffect(() => {
    if (isOpen) {
      setContent(initialContent || "");
      setSavedBaseline(initialContent || "");
      setShowDiscardConfirm(false);
      setSaving(false);
    }
  }, [isOpen, initialContent]);

  if (!isOpen || !selectedItem) return null;

  const isDirty = content !== savedBaseline;

  const handleCloseAttempt = () => {
    if (isDirty) {
      setShowDiscardConfirm(true);
    } else {
      onClose();
    }
  };

  const handleConfirmDiscard = () => {
    setShowDiscardConfirm(false);
    onClose();
  };

  const handleSaveClick = async () => {
    setSaving(true);
    try {
      const success = await onSave(content);
      if (success) {
        setSavedBaseline(content);
      }
    } finally {
      setSaving(false);
    }
  };

  return (
    <div className="modal-overlay" onClick={handleCloseAttempt}>
      <div className="modal-content large" onClick={(e) => e.stopPropagation()}>
        <div className="modal-header">
          <div className="modal-title">
            <FileEdit size={18} color="#0078d4" />
            <span>Editing: {selectedItem.name}</span>
            {isDirty ? (
              <span className="badge-unsaved">? Unsaved Changes</span>
            ) : (
              <span className="badge-saved">? Saved</span>
            )}
          </div>
          <button className="modal-close" onClick={handleCloseAttempt}>
            <X size={18} />
          </button>
        </div>

        {showDiscardConfirm && (
          <div className="alert-box alert-danger" style={{ margin: "10px 18px 0 18px" }}>
            <AlertCircle size={16} />
            <span style={{ flex: 1 }}>
              You have unsaved changes. Do you want to discard them?
            </span>
            <button
              className="toolbar-btn danger"
              style={{ height: 26, padding: "2px 8px", background: "#c42b1c", color: "#fff" }}
              onClick={handleConfirmDiscard}
            >
              Discard & Close
            </button>
            <button
              className="toolbar-btn"
              style={{ height: 26, padding: "2px 8px" }}
              onClick={() => setShowDiscardConfirm(false)}
            >
              Keep Editing
            </button>
          </div>
        )}

        <div className="modal-body" style={{ flex: 1 }}>
          <div style={{ fontSize: 12, color: "#666" }}>
            File Location: <code>{selectedItem.path}</code>
          </div>
          <textarea
            autoFocus
            className="file-editor-textarea"
            value={content}
            onChange={(e) => setContent(e.target.value)}
            placeholder="Type file contents here..."
            spellCheck={false}
          />
        </div>

        <div className="modal-footer">
          <span style={{ marginRight: "auto", fontSize: 11, color: "#777", fontStyle: "italic" }}>
            Updates WSL Linux file via C adapter
          </span>
          <button
            type="button"
            className="toolbar-btn"
            onClick={handleCloseAttempt}
            disabled={saving}
          >
            Cancel
          </button>
          <button
            type="button"
            className="toolbar-btn primary"
            onClick={handleSaveClick}
            disabled={saving || !isDirty}
          >
            <Save size={15} />
            <span>{saving ? "Saving..." : "Save"}</span>
          </button>
        </div>
      </div>
    </div>
  );
}

export function InfoModal({ isOpen, onClose, infoOutput }) {
  if (!isOpen) return null;

  return (
    <div className="modal-overlay" onClick={onClose}>
      <div className="modal-content" onClick={(e) => e.stopPropagation()}>
        <div className="modal-header">
          <div className="modal-title">
            <Info size={18} color="#0078d4" />
            <span>Properties</span>
          </div>
          <button className="modal-close" onClick={onClose}><X size={18} /></button>
        </div>
        <div className="modal-body">
          <pre
            style={{
              background: "#f4f6f8",
              padding: 12,
              borderRadius: 4,
              fontSize: 13,
              fontFamily: "monospace",
              lineHeight: 1.6,
            }}
          >
            {infoOutput || "Loading info..."}
          </pre>
        </div>
        <div className="modal-footer">
          <span style={{ marginRight: "auto", fontSize: 11, color: "#777", fontStyle: "italic" }}>
            Invoked C <code>show_file_info()</code> from <code>file_ops.c</code>
          </span>
          <button type="button" className="toolbar-btn primary" onClick={onClose}>Close</button>
        </div>
      </div>
    </div>
  );
}
