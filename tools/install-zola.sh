#!/usr/bin/env bash

ZOLA_VERSION=0.23.4
ZOLA_SHA256=d99c51302ebbf909a0d83d4319d4d745b56a93dc49c4a69878c0f0dcaa4c8531

set -euo pipefail

url="https://github.com/getzola/zola/releases/download/v${ZOLA_VERSION}/zola-v${ZOLA_VERSION}-x86_64-unknown-linux-musl.tar.gz"
curl -sSL -o zola.tar.gz "$url"
echo "${ZOLA_SHA256}  zola.tar.gz" | sha256sum -c -
tar xzf zola.tar.gz zola
sudo mv zola /usr/local/bin/zola
zola --version
