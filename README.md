# Extended xv6-riscv

> The MIT **xv6-riscv** teaching operating system, extended with three custom
> kernel feature sets developed for the Operating Systems course at
> **Ben-Gurion University of the Negev**.

![Platform](https://img.shields.io/badge/arch-RISC--V%2064-blue)
![Emulator](https://img.shields.io/badge/run-QEMU-orange)
![Language](https://img.shields.io/badge/language-C-555)
![License](https://img.shields.io/badge/license-MIT-green)

This repository is a single, bootable xv6 kernel that combines three
independent extensions, each adding its own system calls and user-space
programs. Everything builds into one image and runs from the xv6 shell.

---

## Features

### 1. Cooperative coroutines & memory introspection
A direct, cooperative hand-off primitive between two processes.

- **`co_yield(pid, val)`** *(kernel/proc.c)* — wakes the target process at its
  own `co_yield` rendezvous, delivering `val` as that process's return value,
  then blocks the caller until someone yields back. Returns the value passed by
  the next waker, or `-1` on error (e.g. unknown / non-waiting target).
- **`memsize()`** — returns the calling process's current memory size in bytes.

**Programs:** `co_test` (parent/child ping-pong), `co_error_test`
(error-path coverage), `memsize_test` (size before/after `malloc`/`free`),
`helloworld`.

### 2. The "Israeli Lock" & PRNG
A custom synchronization primitive that models favouritism-based queue jumping.

- **PRNG** — a Linear Congruential Generator available in both the kernel
  (`kernel/rand.c`: `lcg_srand` / `lcg_rand`) and user space
  (`user/ulib.c`).
- **Israeli Lock** *(kernel/israeli.c)* — a queue lock with a `favoritism`
  percentage. On release, with probability `favoritism%` the PRNG triggers a
  queue reshuffle so the lock is handed to a waiter from the **same team**
  (process group id) as the previous holder — effectively letting a teammate
  "cut the line." Safe against process termination while sleeping in the queue.
- **Process groups** — `setgid` / `getgid`; a child inherits its parent's gid.

**Syscalls:** `israeli_create`, `israeli_acquire`, `israeli_release`,
`israeli_destroy`, plus a kernel-side scoreboard `israeli_score_inc` /
`israeli_score_get`.

**Program:** `relay_race [favoritism]` — forks 15 runners split into 3 teams,
all competing for one Israeli Lock used as a relay baton; the kernel scoreboard
tallies team wins without data races.

### 3. virtio-GPU framebuffer
A real graphics path: a virtio-gpu driver plus two zero-/low-copy display
syscalls.

- **`kernel/virtio_gpu.c`** — brings up a **640×480** BGRX framebuffer over the
  virtio-gpu device, with a kernel `display_daemon` that flushes the screen
  roughly every 16 ms.
- **`map_display(addr)`** — maps the kernel framebuffer pages directly into the
  calling process (`PTE_U|PTE_R|PTE_W`) so it can write pixels with no copy.
  Pass `0` to let the kernel pick a free virtual address.
- **`flip_display(buf)`** — zero-copy **page flip**: re-points the GPU
  resource's backing pages at the user's page-aligned buffer, without copying
  any pixel data.

**Programs:** `show_flip <text>` and `show_map` (render text to the screen),
and `gol` — Conway's **Game of Life** on an 80×60 grid, in either `flip`
(default, double-buffered) or `map` mode.

> The graphics features need QEMU's `virtio-gpu-device`, which is already wired
> into the `Makefile` (`make qemu` / `make qemu-web`).

---

## System calls added

The three feature sets are assigned one non-overlapping numbering scheme in
`kernel/syscall.h`:

| #  | Name                  | Feature            |
|----|-----------------------|--------------------|
| 22 | `co_yield`            | Coroutines         |
| 23 | `memsize`             | Coroutines         |
| 24 | `setgid`              | Israeli Lock       |
| 25 | `getgid`              | Israeli Lock       |
| 26 | `israeli_create`      | Israeli Lock       |
| 27 | `israeli_acquire`     | Israeli Lock       |
| 28 | `israeli_release`     | Israeli Lock       |
| 29 | `israeli_destroy`     | Israeli Lock       |
| 30 | `israeli_score_inc`   | Israeli Lock       |
| 31 | `israeli_score_get`   | Israeli Lock       |
| 32 | `flip_display`        | virtio-GPU         |
| 33 | `map_display`         | virtio-GPU         |

---

## Building & running

### Requirements
A RISC-V cross toolchain and QEMU. Two easy options:

- **VS Code Dev Container** — open the folder in VS Code and "Reopen in
  Container"; the included [`.devcontainer`](.devcontainer) installs the full
  toolchain for you.
- **Debian / Ubuntu (or WSL):**
  ```bash
  sudo apt update
  sudo apt install build-essential gcc-riscv64-linux-gnu \
                   binutils-riscv64-linux-gnu qemu-system-misc
  ```

### Run
```bash
git clone https://github.com/<your-username>/<repo>.git
cd <repo>
make qemu
```
This boots xv6 with the virtio-gpu device attached. To quit QEMU, press
**Ctrl-a** then **x**.

For a browser-based view of the GPU window (noVNC), use:
```bash
make qemu-web
```

### Try it from the xv6 shell
```text
$ helloworld
$ co_test
$ memsize_test
$ relay_race 50        # 50% favouritism
$ show_flip Hello
$ show_map
$ gol                  # Game of Life (flip mode); 'gol map' for mapped mode
```

---

## Project structure
```
kernel/         xv6 kernel + extensions (israeli.c, rand.c, virtio_gpu.c, ...)
user/           user programs and the C library
mkfs/           builds the initial file-system image (fs.img)
Makefile        build + QEMU targets (qemu, qemu-web, qemu-gdb)
.devcontainer/  ready-to-use toolchain container
```

---

## Credits & license
Built on [xv6-riscv](https://github.com/mit-pdos/xv6-riscv) from MIT PDOS, a
re-implementation of Unix V6 for RISC-V used for teaching. xv6 and these
extensions are released under the **MIT License** — see [LICENSE](LICENSE).

**Authors:** Wesam Gara · Nasr Assi
