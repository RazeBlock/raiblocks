#!/bin/bash
set -euo pipefail
IFS=$'\n\t'

mkdir -p ~/Raze
if [ ! -f ~/Raze/config.json ]; then
  echo "Config File not found, adding default."
  cp /usr/share/raze/config.json ~/Raze/
fi
/usr/bin/raze_node --daemon
