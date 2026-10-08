#!/usr/bin/env python3
"""Copy the firmware of the GitHub releases into the web tool, with an index.

The web tool lists the releases and updates a tag from one of them. Browsers can't download GitHub
release assets (no CORS headers), so the Pages build bundles them: <out>/<version>.bin for each
published release from MIN_VERSION on with a .bin asset, and <out>/index.json, newest first:

    [{"version": "0.10.0", "tag": "v0.10.0", "date": "2026-10-08T21:23:16Z",
      "file": "0.10.0.bin", "size": 81234, "url": "https://github.com/.../releases/tag/v0.10.0"}, ...]

Usage: bundle_firmware_releases.py --repo owner/name --out web_tools/static/firmware
Uses $GITHUB_TOKEN when set (higher API rate limit).
"""
import argparse
import json
import os
import re
import sys
import urllib.request

API = "https://api.github.com"
# Releases before 0.7.0 store images over the flash sectors holding the MAC address and radio
# calibration; the web tool doesn't offer them.
MIN_VERSION = (0, 7, 0)


def request(url: str, accept: str) -> bytes:
    headers = {"Accept": accept, "User-Agent": "stellar-firmware-bundler"}
    token = os.environ.get("GITHUB_TOKEN")
    if token:
        headers["Authorization"] = f"Bearer {token}"
    with urllib.request.urlopen(urllib.request.Request(url, headers=headers), timeout=60) as response:
        return response.read()


def version_key(version: str) -> tuple:
    return tuple(int(part) for part in re.findall(r"\d+", version)[:3])


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--repo", required=True)
    parser.add_argument("--out", required=True)
    args = parser.parse_args()

    releases = json.loads(request(f"{API}/repos/{args.repo}/releases?per_page=100", "application/vnd.github+json"))
    os.makedirs(args.out, exist_ok=True)
    index = []
    for release in releases:
        if release["draft"] or release["prerelease"] or not re.fullmatch(r"v?\d+\.\d+\.\d+", release["tag_name"]):
            continue
        version = release["tag_name"].lstrip("v")
        asset = next((a for a in release["assets"] if a["name"].endswith(".bin")), None)
        if asset is None or version_key(version) < MIN_VERSION:
            continue
        data = request(asset["url"], "application/octet-stream")
        if data[8:12] != b"KNLT":
            print(f"skipping {release['tag_name']}: {asset['name']} is not a Telink image", file=sys.stderr)
            continue
        file = f"{version}.bin"
        with open(os.path.join(args.out, file), "wb") as out:
            out.write(data)
        index.append({
            "version": version,
            "tag": release["tag_name"],
            "date": release["published_at"],
            "file": file,
            "size": len(data),
            "url": release["html_url"],
        })

    index.sort(key=lambda entry: version_key(entry["version"]), reverse=True)
    with open(os.path.join(args.out, "index.json"), "w") as out:
        json.dump(index, out, indent=1)
    print(f"{len(index)} releases in {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
