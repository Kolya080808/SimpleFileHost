# SimpleFileHost

**SimpleFileHost** is a lightweight local file-sharing tool written in C++.
It lets you send or receive files over Wi-Fi using only a web browser — no configuration, no external servers.

The program starts a tiny built-in HTTP server, generates a random token URL, and optionally displays an ASCII QR code (via **libqrencode**) for easy scanning.

---

## 🚀 Features

- 📦 Send or receive files and folders locally over Wi-Fi (`send <file/path to file>` or `get <output file>`)
- 🔑 Random one-time token URL for each transfer
- 🧾 ASCII QR code display
- 🧩 Archive and split large files into parts (`zip <target> <split_size>`)
- 🖥️ Built-in minimal POSIX HTTP server — **no external libraries**
- 🧹 Auto-shutdown after transfer completion

---
## ⬇️ Installation, 🛠️ Building or ❌Removing

### ⬇️ Installation

Requirements:
- POSIX system (Linux or macOS)
- libqrencode (for QR code display)

This one-liner will help you to install.

```bash
mkdir temp; cd temp; wget https://github.com/Kolya080808/SimpleFileHost/releases/download/v1.0/release.zip; unzip release.zip; chmod +x install.sh; sudo ./install.sh; cd ../; rm -r temp
```

![install](https://github.com/user-attachments/assets/045042eb-f730-4ec3-9698-a6c697e09a4f)

### 🛠️ Build

Requirements:
- **C++17**
- **CMake ≥ 3.10**
- POSIX system (Linux or macOS)
- Optional: `libqrencode` (for QR code display) (it can be built without it, but it will be more easy for phone-to-pc transfer 😉)

If you want to install it with libqrencode, to build, you will need this command: 

```bash
git clone https://github.com/Kolya080808/SimpleFileHost.git; cd SimpleFileHost; chmod +x build.sh; ./build.sh
````

Else, you should do like this:
```bash
git clone https://github.com/Kolya080808/SimpleFileHost.git; cd SimpleFileHost; chmod +x build.sh; sed -i 's/\s*libqrencode[-a-z]*//g; s/\s*qrencode//g' build.sh; ./build.sh
```

After doing one of theese two commands, you can delete the folder.

![build](https://github.com/user-attachments/assets/487b20a6-e88e-4d75-a5cc-61056320acd1)


### ❌Removing

Nothing is required.

This one-liner should help you to uninstall the binary and optionally remove installed dependencies.

```bash
wget https://github.com/Kolya080808/SimpleFileHost/raw/main/remove.sh; chmod +x remove.sh; ./remove.sh; rm -r remove.sh
```

![removing](https://github.com/user-attachments/assets/8e589bd4-da10-44a3-9816-a0287d7f37ff)


---

## ▶️ Usage

Run the program:

```bash
simplefilehost
```

You’ll enter an interactive CLI with commands:

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
  zip <target> [size]      — Archive and optionally split.
                             Example: zip hello 500mb
  help                     — Show this help message.
  exit                     — Quit program.

Notes:
  - Supported split units: kb, mb, gb.
  - Requires 'zip' and 'split' commands installed.
```

```bash
send <path_to_file>
```

The server will print a URL (and QR code).
Open it on another device in the same Wi-Fi network to download the file.


```bash
get <output_filename>
```

The server will print a URL (and QR code).
Open it on another device and drag-and-drop a file to send it.

---

```bash
zip <target> <split_size>
```

Archives the specified file or directory and (optionally) splits it into smaller chunks.

**Examples:**

```bash
zip hello 500mb
```

→ Creates `hello.zip` and splits it into **500 MB** chunks:

```
hello.zip.part1
hello.zip.part2
...
```

```bash
zip dataset 1gb
```

→ Creates `dataset.zip` and splits it into **1 GB** parts.

```bash
zip report 100kb
```

→ Creates `report.zip` and splits it into **100 KB** parts.

Accepted units:

* `kb` — kilobytes
* `mb` — megabytes
* `gb` — gigabytes

If no size is given, only `target.zip` is created (no splitting).

```bash
senddir <directory_path>
```

Example:

```bash
senddir project_folder
```

The directory will be archived (`zip -r project_folder.zip project_folder`) and automatically shared over HTTP.

```bash
exit
```
Exits program.

### ⚠️ Example Usage

![usage](https://github.com/user-attachments/assets/6943ae19-017f-4484-82a8-47e31fd0cb79)

As I use WSL, I need to do some more commands in PowerShell. Don't do them, if you don't use WSL.

---

## 🔒 Security Notes

* The server binds to **all interfaces (0.0.0.0)**. Do **not** use it on untrusted networks.
* Each session has a **random 24-character token** to make guessing the URL unlikely.
* Transfers are plain **HTTP (no TLS)** — avoid sending confidential files.
* The server automatically shuts down after a completed upload/download.

---

## ⚠️ Limitations

* Minimal HTTP implementation (no chunked transfer, no keep-alive).
* Naive multipart parser — works for small uploads only.
* Requires `zip` and `split` system binaries for compression/splitting.
* Large or interrupted uploads may fail.
* For production use, replace the server with:

  * [cpp-httplib](https://github.com/yhirose/cpp-httplib)
  * [civetweb](https://github.com/civetweb/civetweb)
  * [mongoose](https://github.com/cesanta/mongoose)

  Or something better, then I've used.

---
## P.S.

If you have something, that can be helpful in this programm, you can suggest it in my [telegram](https://kolya080808.t.me/). I will be glad to add something new to it.

---

## 📚 License

MIT License © 2025 — [Kolya080808]

---
