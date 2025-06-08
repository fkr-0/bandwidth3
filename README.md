# bandwidth3 – Polybar bandwidth module

**Version 0.1.3**  •  single‑binary C utility that shows per‑adapter throughput, link status and more.

```
                       ┌────── States ──────┐
Nerd‑Font icons →    Ethernet    Wi‑Fi   ⚠  no‑link   ✗  disabled
Rate glyphs     →    download    upload
```

---

## 1  Features

* **Per‑interface view** – prints *one segment per adapter* (no loops).
* **Link state detection** – `connected / disconnected / disabled / err`.
* **Wireless vs. wired** – auto‑detects with `/sys/class/net/**/wireless`.
* **Threshold markers** – `!` (critical) & `?` (warning) based on Rx/Tx bytes.
* **Units** – IEC (KiB) or SI (kB) – selectable via env or CLI.
* **Config precedence** – `defaults < ENV < CLI`.
* **Zero deps** – pure C11, libc only; optional tests use `assert(3)`.

---

## 2  File structure

```text
bandwidth3/
├── include/          public header used by all sources
│   └── netmon.h
├── src/              implementation (modular C files)
│   ├── iface_state.c
│   ├── net_stats.c
│   └── netmon.c      ← main program 🠖 builds to ./build/bandwidth3
├── tests/            zero‑dep asserts + smoke shell test
│   ├── smoke.sh
│   └── test_basic.c
├── docs/
│   └── bandwidth3.1  man page stub (optional)
├── Makefile
└── README.md         ← you are here
```

---

## 3  Environment variables & CLI options

| Priority | ENV var (example)        | CLI flag           | Description                             |
| -------- | ------------------------ | ------------------ | --------------------------------------- |
| 1 (low)  | – *(built‑in defaults)*  | –                  | Bytes/s, 1 s refresh, all non‑lo ifaces |
| 2        | `USE_BITS=1`             | `-b` / `-B`        | Bits/s (`-b`) or Bytes/s (`-B`)         |
|          | `REFRESH_TIME=2`         | `-t 2`             | Seconds per update                      |
|          | `INTERFACES=enp0s3,wlp0` | `-i`               | Comma list to monitor                   |
|          | `WARN_RX=307200`         | `-W rx:tx`         | Warning thresholds (bytes / s)          |
|          | `CRIT_RX=512000`         | `-C rx:tx`         | Critical thresholds                     |
|          | `USE_SI=1`               | `-s`               | Use 1000 divisor                        |
|          | `WIFI_ONLY=1`            | `--wifi-only`      | Filter wireless                         |
|          | `ETH_ONLY=1`             | `--eth-only`       | Filter wired                            |
| –        | –                        | `-V` / `--version` | Print version and quit                  |
| –        | –                        | `-h` / `--help`    | Full help text                          |

> **Note:** CLI overrides env values; env overrides built‑ins.

---

## 4  Polybar snippet

```ini
[module/bandwidth]
type  = custom/script
exec  = env \
        WARN_RX=307200 WARN_TX=30720 \
        CRIT_RX=512000 CRIT_TX=51200  \
        INTERFACES=wlp0s0,enp3s0      \
        /path/to/bandwidth3 -t 1 -B

# keep process alive to avoid respawn every second
tail = true
format-padding = 1
```

Each adapter prints something like:

```
   2.4 M​B/s   110 k​B/s  ⚠  [no-link]
```

---

## 5  Building & installing

```sh
# release build in ./build/
$ make

# debug build (symbols, no optimisations)
$ make debug

# run tests (assert + smoke)
$ make test

# system‑wide install (defaults to PREFIX=/usr/local)
$ sudo make install

# remove files again
$ sudo make uninstall
```

The binary lands in **\$PREFIX/bin/bandwidth3** and an optional manpage in
**\$PREFIX/share/man/man1/**.

---

## 6  Uninstall

```sh
sudo make uninstall      # removes binary & manpage
rm -rf build/            # optional: wipe artefacts
```

---

## 7  Testing

`make test` compiles:

* **tests/test\_basic.c** – unit asserts for version, maths, enum uniqueness.
* **tests/smoke.sh** – launches `bandwidth3 -V`, greps version.

Both must exit 0; otherwise the Make rule fails.

Enjoy a lean, dependency‑free bandwidth read‑out for your Polybar 😊
