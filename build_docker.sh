#!/usr/bin/env bash
set -euo pipefail
IMAGE_NAME=telink-tc32
DOCKERFILE=Dockerfile.tc32
# Reported by the firmware over BLE (E8), e.g. 0.9.0-3-g2843ceb for a build after v0.9.0.
FIRMWARE_VERSION=$(git describe --tags --always 2>/dev/null | sed 's/^v//' || echo dev)

# Build (force amd64 so bundled tc32 Linux toolchain works)
docker build --platform=linux/amd64 -f ${DOCKERFILE} -t ${IMAGE_NAME} .

# Run make inside container
docker run --platform=linux/amd64 --rm -v "$(pwd)":/workspace -w /workspace/Firmware ${IMAGE_NAME} \
	make FIRMWARE_VERSION="${FIRMWARE_VERSION}" "$@"

# Show outputs
ls -lh Firmware/ATC_Paper.bin Firmware/out/ATC_Paper.elf 2>/dev/null || true
