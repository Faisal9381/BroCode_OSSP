import React from "react";
import { ArrowLeft, ArrowUp, RotateCw, HardDrive, Search, X } from "lucide-react";

export default function AddressBar({
  history,
  onBack,
  currentPath,
  onUp,
  onRefresh,
  loading,
  pathInput,
  setPathInput,
  onPathSubmit,
  searchQuery,
  setSearchQuery,
}) {
  return (
    <div className="address-bar-row">
      <div className="nav-buttons">
        <button
          className="nav-btn"
          title="Back"
          disabled={history.length === 0}
          onClick={onBack}
        >
          <ArrowLeft size={16} />
        </button>
        <button
          className="nav-btn"
          title="Up to Parent Directory"
          disabled={!currentPath || currentPath === "/"}
          onClick={onUp}
        >
          <ArrowUp size={16} />
        </button>
        <button
          className="nav-btn"
          title="Refresh Directory"
          onClick={onRefresh}
        >
          <RotateCw size={15} className={loading ? "spinner" : ""} />
        </button>
      </div>

      <form className="address-bar" onSubmit={onPathSubmit}>
        <HardDrive size={16} className="address-icon" />
        <input
          type="text"
          className="address-input"
          value={pathInput}
          onChange={(e) => setPathInput(e.target.value)}
          placeholder="/home/faisal/linux_file_system_explorer"
        />
      </form>

      <div className="search-box">
        <Search size={15} color="#888" />
        <input
          type="text"
          className="search-input"
          placeholder="Search current folder..."
          value={searchQuery}
          onChange={(e) => setSearchQuery(e.target.value)}
        />
        {searchQuery && (
          <X
            size={14}
            color="#888"
            style={{ cursor: "pointer" }}
            onClick={() => setSearchQuery("")}
          />
        )}
      </div>
    </div>
  );
}
