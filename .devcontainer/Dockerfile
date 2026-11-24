# Zule development and CI image: the source of truth for host tools. It pins the
# tools that affect results; requirements-dev.txt pins the Python-installed ones
# (CMake, Ninja, clang-format, and the schedule tool's dependencies), which are
# installed from that file rather than duplicated here. Build from the
# repository root:
#
#   docker build -f .devcontainer/Dockerfile -t zule-dev .
FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive

# Pinned: the host compiler, the Arm GNU toolchain and its C library, newlib, Cppcheck,
# QEMU, and Python. Supporting
# packages, such as certificates and git, follow the release's updates.
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        gcc=4:13.2.0-7ubuntu1 \
        gcc-13=13.3.0-6ubuntu2~24.04.1 \
        libc6-dev \
        gcc-arm-none-eabi=15:13.2.rel1-2 \
        binutils-arm-none-eabi=2.42-1ubuntu1+23 \
        libnewlib-arm-none-eabi=4.4.0.20231231-2 \
        cppcheck=2.13.0-2ubuntu3 \
        qemu-system-arm=1:8.2.2+ds-0ubuntu1.18 \
        python3=3.12.3-0ubuntu2.1 \
        python3-venv=3.12.3-0ubuntu2.1 \
        ca-certificates \
        git \
    && rm -rf /var/lib/apt/lists/*

# requirements-dev.txt includes the schedule tool's requirements by relative path.
COPY requirements-dev.txt /tmp/requirements/
COPY zule/tools/schedule/requirements.txt /tmp/requirements/zule/tools/schedule/
RUN python3 -m venv /opt/venv \
    && /opt/venv/bin/python -m pip install --no-cache-dir -r /tmp/requirements/requirements-dev.txt \
    && rm -rf /tmp/requirements
ENV PATH="/opt/venv/bin:${PATH}"
