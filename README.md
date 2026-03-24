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
- 🔒 Optional TLS (HTTPS) support via `--tls <cert> <key>`

---
## ⬇️ Installation, 🛠️ Building or ❌Removing

### ⬇️ Installation

#### From GitHub release

Requirements:
- POSIX system (Linux or macOS)
- libarchive (required for archive operations)
- Optional: libqrencode (for QR code display)
- OpenSSL dev libraries (libssl-dev)


This one-liner will help you to install from pre-built binary:

```bash
mkdir temp; cd temp; wget https://github.com/Kolya080808/SimpleFileHost/releases/download/v2.0/release.zip; unzip release.zip; chmod +x install.sh; sudo ./install.sh; cd ../; rm -r temp
```

The install script will ask if you want to install optional QR code support or no.

#### From Debian Mentors

Here everything installs by computer automatically. The only thing that you need is devscripts:

```bash
sudo apt-get install devscripts; mkdir tmp/; cd tmp/
```

Then, you need to install my pubkey from debian keyserver:

```bash
gpg --keyserver hkps://mentors.debian.net --recv-keys 466A7895D13BBA22B05EF3BCADD15E2243E24BA0
```

After that, import that as debian mentor's key:

```bash
gpg --export 466A7895D13BBA22B05EF3BCADD15E2243E24BA0 | sudo gpg --dearmor -o /usr/share/keyrings/debian-mantainers.gpg
```

Or, you if you don't want to, just skip this step and go further.

Then, you can download and install it:

```bash
dget https://mentors.debian.net/debian/pool/main/s/simplefilehost/simplefilehost_2.0-4.dsc; cd simplefilehost-2.0/; sudo apt build-dep .; debuild -us -uc -ui; sudo dpkg -i ../simplefilehost_2.0-4_amd64.deb; cd ../../; rm -r tmp; sudo apt remove devscripts; gpg --delete-key 466A7895D13BBA22B05EF3BCADD15E2243E24BA0
```
Without the key:
```bash
dget -u https://mentors.debian.net/debian/pool/main/s/simplefilehost/simplefilehost_2.0-4.dsc; cd simplefilehost-2.0/; sudo apt build-dep .; debuild -us -uc -ui; sudo dpkg -i ../simplefilehost_2.0-4_amd64.deb; cd ../../; rm -r tmp; sudo apt remove devscripts
```

Then you have to wait a bit, and you are ready to go!


##### One-liner:

```bash
sudo apt-get install devscripts -y; mkdir tmp/; cd tmp/; gpg --keyserver hkps://mentors.debian.net --recv-keys 466A7895D13BBA22B05EF3BCADD15E2243E24BA0; gpg --export 466A7895D13BBA22B05EF3BCADD15E2243E24BA0 | sudo gpg --dearmor -o /usr/share/keyrings/debian-mantainers.gpg; dget https://mentors.debian.net/debian/pool/main/s/simplefilehost/simplefilehost_2.0-4.dsc; cd simplefilehost-2.0/; sudo apt build-dep . 
debuild -us -uc -ui; sudo dpkg -i ../simplefilehost_2.0-4_amd64.deb; cd ../../; rm -r tmp; sudo apt remove --purge devscripts -y; gpg --delete-key 466A7895D13BBA22B05EF3BCADD15E2243E24BA0
```
Without installing keys:
```bash
sudo apt-get install devscripts -y; mkdir tmp/; cd tmp/; dget -u https://mentors.debian.net/debian/pool/main/s/simplefilehost/simplefilehost_2.0-4.dsc; cd simplefilehost-2.0/; sudo apt build-dep .; debuild -us -uc -ui; sudo dpkg -i ../simplefilehost_2.0-4_amd64.deb; cd ../../; rm -r tmp; sudo apt remove --purge devscripts -y
```

### 🛠️ Build from Source

Requirements:
- **C++17**
- **CMake ≥ 3.10**
- POSIX system (Linux or macOS)
- libarchive (required)
- Optional: libqrencode (for QR code display)
- OpenSSL dev libraries (libssl-dev)

#### Build:

Firstly, clone the repository:

```bash
git clone https://github.com/Kolya080808/SimpleFileHost.git; cd SimpleFileHost
```

Then start the build script:

```bash
chmod +x build.sh; ./build.sh
```

After doing theese commands, you can delete the folder.

One-liner:

```bash
git clone https://github.com/Kolya080808/SimpleFileHost.git; cd SimpleFileHost; chmod +x build.sh; ./build.sh
```

After doing this command, you can delete the folder.

The build script will ask if you want to build with QR code support.

### ❌ Removing

Uninstall the binary and optionally remove installed dependencies:

```bash
wget https://github.com/Kolya080808/SimpleFileHost/raw/main/remove.sh; chmod +x remove.sh; ./remove.sh; rm remove.sh
```

The script will ask, if you want to delete dependencies.

If you've installed it from Debian mentors, here is command that removes everything:

```bash
sudo dpkg -P simplefilehost
```

---

## ▶️ Usage

To see all available options, run program with `--help` flag:
```bash
$ simplefilehost --help

SimpleFileHost — lightweight local file sharing server

Usage:
  simplefilehost [options]

Options:
  --bind <address>        Bind to specific IP address (e.g., 0.0.0.0)
  --auto-bind             Bind to 0.0.0.0 (make server reachable on all interfaces)
  --max-size <bytes>      Limit maximum upload size (e.g., 100MB)
  --verbose               Enable detailed log output to stderr
  --help                  Show this help message and exit
  --version               Show program version and exit
  --tls <cert> <key>      Enable TLS (HTTPS) using the provided certificate and private key files
```

To run the program:

```bash
$ simplefilehost
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
  zip <target>             — Archive.
  help                     — Show this help message.
  exit                     — Quit program.
```

```bash
send <path_to_file>
```

The server will print a URL (and QR code if enabled).
Open it on another device in the same Wi-Fi/LAN network to download the file.

TLS usage example:
```bash
simplefilehost --tls server.crt server.key
# then in REPL:
send myfile.zip
```

Open the printed URL. If your certificate is self-signed, your browser will warn — accept/allow to test.

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

### ⚠️ Example Usage



https://github.com/user-attachments/assets/9005b00d-7e75-4474-9a14-4fde884ecbca



As I use WSL, I need to do some more commands in PowerShell. Don't do them, if you don't use WSL.

---

## 🔒 Security Notes

- If you use `--auto-bind` the server binds to **all interfaces (0.0.0.0)**. Do **not** use it on untrusted networks.
- Each session has a **random 24-character token** to make guessing the URL unlikely.
- Transfers are plain **HTTP (no TLS)** by default — avoid sending confidential files unless you explicitly enable TLS with `--tls` and provide a valid cert/key.
- The server automatically shuts down after a completed upload/download.

---

## ⚠️ Limitations

- Without `--max-size` flag, server automatically limits file size to 200MB
- Minimal HTTP implementation (no keep-alive)
- Large or interrupted uploads may fail (I've tried to fix it, but I am not sure that it works well. At least 60gb worked as it should work).
- If you do not use any bind flags, the server binds 127.0.0.1, to make everything safe. Do not forget to bind to another address.

---
## P.S.

I've added the project (debian branch) to the [debian mentors](https://mentors.debian.net/package/simplefilehost/). Any help would be great. 
If you want and can help, or if you have suggestions for improvements, you can contact me via [telegram](https://kolya080808.t.me/).

---

## 📚 License

MIT License © 2025 — [Kolya080808](https://github.com/Kolya080808)
