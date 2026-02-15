---
# the default layout is 'page'
icon: fas fa-play-circle 
order: 3
title: Getting started
---

# Getting Started with HS5

HS5 is a high-performance, scale-up, self-hosted object storage service compatible with the AWS S3 API. This guide will help you quickly set up and start using HS5.

## Installation

### 1. Using Docker

You can run HS5 easily with Docker:

```bash
docker run -d \
	--name hs5 \
	--restart unless-stopped \
	-e INIT_ROOT_PASSWORD=password \
	-v /path/to/metadata:/metadata \
	-v /path/to/data:/data \
	-p 8085:80 \
	uroni/hs5:latest
```

Or with Docker Compose:

```yml
volumes:
  hs5-data:
  hs5-metadata:

services:
  hs5:
    image: uroni/hs5:latest
    container_name: hs5
    restart: unless-stopped
    environment:
      - INIT_ROOT_ACCESS_KEY=root
      - INIT_ROOT_PASSWORD=password
      - INIT_CREATE_BUCKET=test
    volumes:
      - hs5-metadata:/metadata
      - hs5-data:/data
    ports:
      - "8085:80"
```

### 2. On Linux (AMD64, glibc >= 2.35)

Download and run the HS5 binary:

```bash
wget https://github.com/uroni/hs5/releases/latest/download/hs5.xz -O - | xz -d > hs5
chmod +x hs5
./hs5 run
```

By default, data and metadata are stored in the current directory. On first run, the root password will be printed to stdout.

## Basic Usage

HS5 is API-compatible with AWS S3. You can use standard S3 tools (like `awscli`, `s3cmd`, or SDKs) to interact with HS5.

Example: Using AWS CLI

```bash
aws --endpoint-url http://localhost:8085 s3 cp testfile.txt s3://testbucket/testfile.txt
aws --endpoint-url http://localhost:8085 s3 ls s3://testbucket
```

## Next Steps

- [Read the full README](https://github.com/uroni/hs5#readme) for advanced features and durability details.
- Explore the API compatibility and integration options.
- For troubleshooting or advanced configuration, see the documentation and community resources.

---
HS5 is licensed under LGPLv3+. Self-host your data with confidence and simplicity!
---



