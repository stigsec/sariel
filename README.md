# sariel
**sariel** is a minimal C-based tool for scanning an ip address for open ports.

---

## Features
 - changeable batch size (default 5000)
 - outputs a ready `nmap` command

---

## Getting Started

### Installation

1. Clone the repository and navigate to the project directory
```bash
git clone https://github.com/stigsec/sariel.git
cd sariel
```

2. Compile the source code into a binary
```bash
gcc main.c -o sariel
```

---

## Usage

### Syntax

```bash
./sariel ip
```
If you want to change the default `batch size` use the `-b` flag:
```bash
./sariel ip -b BATCH_SIZE
```

### Examples

Default (`5000`) batch size:
```bash
./sariel 192.168.1.1
```
With `10000` `batch size`:
```bash
./sariel 192.168.1.1 -b 10000
```

---

## License
This project is licensed under the GNU General Public License v3.0. See the [LICENSE file](LICENSE) for more details.

---

Developed by [stigsec](https://github.com/stigsec)
