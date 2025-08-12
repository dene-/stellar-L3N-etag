#!/usr/bin/env bash
set -euo pipefail
IMAGE_NAME=telink-tc32
DOCKERFILE=Dockerfile.tc32

# Build (force amd64 so bundled tc32 Linux toolchain works)
docker build --platform=linux/amd64 -f ${DOCKERFILE} -t ${IMAGE_NAME} .

# Run make inside container
docker run --platform=linux/amd64 --rm -v "$(pwd)":/workspace -w /workspace/Firmware ${IMAGE_NAME} make "$@"

# Show outputs
ls -lh Firmware/ATC_Paper.bin Firmware/out/ATC_Paper.elf 2>/dev/null || true
