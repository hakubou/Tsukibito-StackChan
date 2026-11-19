@echo off
chcp 65001 >nul
rem StackChan bridge: relays game_end from print_server.py to StackChan over USB. Keep this window open.
rem Then start the printer with: python print_server.py --stackchan http://127.0.0.1:50090/event
cd /d %~dp0
python -c "import serial" 2>nul || python -m pip install pyserial
python stackchan_bridge.py %*
pause
