# Dev container for this xv6-riscv project:
# RISC-V cross toolchain + QEMU + noVNC web viewer.
#
# Debian "bookworm" (current stable). The toolchain is installed in a SINGLE
# RUN layer: keeping "apt-get update" and "apt-get install" together avoids the
# stale-cache 404 errors you get when the package index layer is cached but the
# distro mirror has since rotated package versions.
FROM mcr.microsoft.com/devcontainers/base:bookworm

RUN apt-get update \
 && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
      git \
      build-essential \
      gdb-multiarch \
      qemu-system-misc \
      gcc-riscv64-linux-gnu \
      binutils-riscv64-linux-gnu \
      novnc \
      websockify \
 && apt-get clean \
 && rm -rf /var/lib/apt/lists/* \
 && echo "set architecture riscv:rv64" > /root/.gdbinit

