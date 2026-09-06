#!/usr/bin/env bash

ZOLA_VERSION=0.19.2
ZOLA_SHA256=0798e69b86c628ddcb264ebd86c8cc8dce7670b9049060bf94faa73f6857cd9c

set -euo pipefail

url="https://github.com/getzola/zola/releases/download/v${ZOLA_VERSION}/zola-v${ZOLA_VERSION}-x86_64-unknown-linux-gnu.tar.gz"
curl -sSL -o zola.tar.gz "$url"
echo "${ZOLA_SHA256}  zola.tar.gz" | sha256sum -c -
tar xzf zola.tar.gz zola
sudo mv zola /usr/local/bin/zola
zola --version
