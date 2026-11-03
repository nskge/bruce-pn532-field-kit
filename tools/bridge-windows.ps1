param(
    [Parameter(Mandatory = $true)][string]$ComPort,
    [Parameter(Mandatory = $true)][string]$ListenAddress,
    [int]$TcpPort = 45832,
    [bool]$WriteMode = $false
)

$ErrorActionPreference = 'Stop'
$serial = [System.IO.Ports.SerialPort]::new($ComPort, 115200)
$serial.ReadTimeout = 500
$serial.WriteTimeout = 5000
$listener = $null
$client = $null
$started = $false
try {
    $serial.Open()
    Start-Sleep -Seconds 5
    $serial.DiscardInBuffer()
    $command = if ($WriteMode) { 'rfid bridge-write' } else { 'rfid bridge' }
    $serial.Write("$command`n")
    $deadline = [DateTime]::UtcNow.AddSeconds(15)
    $startup = ''
    while ([DateTime]::UtcNow -lt $deadline -and -not $startup.Contains('BRIDGE_READY')) {
        $startup += $serial.ReadExisting()
        Start-Sleep -Milliseconds 50
    }
    if (-not $startup.Contains('BRIDGE_READY')) { throw "Bridge did not start: $startup" }
    $started = $true
    $serial.DiscardInBuffer()
    $listener = [System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::Parse($ListenAddress), $TcpPort)
    $listener.Start()
    Write-Output "TCP_LISTEN $ListenAddress`:$TcpPort"
    $client = $listener.AcceptTcpClient()
    $client.NoDelay = $true
    $stream = $client.GetStream()
    $buffer = [byte[]]::new(4096)
    while ($client.Connected) {
        if ($client.Available -gt 0) {
            $count = $stream.Read($buffer, 0, [Math]::Min($buffer.Length, $client.Available))
            if ($count -le 0) { break }
            $serial.Write($buffer, 0, $count)
        }
        if ($serial.BytesToRead -gt 0) {
            $count = $serial.Read($buffer, 0, [Math]::Min($buffer.Length, $serial.BytesToRead))
            if ($count -gt 0) { $stream.Write($buffer, 0, $count) }
        }
        if ($client.Client.Poll(0, [System.Net.Sockets.SelectMode]::SelectRead) -and $client.Available -eq 0) { break }
        Start-Sleep -Milliseconds 1
    }
} finally {
    if ($client) { $client.Close() }
    if ($listener) { $listener.Stop() }
    if ($started -and $serial.IsOpen) {
        try { $serial.Write([byte[]](27,66,82,85,67,69,95,69,88,73,84,10), 0, 12) } catch {}
    }
    if ($serial.IsOpen) { $serial.Close() }
    $serial.Dispose()
}
