$env:IDF_PATH = "C:\esp\v6.0\esp-idf"
$env:IDF_TOOLS_PATH = "C:\Espressif\tools"
$env:IDF_PYTHON_ENV_PATH = "C:\Espressif\tools\python\v6.0\venv"
$env:PATH = "C:\Espressif\tools\python\v6.0\venv;C:\Espressif\tools\esptool_py\esptool;C:\Espressif\tools\esp32ulp-elf;C:\Espressif\tools\cmake\bin;C:\Espressif\tools\openocd-esp32\v0.12.0-esp32-20251215\bin;C:\Espressif\tools\python\v6.0\venv\Scripts;C:\WINDOWS\system32;C:\WINDOWS;C:\WINDOWS\System32\Wbem;C:\WINDOWS\System32\WindowsPowerShell\v1.0;C:\WINDOWS\System32\OpenSSH;C:\ProgramData\chocolatey\bin;C:\esp\v6.0\esp-idf\tools"
Set-Location "E:\projects\esp32-rabbit"
& "C:\esp\v6.0\esp-idf\tools\idf.py" build