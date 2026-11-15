# Node-RED check for Windows: finds Node.js, Node-RED and node-red-dashboard, installs what is missing,
# then prints where everything is and the commands that start Node-RED.
# Run:  double-click check_nodered.cmd, or in a terminal:  check_nodered.cmd [-Yes] [-CheckOnly]
#   -Yes        answer yes to every question (install, add to PATH, start)
#   -CheckOnly  only check, never install or change anything
param([switch]$Yes, [switch]$CheckOnly)

$ErrorActionPreference = 'Continue'
$MinNode = [version]'18.5.0'        # oldest Node.js that Node-RED 4 runs on
$NodeForLatest = [version]'22.9.0'  # Node-RED 5 needs this; older Node.js gets Node-RED 4
$Port = 1880
$UserDir = Join-Path $HOME '.node-red'
$env:npm_config_update_notifier = 'false'
$env:npm_config_fund = 'false'
$env:npm_config_audit = 'false'
$Problems = 0

function Ok($m)   { Write-Host "  [OK]   $m" -ForegroundColor Green }
function Warn($m) { Write-Host "  [WARN] $m" -ForegroundColor Yellow }
function Bad($m)  { Write-Host "  [FAIL] $m" -ForegroundColor Red; $script:Problems++ }
function Info($m) { Write-Host "         $m" }

function Ask($q) {
    if ($CheckOnly) { return $false }
    if ($Yes) { return $true }
    $a = Read-Host "  $q [Y/n]"
    return ($a -eq '' -or $a -match '^[yY]')
}

function Refresh-Path {
    # pick up PATH changes made by an installer without reopening the window
    $env:Path = [Environment]::GetEnvironmentVariable('Path', 'Machine') + ';' + [Environment]::GetEnvironmentVariable('Path', 'User')
}

function On-SavedPath($dir) {
    # is $dir on the PATH that new windows get (not just this window)?
    $saved = [Environment]::GetEnvironmentVariable('Path', 'Machine') + ';' + [Environment]::GetEnvironmentVariable('Path', 'User')
    foreach ($p in $saved.Split(';')) {
        if ($p -and ([Environment]::ExpandEnvironmentVariables($p).TrimEnd('\') -eq $dir.TrimEnd('\'))) { return $true }
    }
    return $false
}

function Find-Node {
    $cmd = Get-Command node.exe -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    # installed, but not on PATH
    foreach ($p in @("$env:ProgramFiles\nodejs\node.exe", "${env:ProgramFiles(x86)}\nodejs\node.exe",
                     "$env:LOCALAPPDATA\Programs\nodejs\node.exe", "$env:NVM_SYMLINK\node.exe")) {
        if ((Test-Path $p) -and $p -notlike '\*') { return $p }
    }
    return $null
}

function Node-Version($node) {
    return [version]((& $node -v).Trim().TrimStart('v'))
}

function Winget-Node($verb) {
    if (-not (Get-Command winget -ErrorAction SilentlyContinue)) {
        Bad "winget is not available on this computer"
        return
    }
    winget $verb -e --id OpenJS.NodeJS.LTS
    Refresh-Path
}

function Package-Version($dir) {
    $pj = Join-Path $dir 'package.json'
    if (Test-Path $pj) { return (Get-Content $pj -Raw | ConvertFrom-Json).version }
    return $null
}

# 0 = Node-RED answers on the port, 2 = something else uses the port, 1 = the port is free
function Port-State($node) {
    $js = "fetch('http://127.0.0.1:$Port/',{signal:AbortSignal.timeout(3000)}).then(r=>r.text())" +
          ".then(t=>process.exit(/Node-RED/.test(t)?0:2)).catch(e=>process.exit(e.cause&&e.cause.code==='ECONNREFUSED'?1:2))"
    & $node -e $js 2>$null
    return $LASTEXITCODE
}

Write-Host "Node-RED check (Windows)"
Write-Host ""

# ---------------------------------------------------------------- 1) Node.js
Write-Host "1) Node.js"
$node = Find-Node
if (-not $node) {
    Bad "Node.js is not installed"
    if (Ask "Install Node.js LTS with winget?") { Winget-Node 'install'; $node = Find-Node }
    if (-not $node) {
        Info "Install the LTS version from https://nodejs.org, then run this check again."
        exit 1
    }
    $script:Problems--
}
$nodeDir = Split-Path $node
if (-not (Get-Command node.exe -ErrorAction SilentlyContinue)) {
    Warn "Node.js is installed but not on PATH (typing 'node' does not work)"
    $env:Path = "$nodeDir;$env:Path"
}
$nodeVer = Node-Version $node
if ($nodeVer -lt $MinNode) {
    Bad "Node.js $nodeVer is too old (Node-RED needs $MinNode or newer)"
    if (Ask "Upgrade Node.js to the LTS version with winget?") {
        Winget-Node 'upgrade'
        $node = Find-Node; $nodeDir = Split-Path $node; $nodeVer = Node-Version $node
    }
    if ($nodeVer -lt $MinNode) {
        Info "Install the LTS version from https://nodejs.org, then run this check again."
        exit 1
    }
    $script:Problems--
}
Ok "$node (v$nodeVer)"

$npm = Join-Path $nodeDir 'npm.cmd'
if (-not (Test-Path $npm)) {
    Bad "npm is missing next to node.exe - reinstall Node.js from https://nodejs.org"
    exit 1
}
$npmPrefix = "$(& $npm prefix -g)".Trim()
$npmRoot = "$(& $npm root -g)".Trim()
Ok "npm $("$(& $npm -v)".Trim()), global packages in $npmRoot"

# ---------------------------------------------------------------- 2) Node-RED
Write-Host ""
Write-Host "2) Node-RED"
$want = if ($nodeVer -ge $NodeForLatest) { 'node-red' } else { 'node-red@4' }
$redDir = Join-Path $npmRoot 'node-red'
$redVer = Package-Version $redDir
if (-not $redVer) {
    # installed with another npm prefix? follow the node-red command if there is one
    $cmd = Get-Command node-red.cmd -ErrorAction SilentlyContinue
    if ($cmd) {
        $other = Join-Path (Split-Path $cmd.Source) 'node_modules\node-red'
        if (Package-Version $other) { $redDir = $other; $npmPrefix = Split-Path $cmd.Source; $redVer = Package-Version $other }
    }
}
if ($redVer -and ([version]$redVer).Major -ge 5 -and $nodeVer -lt $NodeForLatest) {
    Bad "Node-RED $redVer needs Node.js $NodeForLatest or newer, this computer has $nodeVer (this is why it does not start)"
    Info "Fix: install Node-RED 4 (works with this Node.js), or upgrade Node.js and run this check again"
    $redVer = $null
}
if (-not $redVer) {
    if (-not (Test-Path (Join-Path $redDir 'package.json'))) { Bad "Node-RED is not installed" }
    if (Ask "Install $want now (npm install -g $want)?") {
        & $npm install -g --unsafe-perm $want
        $redDir = Join-Path $npmRoot 'node-red'
        $npmPrefix = "$(& $npm prefix -g)".Trim()
        $redVer = Package-Version $redDir
        if ($redVer) { $script:Problems-- }
    }
    if (-not $redVer) {
        Info "Install it by hand:  `"$npm`" install -g --unsafe-perm $want"
        exit 1
    }
}
$redJs = Join-Path $redDir 'red.js'
$launcher = Join-Path $npmPrefix 'node-red.cmd'
Ok "Node-RED $redVer in $redDir"

$onPath = On-SavedPath $npmPrefix
if (-not $onPath) {
    Warn "the folder with node-red.cmd is not on PATH, so typing 'node-red' does not work"
    Info $npmPrefix
    if (Ask "Add it to your PATH (for new windows)?") {
        $userPath = [Environment]::GetEnvironmentVariable('Path', 'User')
        [Environment]::SetEnvironmentVariable('Path', ($userPath.TrimEnd(';') + ';' + $npmPrefix).TrimStart(';'), 'User')
        Refresh-Path
        $onPath = $true
        Ok "added - open a new terminal window for it to take effect"
    }
}
$policy = Get-ExecutionPolicy
$psBlocked = ($policy -eq 'Restricted' -or $policy -eq 'AllSigned') -and (Test-Path (Join-Path $npmPrefix 'node-red.ps1'))
if ($onPath -and $psBlocked) {
    Warn "in PowerShell, 'node-red' is blocked by the script policy ($policy): type 'node-red.cmd' instead"
}

# ---------------------------------------------------------------- 3) Dashboard
Write-Host ""
Write-Host "3) node-red-dashboard"
$dashDir = Join-Path $UserDir 'node_modules\node-red-dashboard'
$dashVer = Package-Version $dashDir
if (-not $dashVer) {
    Bad "node-red-dashboard is not installed in $UserDir"
    if (Ask "Install node-red-dashboard now?") {
        New-Item -ItemType Directory -Force $UserDir | Out-Null
        Push-Location $UserDir
        & $npm install --omit=dev node-red-dashboard
        Pop-Location
        $dashVer = Package-Version $dashDir
        if ($dashVer) { $script:Problems--; Info "(restart Node-RED if it is already running)" }
    }
    if (-not $dashVer) { Info "Or in Node-RED: menu > Manage palette > Install > node-red-dashboard" }
}
if ($dashVer) { Ok "node-red-dashboard $dashVer in $dashDir" }

# ---------------------------------------------------------------- 4) Port
Write-Host ""
Write-Host "4) Port $Port"
$state = Port-State $node
$running = $false
switch ($state) {
    0 { Ok "Node-RED is already running at http://127.0.0.1:$Port"; $running = $true }
    1 { Ok "port $Port is free" }
    default {
        Warn "another program is using port $Port"
        Info "close it, or start Node-RED on another port by adding  -p 1881  to the commands below"
    }
}

# ---------------------------------------------------------------- Summary
Write-Host ""
Write-Host "Paths"
Info "Node.js      $node"
Info "Node-RED     $redDir"
Info "Dashboard    $(if ($dashVer) { $dashDir } else { '(not installed)' })"
Info "User folder  $UserDir   (your flows and settings are saved here)"
Write-Host ""
Write-Host "Start Node-RED with any one of these (stop it with Ctrl+C):"
if ($onPath) {
    if ($psBlocked) { Info "node-red.cmd" } else { Info "node-red" }
}
Info "`"$launcher`""
Info "`"$node`" `"$redJs`""
Write-Host "Then open  http://127.0.0.1:$Port  (editor)  and  http://127.0.0.1:$Port/ui  (dashboard)"
Write-Host ""

if ($Problems -gt 0) {
    Bad "$Problems problem(s) left - see above"
    exit 1
}
if (-not $running -and $state -eq 1 -and (Ask "Start Node-RED now in a new window?")) {
    Start-Process -FilePath $node -ArgumentList "`"$redJs`""
    for ($i = 0; $i -lt 30 -and (Port-State $node) -ne 0; $i++) { Start-Sleep 1 }
    if ((Port-State $node) -eq 0) {
        Ok "Node-RED started"
        Start-Process "http://127.0.0.1:$Port"
    } else {
        Warn "Node-RED did not answer within 30 s - look at the new window for the error"
    }
}
exit 0
