import React, { useState, useEffect } from "react";
import TitleBar from "./components/TitleBar";
import AddressBar from "./components/AddressBar";
import Toolbar from "./components/Toolbar";
import Sidebar from "./components/Sidebar";
import FileView from "./components/FileView";
import {
  CreateFileModal,
  CreateFolderModal,
  RenameModal,
  DeleteModal,
  ReadFileModal,
  EditFileModal,
  InfoModal,
} from "./components/Modals";

export default function App() {
  const [currentPath, setCurrentPath] = useState("");
  const [pathInput, setPathInput] = useState("");
  const [history, setHistory] = useState([]);
  const [items, setItems] = useState([]);
  const [loading, setLoading] = useState(false);
  const [searchQuery, setSearchQuery] = useState("");
  const [selectedItem, setSelectedItem] = useState(null);
  const [viewMode, setViewMode] = useState("list");
  const [quickFolders, setQuickFolders] = useState([]);

  // Modals state
  const [showCreateFile, setShowCreateFile] = useState(false);
  const [showCreateFolder, setShowCreateFolder] = useState(false);
  const [showRename, setShowRename] = useState(false);
  const [showDelete, setShowDelete] = useState(false);
  const [showReadFile, setShowReadFile] = useState(false);
  const [showEditFile, setShowEditFile] = useState(false);
  const [showInfo, setShowInfo] = useState(false);

  // Modal contents
  const [fileContent, setFileContent] = useState("");
  const [editContent, setEditContent] = useState("");
  const [infoOutput, setInfoOutput] = useState("");
  const [notification, setNotification] = useState(null);

  const showNotification = (msg, type = "success") => {
    setNotification({ msg, type });
    setTimeout(() => setNotification(null), 4000);
  };

  useEffect(() => {
    fetch("/api/current-dir")
      .then((res) => res.json())
      .then((data) => {
        if (data.currentDir) {
          loadDirectory(data.currentDir, false);
        }
      })
      .catch((err) => {
        console.error(err);
        showNotification("Failed to connect to backend", "danger");
      });
  }, []);

  const loadDirectory = async (dirPath, addToHistory = true) => {
    if (!dirPath) return;
    setLoading(true);
    setSelectedItem(null);

    try {
      const res = await fetch(`/api/list?path=${encodeURIComponent(dirPath)}`);
      const data = await res.json();

      if (!res.ok) {
        throw new Error(data.details || data.error || "Failed to load directory");
      }

      if (addToHistory && currentPath && currentPath !== data.path) {
        setHistory((prev) => [...prev, currentPath]);
      }

      setCurrentPath(data.path);
      setPathInput(data.path);
      setItems(data.items || []);

      const subdirs = (data.items || []).filter((it) => it.isDirectory);
      setQuickFolders(subdirs.slice(0, 10));
    } catch (err) {
      showNotification(err.message, "danger");
    } finally {
      setLoading(false);
    }
  };

  const handleBack = () => {
    if (history.length === 0) return;
    const prev = history[history.length - 1];
    setHistory((prevHist) => prevHist.slice(0, -1));
    loadDirectory(prev, false);
  };

  const handleUp = () => {
    if (!currentPath || currentPath === "/") return;
    const parent = currentPath.substring(0, currentPath.lastIndexOf("/")) || "/";
    loadDirectory(parent);
  };

  const handleRefresh = () => {
    loadDirectory(currentPath, false);
  };

  const handlePathSubmit = (e) => {
    e.preventDefault();
    if (pathInput.trim()) {
      loadDirectory(pathInput.trim());
    }
  };

  const handleCreateFile = async (name) => {
    try {
      const res = await fetch("/api/create-file", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ dirPath: currentPath, name }),
      });
      const data = await res.json();
      if (!res.ok) throw new Error(data.details || data.error || "Failed to create file");

      showNotification(`File '${name}' created successfully!`);
      setShowCreateFile(false);
      handleRefresh();
    } catch (err) {
      showNotification(err.message, "danger");
    }
  };

  const handleCreateFolder = async (name) => {
    try {
      const res = await fetch("/api/create-dir", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ dirPath: currentPath, name }),
      });
      const data = await res.json();
      if (!res.ok) throw new Error(data.details || data.error || "Failed to create directory");

      showNotification(`Folder '${name}' created successfully!`);
      setShowCreateFolder(false);
      handleRefresh();
    } catch (err) {
      showNotification(err.message, "danger");
    }
  };

  const handleRename = async (newName) => {
    if (!selectedItem) return;
    const newPath = currentPath === "/" ? `/${newName}` : `${currentPath}/${newName}`;

    try {
      const res = await fetch("/api/rename", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ oldPath: selectedItem.path, newPath }),
      });
      const data = await res.json();
      if (!res.ok) throw new Error(data.details || data.error || "Failed to rename");

      showNotification(`Renamed to '${newName}' successfully!`);
      setShowRename(false);
      handleRefresh();
    } catch (err) {
      showNotification(err.message, "danger");
    }
  };

  const handleDelete = async () => {
    if (!selectedItem) return;

    try {
      const res = await fetch("/api/delete", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          targetPath: selectedItem.path,
          isDirectory: selectedItem.isDirectory,
        }),
      });
      const data = await res.json();
      if (!res.ok) throw new Error(data.details || data.error || "Failed to delete");

      showNotification(`'${selectedItem.name}' deleted successfully!`);
      setShowDelete(false);
      setSelectedItem(null);
      handleRefresh();
    } catch (err) {
      showNotification(err.message, "danger");
    }
  };

  const handleReadFile = async (targetItem) => {
    const item = targetItem || selectedItem;
    if (!item || item.isDirectory) return;

    try {
      const res = await fetch(`/api/read?path=${encodeURIComponent(item.path)}`);
      const data = await res.json();
      if (!res.ok) throw new Error(data.details || data.error || "Failed to read file");

      setFileContent(data.content);
      setSelectedItem(item);
      setShowReadFile(true);
    } catch (err) {
      showNotification(err.message, "danger");
    }
  };

  const handleOpenEditFile = async (targetItem) => {
    const item = targetItem || selectedItem;
    if (!item || item.isDirectory) return;

    try {
      const res = await fetch(`/api/read?path=${encodeURIComponent(item.path)}`);
      const data = await res.json();
      if (!res.ok) throw new Error(data.details || data.error || "Failed to load file for editing");

      setEditContent(data.content || "");
      setSelectedItem(item);
      setShowEditFile(true);
    } catch (err) {
      showNotification(err.message, "danger");
    }
  };

  const handleSaveFile = async (newContent) => {
    if (!selectedItem) return false;

    try {
      const res = await fetch("/api/save-file", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          filePath: selectedItem.path,
          content: newContent,
        }),
      });
      const data = await res.json();
      if (!res.ok) throw new Error(data.details || data.error || "Failed to save file");

      showNotification(`File '${selectedItem.name}' saved successfully!`);
      handleRefresh();
      return true;
    } catch (err) {
      showNotification(err.message, "danger");
      return false;
    }
  };

  const handleShowInfo = async (targetItem) => {
    const item = targetItem || selectedItem;
    if (!item) return;

    try {
      const res = await fetch(`/api/info?path=${encodeURIComponent(item.path)}`);
      const data = await res.json();
      if (!res.ok) throw new Error(data.details || data.error || "Failed to get properties");

      setInfoOutput(data.info);
      setSelectedItem(item);
      setShowInfo(true);
    } catch (err) {
      showNotification(err.message, "danger");
    }
  };

  const filteredItems = items.filter((it) =>
    it.name.toLowerCase().includes(searchQuery.toLowerCase())
  );

  return (
    <div className="app-container">
      <TitleBar />

      <AddressBar
        history={history}
        onBack={handleBack}
        currentPath={currentPath}
        onUp={handleUp}
        onRefresh={handleRefresh}
        loading={loading}
        pathInput={pathInput}
        setPathInput={setPathInput}
        onPathSubmit={handlePathSubmit}
        searchQuery={searchQuery}
        setSearchQuery={setSearchQuery}
      />

      <Toolbar
        onNewFile={() => setShowCreateFile(true)}
        onNewFolder={() => setShowCreateFolder(true)}
        selectedItem={selectedItem}
        onRename={() => setShowRename(true)}
        onDelete={() => setShowDelete(true)}
        onReadFile={() => handleReadFile(selectedItem)}
        onEditFile={() => handleOpenEditFile(selectedItem)}
        onInfo={() => handleShowInfo(selectedItem)}
        viewMode={viewMode}
        setViewMode={setViewMode}
      />

      {notification && (
        <div
          className={`alert-box ${
            notification.type === "danger" ? "alert-danger" : "alert-success"
          }`}
          style={{ margin: "6px 16px" }}
        >
          {notification.msg}
        </div>
      )}

      <div className="main-layout">
        <Sidebar
          currentPath={currentPath}
          quickFolders={quickFolders}
          onNavigate={(p) => loadDirectory(p)}
        />

        <div className="content-area">
          <div className="file-view-container">
            <FileView
              items={filteredItems}
              loading={loading}
              searchQuery={searchQuery}
              selectedItem={selectedItem}
              onItemClick={(it) => setSelectedItem(it)}
              onItemDoubleClick={(it) => {
                if (it.isDirectory) {
                  loadDirectory(it.path);
                } else {
                  handleReadFile(it);
                }
              }}
              viewMode={viewMode}
            />
          </div>

          <div className="statusbar">
            <div className="statusbar-left">
              <span>{filteredItems.length} items</span>
              {selectedItem && (
                <span>
                  Selected: <strong>{selectedItem.name}</strong>
                </span>
              )}
            </div>
            <div className="statusbar-right">
              <div className="status-pill">
                <span className="status-dot" />
                <span>WSL Ubuntu Connected</span>
              </div>
            </div>
          </div>
        </div>
      </div>

      <CreateFileModal
        isOpen={showCreateFile}
        onClose={() => setShowCreateFile(false)}
        currentPath={currentPath}
        onSubmit={handleCreateFile}
      />

      <CreateFolderModal
        isOpen={showCreateFolder}
        onClose={() => setShowCreateFolder(false)}
        currentPath={currentPath}
        onSubmit={handleCreateFolder}
      />

      <RenameModal
        isOpen={showRename}
        onClose={() => setShowRename(false)}
        selectedItem={selectedItem}
        onSubmit={handleRename}
      />

      <DeleteModal
        isOpen={showDelete}
        onClose={() => setShowDelete(false)}
        selectedItem={selectedItem}
        onConfirm={handleDelete}
      />

      <ReadFileModal
        isOpen={showReadFile}
        onClose={() => setShowReadFile(false)}
        selectedItem={selectedItem}
        content={fileContent}
      />

      <EditFileModal
        isOpen={showEditFile}
        onClose={() => setShowEditFile(false)}
        selectedItem={selectedItem}
        initialContent={editContent}
        onSave={handleSaveFile}
      />

      <InfoModal
        isOpen={showInfo}
        onClose={() => setShowInfo(false)}
        infoOutput={infoOutput}
      />
    </div>
  );
}
