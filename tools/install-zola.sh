#!/usr/bin/env bash

ZOLA_VERSION=0.23.4
ZOLA_SHA256=d99c51302ebbf909a0d83d4319d4d745b56a93dc49c4a69878c0f0dcaa4c8531

set -euo pipefail

url="https://github.com/getzola/zola/releases/download/v${ZOLA_VERSION}/zola-v${ZOLA_VERSION}-x86_64-unknown-linux-musl.tar.gz"
curl -sSL -o zola.tar.gz "$url"
echo "${ZOLA_SHA256}  zola.tar.gz" | sha256sum -c -
tar xzf zola.tar.gz zola
rm zola.tar.gz

SUDO=""
if [ "$(id -u)" -ne 0 ] && command -v sudo >/dev/null 2>&1 ; then
    sudo mv zola /usr/local/bin/zola
else
    echo "Can't install with root, installing to '$HOME/.local/bin/' make sure it is on your PATH"
    mv zola $HOME/.local/bin/zola
fi
zola --version
