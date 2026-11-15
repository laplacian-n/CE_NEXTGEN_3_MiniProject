# MQTT connection check for Windows: finds Python (installs it if missing), runs mqtt_check.py,
# then prints where Python is and the commands to run the checks yourself.
# Run:  double-click check_mqtt.cmd, or in a terminal:  check_mqtt.cmd [url] [-u USERNAME]
#   any arguments go straight to mqtt_check.py (see the top of mqtt_check.py)
#   -Yes  install Python without asking
# (plain $args instead of param(): PowerShell would try to bind -u / -p to its own parameters)
$Yes = $args -contains '-Yes'
$CheckArgs = @($args | Where-Object { $_ -ne '-Yes' })

$ErrorActionPreference = 'Continue'
$Here = $PSScriptRoot

function Ok($m)   { Write-Host "  [OK]   $m" -ForegroundColor Green }
function Bad($m)  { Write-Host "  [FAIL] $m" -ForegroundColor Red }
function Info($m) { Write-Host "         $m" }

function Ask($q) {
    if ($Yes) { return $true }
    $a = Read-Host "  $q [Y/n]"
    return ($a -eq '' -or $a -match '^[yY]')
}

function Refresh-Path {
    # pick up PATH changes made by an installer without reopening the window
    $env:Path = [Environment]::GetEnvironmentVariable('Path', 'Machine') + ';' + [Environment]::GetEnvironmentVariable('Path', 'User')
}

function Find-Python {
    # "python" may be the Microsoft Store shortcut that only opens the Store, so run it to be sure
    foreach ($c in @(@('py', '-3'), @('python'), @('python3'))) {
        if (-not (Get-Command $c[0] -ErrorAction SilentlyContinue)) { continue }
        $rest = @($c | Select-Object -Skip 1)
        $p = & $c[0] @rest -c "import sys; print(sys.executable if sys.version_info >= (3, 7) else '')" 2>$null
        if ($LASTEXITCODE -eq 0 -and $p) { return "$p".Trim() }
    }
    # installed, but not on PATH
    $found = Get-ChildItem "$env:LOCALAPPDATA\Programs\Python\Python3*\python.exe", "$env:ProgramFiles\Python3*\python.exe" -ErrorAction SilentlyContinue |
        Sort-Object FullName -Descending | Select-Object -First 1
    if ($found) { return $found.FullName }
    return $null
}

Write-Host "MQTT connection check"
Write-Host ""
Write-Host "Python"
$py = Find-Python
if (-not $py) {
    Bad "Python 3 is not installed"
    if (Ask "Install Python 3.12 with winget?") {
        if (Get-Command winget -ErrorAction SilentlyContinue) {
            winget install -e --id Python.Python.3.12
            Refresh-Path
            $py = Find-Python
        } else {
            Bad "winget is not available on this computer"
        }
    }
    if (-not $py) {
        Info "Install Python 3 from https://www.python.org/downloads/ (tick 'Add python.exe to PATH'),"
        Info "then run this check again."
        exit 1
    }
}
$pyVer = (& $py -c "import platform; print(platform.python_version())").Trim()
Ok "$py ($pyVer)"
Write-Host ""

& $py "$Here\mqtt_check.py" @CheckArgs
$rc = $LASTEXITCODE

Write-Host ""
Write-Host "Run the checks yourself (copy one line):"
Info "`"$py`" `"$Here\mqtt_check.py`" mqtt://mqtt.ce-esl.com:1883 -u USERNAME"
Info "`"$py`" `"$Here\mqtt_check.py`" wss://mqtt.ce-esl.com:443/mqtt -u USERNAME"
Info "`"$py`" `"$Here\mqtt_listen.py`" CENEXTGEN3/GROUPNAME/# -u USERNAME"
exit $rc
