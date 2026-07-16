@echo off
setlocal
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0CleanGenerated.ps1" %*
if not defined CI pause
