param(
    [ValidateSet('List', 'Recover')][string]$Action = 'List',
    [ValidateSet('Auto', 'Native', 'WSL')][string]$Backend = 'Auto',
    [string]$ComPort = '',
    [string]$Distro = 'kali-linux',
    [string]$HostAddress = '',
    [int]$TcpPort = 45832,
    [string]$KnownKey = '',
    [string]$ToolsDir = ''
)

$ErrorActionPreference = 'Stop'
if ($KnownKey -and $KnownKey -notmatch '^[0-9A-Fa-f]{12}$') { throw 'KnownKey must be 12 hex characters' }
if (-not $ComPort) {
    $devices = @(Get-CimInstance Win32_PnPEntity | Where-Object {
        $_.PNPDeviceID -match 'VID_303A&PID_1001' -and $_.Name -match '\(COM\d+\)'
    })
    if ($devices.Count -ne 1) { throw 'Could not identify one T-Embed USB serial port. Pass -ComPort COM7 (or your port).' }
    $ComPort = [regex]::Match($devices[0].Name, 'COM\d+').Value
}
$dumpWindows = Join-Path ([Environment]::GetFolderPath('UserProfile')) ('Downloads\bruce-nfc-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '.mfd')
if (-not $ToolsDir) {
    $bundledTools = Join-Path $PSScriptRoot '..\dist\windows-host'
    if ((Test-Path (Join-Path $bundledTools 'nfc-list.exe')) -and
        (Test-Path (Join-Path $bundledTools 'mfoc.exe'))) {
        $ToolsDir = (Resolve-Path $bundledTools).Path
    }
}
$nativeList = if ($ToolsDir) { Join-Path $ToolsDir 'nfc-list.exe' } else { (Get-Command nfc-list.exe -ErrorAction SilentlyContinue).Source }
$nativeMfoc = if ($ToolsDir) { Join-Path $ToolsDir 'mfoc.exe' } else { (Get-Command mfoc.exe -ErrorAction SilentlyContinue).Source }
$nativeReady = $nativeList -and $nativeMfoc -and (Test-Path $nativeList) -and (Test-Path $nativeMfoc)
if ($Backend -eq 'Auto') { $Backend = if ($nativeReady) { 'Native' } else { 'WSL' } }

if ($Backend -eq 'Native') {
    if (-not $nativeReady) { throw 'Native libnfc tools not found. Put nfc-list.exe and mfoc.exe in PATH or pass -ToolsDir, or use -Backend WSL.' }
    $serial = [System.IO.Ports.SerialPort]::new($ComPort, 115200)
    $serial.ReadTimeout = 500
    $serial.WriteTimeout = 5000
    try {
        $serial.Open()
        Start-Sleep -Seconds 5
        $serial.DiscardInBuffer()
        $serial.Write("rfid bridge`n")
        $deadline = [DateTime]::UtcNow.AddSeconds(15)
        $startup = ''
        while ([DateTime]::UtcNow -lt $deadline -and -not $startup.Contains('BRIDGE_READY')) {
            $startup += $serial.ReadExisting()
            Start-Sleep -Milliseconds 50
        }
        if (-not $startup.Contains('BRIDGE_READY')) { throw "Bridge did not start: $startup" }
        $serial.Close()
        $env:LIBNFC_DEVICE = "pn532_uart:$ComPort"
        $env:LIBNFC_AUTO_SCAN = 'false'
        if ($Action -eq 'List') {
            & $nativeList
        } elseif ($KnownKey) {
            & $nativeMfoc -k $KnownKey -O $dumpWindows
        } else {
            & $nativeMfoc -O $dumpWindows
        }
        if ($LASTEXITCODE -ne 0) { throw "Native NFC tool failed with exit code $LASTEXITCODE" }
        if ($Action -eq 'Recover') { Write-Host "Dump saved: $dumpWindows" }
    } finally {
        try {
            if (-not $serial.IsOpen) { $serial.Open() }
            $serial.Write([byte[]](27,66,82,85,67,69,95,69,88,73,84,10), 0, 12)
        } catch { Write-Warning 'Could not exit bridge; press Back or reset the T-Embed' }
        if ($serial.IsOpen) { $serial.Close() }
        $serial.Dispose()
    }
    return
}

if (-not (Get-Command wsl.exe -ErrorAction SilentlyContinue)) {
    throw 'WSL is unavailable and native tools were not found. Install libnfc/mfoc Windows binaries, then use -Backend Native -ToolsDir <folder>.'
}
if (-not $HostAddress) {
    $route = (& wsl.exe -d $Distro -- ip -4 route show default) -join ' '
    if ($LASTEXITCODE -ne 0) { throw "Could not start WSL distribution $Distro" }
    $match = [regex]::Match($route, 'via\s+(\d+\.\d+\.\d+\.\d+)')
    $HostAddress = if ($match.Success) { $match.Groups[1].Value } else { '127.0.0.1' }
}

$relayPath = Join-Path $PSScriptRoot 'bridge-windows.ps1'
$linuxScript = & wsl.exe -d $Distro -- wslpath -u ((Join-Path $PSScriptRoot 'run-mfoc-wsl.sh').Replace('\', '/'))
if ($LASTEXITCODE -ne 0) { throw 'Could not locate the WSL helper script' }
$dumpLinux = & wsl.exe -d $Distro -- wslpath -u ($dumpWindows.Replace('\', '/'))
if ($LASTEXITCODE -ne 0) { throw 'Could not resolve Downloads in WSL' }

$job = Start-Job -FilePath $relayPath -ArgumentList @($ComPort, $HostAddress, $TcpPort, $false)
try {
    $ready = $false
    for ($i = 0; $i -lt 150; $i++) {
        $output = @(Receive-Job -Job $job -Keep -ErrorAction SilentlyContinue)
        if (($output -join "`n").Contains('TCP_LISTEN')) { $ready = $true; break }
        if ($job.State -ne 'Running') { break }
        Start-Sleep -Milliseconds 100
    }
    if (-not $ready) { throw "Relay failed: $((Receive-Job -Job $job -Keep -ErrorAction SilentlyContinue) -join ' ')" }
    Write-Host "T-Embed $ComPort connected; running $Action in WSL..."
    & wsl.exe -d $Distro -- bash $linuxScript $Action.ToLowerInvariant() $HostAddress "$TcpPort" $dumpLinux $KnownKey
    if ($LASTEXITCODE -ne 0) { throw "WSL NFC tool failed with exit code $LASTEXITCODE" }
    if ($Action -eq 'Recover') { Write-Host "Dump saved: $dumpWindows" }
} finally {
    if ($job.State -eq 'Running') { Stop-Job -Job $job }
    Receive-Job -Job $job -ErrorAction SilentlyContinue | Out-Null
    Remove-Job -Job $job -Force
}
