# SimpleFileHost

**SimpleFileHost** is a lightweight local file-sharing tool written in C++.
It lets you send or receive files over Wi-Fi using only a web browser — no configuration, no external servers.

The program starts a tiny built-in HTTP server, generates a random token URL, and optionally displays an ASCII QR code (via **libqrencode**) for easy scanning.

---

## 🚀 Features

- 📦 Send or receive files and folders locally over Wi-Fi (`send <file/path to file>` or `get <output file>`)
- 🔑 Random one-time token URL for each transfer
- 🧾 ASCII QR code display (optional)
- 🧩 Archive and split large files into parts (`zip <target> <split_size>`)
- 🖥️ Built-in minimal POSIX HTTP server **no external libraries**
- 🧹 Auto-shutdown the server after transfer completion

---
## ⬇️ Installation, 🛠️ Building or ❌Removing

### ⬇️ Installation

Requirements:
- POSIX system (Linux or macOS)
- libarchive (required for archive operations)
- Optional: libqrencode (for QR code display)


This one-liner will help you to install from pre-built binary:

```bash
mkdir temp; cd temp; wget https://github.com/Kolya080808/SimpleFileHost/releases/download/v2.0/release.zip; unzip release.zip; chmod +x install.sh; sudo ./install.sh; cd ../; rm -r temp
```

The install script will ask if you want to install optional QR code support.

### 🛠️ Build from Source

Requirements:
- **C++17**
- **CMake ≥ 3.10**
- POSIX system (Linux or macOS)
- libarchive (required)
- Optional: libqrencode (for QR code display)

Build with interactive dependency selection:

```bash
git clone https://github.com/Kolya080808/SimpleFileHost.git
cd SimpleFileHost
chmod +x build.sh
./build.sh
```

The build script will ask if you want to build with QR code support.

### ❌ Removing

Uninstall the binary and optionally remove installed dependencies:

```bash
wget https://github.com/Kolya080808/SimpleFileHost/raw/main/remove.sh
chmod +x remove.sh
./remove.sh
rm remove.sh
```

---

## ▶️ Usage

Run the program:

```bash
simplefilehost
```

You'll enter an interactive CLI with commands:

### 📤 Commands:

```bash
help
```

The server prints out this help message:
```
Available commands:
  send <file>              — Send file over Wi-Fi.
  senddir <dir>            — Send entire folder (auto zipped).
  get <output_file>        — Receive file from another device.
  zip <target> [size]      — Archive.
  help                     — Show this help message.
  exit                     — Quit program.
```

```bash
send <path_to_file>
```

The server will print a URL (and QR code if enabled).
Open it on another device in the same Wi-Fi/LAN network to download the file.

```bash
get <output_filename>
```

The server will print a URL (and QR code if enabled).
Open it on another device and drag-and-drop a file to send it.

```bash
zip <target>
```

Archives the specified file or directory.

**Examples:**

```bash
zip hello
```

→ Creates `hello.zip`.

```bash
senddir <directory_path>
```

The directory will be archived and automatically shared over HTTP.

```bash
exit
```
Exits program.

---

## 🔒 Security Notes

- The server can bind to **all interfaces (0.0.0.0)**. Do **not** use it on untrusted networks.
- Each session has a **random 24-character token** to make guessing the URL unlikely.
- Transfers are plain **HTTP (no TLS)** — avoid sending confidential files.
- The server automatically shuts down after a completed upload/download.

---

## ⚠️ Limitations

- Without `--max-size` flag, server automatically limits file size is 200MB
- Minimal HTTP implementation (no keep-alive)
- Large or interrupted uploads may fail (I've tried to fix it, but I am not sure that it works well. At least 60gb worked well).

---
## P.S.

I tried to make a TLS, but I do not have enough knowledge to do that. In case you want and can help, or if you have suggestions for improvements, you can contact me via [telegram](https://kolya080808.t.me/).

---

## 📚 License

MIT License © 2025 — [Kolya080808](https://github.com/Kolya080808)
