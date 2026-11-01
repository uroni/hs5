---
# the default layout is 'page'
icon: fas fa-download
order: 2
---

# Download HS5

The latest release of HS5 is always available at:

[https://github.com/uroni/hs5/releases/latest](https://github.com/uroni/hs5/releases/latest)

Visit the link above to download binaries and release notes for your platform.

## Quick Download (Linux, AMD64)

You can download and extract the latest Linux AMD64 binary directly with:

```bash
wget https://github.com/uroni/hs5/releases/latest/download/hs5-amd64-musl.xz -O - | xz -d > hs5
chmod +x hs5
./hs5 run
```

## Quick Download (Linux, ARM64/aarch64)

For ARM64 (aarch64) systems, use:

```bash
wget https://github.com/uroni/hs5/releases/latest/download/hs5-arm64-musl.xz -O - | xz -d > hs5
chmod +x hs5
./hs5 run
```

These binaries are statically linked against musl and run on any Linux distribution.
Builds dynamically linked against glibc (`hs5-amd64-glibc.xz`, `hs5-arm64-glibc.xz`) are also available on the [release page](https://github.com/uroni/hs5/releases/latest).

See the [README](../index.html) for more usage instructions.
