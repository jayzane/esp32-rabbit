$env:PATH = "C:\Espressif\tools\python\v6.0\venv\Scripts;C:\Espressif\tools\idf6.0_py3.11_env\Scripts;$env:PATH"
$env:IDF_PATH = "C:\esp\v6.0\esp-idf"
. C:\Espressif\tools\Microsoft.v6.0.PowerShell_profile.ps1

$port = "COM8"
Write-Host "Using COM port: $port"

idf.py -p $port flash monitor 2>&1 | Select-Object -Last 50