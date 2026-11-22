# Development image

[`Dockerfile`](Dockerfile) defines an image with every tool Zule's builds and
checks use, at pinned versions: GCC, the Arm GNU toolchain, Cppcheck, QEMU,
Python, and, from [`requirements-dev.txt`](../requirements-dev.txt), CMake,
Ninja, clang-format, and the schedule tool's dependencies. CI builds and runs in
the same image.

## With Docker or Podman

Build the image and start a shell in it, from the repository root. Podman takes
the same commands, with `podman` in place of `docker`.

```sh
docker build -f .devcontainer/Dockerfile -t zule-dev .
docker run --rm -it --user "$(id -u):$(id -g)" --env HOME=/tmp \
    --volume "$PWD:/work" --workdir /work zule-dev
```

The repository is mounted at `/work`, so build output lands in your checkout.
`--user` runs the shell as you, so that output is owned by you rather than root,
and `HOME=/tmp` gives that user a writable home directory. The shell's tools are
on `PATH`, with no environment to activate; run the commands in the
[README](../README.md#getting-started) or [CONTRIBUTING.md](../CONTRIBUTING.md)
from it.

## With Dev Containers

Editors and tools that support Dev Containers, such as VS Code, GitHub
Codespaces, JetBrains IDEs, and the `devcontainer` CLI, can open the repository
in the image through [`devcontainer.json`](devcontainer.json), which also adds
the CMake, C/C++, and clang-format extensions in VS Code.
