@echo off
cd /d "%~dp0"
echo Starting Lenovo KB331U WebHID Keymap Tool...
echo Please open http://localhost:8765 in Chrome or Edge.
start "" "http://localhost:8765"
where py >nul 2>nul
if %errorlevel%==0 (
    py -m http.server 8765
) else (
    python -m http.server 8765
)
pause
