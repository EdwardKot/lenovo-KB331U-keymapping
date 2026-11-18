#!/usr/bin/env bash
set -e
cd "$(dirname "$0")"
echo "启动 Lenovo KB331U WebHID 改键工具..."
echo "请在 Chrome / Edge 中打开: http://localhost:8765"
open "http://localhost:8765" 2>/dev/null || true
python3 -m http.server 8765
