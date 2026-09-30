@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0play-legacy.ps1"
if errorlevel 1 pause
