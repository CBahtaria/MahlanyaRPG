@echo off
REM MahlanyaRPG — UltraLowEnd launch (integrated GPU / low-spec PC)
REM Bypasses Nanite, Lumen, and DX12 features; forces DX11 + 720p windowed
SET GAME_PATH=%~dp0..\..\MahlanyaRPG.exe
IF NOT EXIST "%GAME_PATH%" (
    echo ERROR: MahlanyaRPG.exe not found at %GAME_PATH%
    echo        Build the game first from the UE5 editor (File → Package Project → Windows)
    pause
    exit /b 1
)
start "" "%GAME_PATH%" -dx11 -ResX=1280 -ResY=720 -windowed -NOSOUND -mahlanya.ForceHardwareTier=0 -nologbatching
