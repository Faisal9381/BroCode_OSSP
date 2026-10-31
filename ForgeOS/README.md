# ForgeOS — Linux File System Explorer & OS Subsystems

**Course:** Operating Systems and Systems Programming (OSSP)  
**Modules:** Linux File System Explorer · Nanokernel · Shellforge  
**Tech Stack:** C (GCC, POSIX API) · Node.js + Express · React + Vite · REST API  
**Environment:** Linux / Ubuntu (WSL)

---

## Overview

**ForgeOS** is a systems programming project developed for the Operating Systems and Systems Programming curriculum. It bridges low-level POSIX/Linux kernel system calls with modern web technologies, providing an interactive GUI and CLI interface for exploring and manipulating the Linux file system.

```
┌─────────────────────────────────────────────────────────────┐
│                    Frontend (React + Vite)                  │
│   Address Bar · File Grid/List · Modals · File Editor (3000)│
└──────────────────────────────┬──────────────────────────────┘
                               │ HTTP / REST API
┌──────────────────────────────▼──────────────────────────────┐
│                  Backend (Node.js + Express)                │
│            Route Handlers · JSON API Bridge (5000)          │
└──────────────────────────────┬──────────────────────────────┘
                               │ execFile() (IPC)
┌──────────────────────────────▼──────────────────────────────┐
│                    Adapter & Core Engine (C)                │
│    adapter.c · file_ops.c · dir_ops.c · POSIX Syscalls      │
└──────────────────────────────┬──────────────────────────────┘
                               │ open(), read(), write(), opendir(), stat()
┌──────────────────────────────▼──────────────────────────────┐
│                     Linux Kernel File System                │
└─────────────────────────────────────────────────────────────┘
```

---

## Directory Structure

```
ForgeOS/
├── Frontend implementation/                      # Full-stack Linux File System Explorer
│   ├── main.c, file_ops.c/.h, dir_ops.c/.h       # Core C file & directory operations
│   ├── adapter.c                                 # CLI adapter returning JSON for web API
│   ├── Makefile                                  # Build scripts for C binaries
│   ├── backend/                                  # Express.js REST API server
│   │   ├── server.js                             # API endpoints bridging to C adapter
│   │   └── package.json
│   └── frontend/                                 # Modern React + Vite application
│       ├── src/                                  # React UI components & styling
│       ├── index.html
│       └── vite.config.js
├── Linux_File_System_Explorer_Project_Report.docx# Comprehensive project report & design doc
├── Nanokernel/                                   # Kernel primitives and experiments
└── Shellforge/                                   # Interactive shell implementation
```

---

## Features

- **POSIX System Calls**: Direct usage of `opendir()`, `readdir()`, `stat()`, `open()`, `read()`, `write()`, `mkdir()`, and `unlink()`.
- **JSON Adapter**: C program (`adapter.c`) formatted to return structured JSON outputs directly consumable by web services.
- **RESTful Backend**: Express backend offering endpoints for `/api/list`, `/api/read`, `/api/save-file`, `/api/create-file`, `/api/create-dir`, and `/api/delete`.
- **Modern Responsive Web UI**: React 19 interface with breadcrumbs, directory tree navigation, file metadata inspection, file editing, and creation dialogs.

---

## Quickstart Guide

### Prerequisites
- GCC / Clang & Make
- Node.js (v18+) & npm
- Linux or Windows Subsystem for Linux (WSL / Ubuntu)

### 1. Build C Binaries
Navigate to the frontend implementation folder and compile the C engine and adapter:
```bash
cd "ForgeOS/Frontend implementation"
make
gcc adapter.c dir_ops.c file_ops.c -o adapter
```

### 2. Start the Backend API
In a new terminal window:
```bash
cd "ForgeOS/Frontend implementation/backend"
npm install
npm run dev
```
*The backend server will start at `http://localhost:5000`.*

### 3. Start the Web Frontend
In another terminal window:
```bash
cd "ForgeOS/Frontend implementation/frontend"
npm install
npm run dev
```
*The web interface will start at `http://localhost:3000`.*

Open your browser and navigate to **`http://localhost:3000`** to interact with the file system.