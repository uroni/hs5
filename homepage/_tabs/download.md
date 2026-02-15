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
wget https://github.com/uroni/hs5/releases/latest/download/hs5.xz -O - | xz -d > hs5
chmod +x hs5
./hs5 run
```

## Quick Download (Linux, ARM64/aarch64)

For ARM64 (aarch64) systems, use:

```bash
wget https://github.com/uroni/hs5/releases/latest/download/hs5-arm64.xz -O - | xz -d > hs5
chmod +x hs5
./hs5 run
```

See the [README](../index.html) for more usage instructions.
