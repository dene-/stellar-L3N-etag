#!/usr/bin/env bash
# Builds the firmware (Firmware/ATC_Paper.bin) in a container. Arguments go to make, e.g.
# `./build_docker.sh clean`. Set REBUILD=1 to rebuild the image.
set -euo pipefail
cd "$(dirname "$0")"

IMAGE=telink-tc32
DOCKERFILE=Dockerfile.tc32
PLATFORM=linux/amd64 # the bundled toolchain is x86-64 only
HASH_LABEL=stellar.dockerfile-hash
# Reported by the firmware over BLE (E8), e.g. 0.9.0-3-g2843ceb for a build after v0.9.0.
FIRMWARE_VERSION=$(git describe --tags --always 2>/dev/null | sed 's/^v//' || echo dev)

# The image is only rebuilt when the Dockerfile changed since it was built, and the image it replaces
# is removed instead of being left behind untagged.
hash=$(cksum <"$DOCKERFILE" | cut -d ' ' -f 1)
built_hash=$(docker image inspect -f "{{index .Config.Labels \"$HASH_LABEL\"}}" "$IMAGE" 2>/dev/null || true)
if [ "$hash" != "$built_hash" ] || [ "${REBUILD:-0}" = 1 ]; then
	old_id=$(docker image inspect -f '{{.Id}}' "$IMAGE" 2>/dev/null || true)
	# No build context: the Dockerfile copies nothing, the sources are mounted when it runs.
	docker build --platform="$PLATFORM" --label "$HASH_LABEL=$hash" -t "$IMAGE" - <"$DOCKERFILE"
	new_id=$(docker image inspect -f '{{.Id}}' "$IMAGE")
	if [ -n "$old_id" ] && [ "$old_id" != "$new_id" ]; then
		docker image rm "$old_id" >/dev/null 2>&1 || true
	fi
fi

# As the calling user, so the build output is not owned by root on Linux.
docker run --rm --platform="$PLATFORM" --user "$(id -u):$(id -g)" \
	-v "$PWD":/workspace -w /workspace/Firmware "$IMAGE" \
	make FIRMWARE_VERSION="$FIRMWARE_VERSION" "$@"

ls -lh Firmware/ATC_Paper.bin Firmware/out/ATC_Paper.elf 2>/dev/null || true
