<#
.SYNOPSIS
  Build, upload and monitor the Nextion_Tester sketch with arduino-cli.

.DESCRIPTION
  Thin wrapper around arduino-cli that uses the profiles in
  Nextion_Tester/sketch.yaml, so nothing needs to be installed globally
  except arduino-cli itself.

.EXAMPLE
  .\build.ps1                      # compile for the default profile (uno)
  .\build.ps1 -Profile mega        # compile for a Mega 2560
  .\build.ps1 upload               # compile + upload, auto-detect the COM port
  .\build.ps1 upload -Port COM7    # compile + upload to a specific port
  .\build.ps1 monitor -Port COM7   # open the 115200 baud debug monitor
  .\build.ps1 all -Port COM7       # compile, upload, then monitor
  .\build.ps1 upload -RawDump      # build the raw byte dump variant (no event decoding)
  .\build.ps1 install              # install arduino-cli via winget
#>
[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [ValidateSet('build', 'upload', 'monitor', 'all', 'install', 'ports')]
    [string]$Task = 'build',

    [ValidateSet('uno', 'nano', 'mega')]
    [string]$Profile = 'uno',

    [string]$Port,

    [int]$Baud = 115200,

    # Compile with RAW_DUMP=1: print every byte from the display instead of decoding events.
    [switch]$RawDump
)

$ErrorActionPreference = 'Stop'
$sketch = Join-Path $PSScriptRoot 'Nextion_Tester'

function Assert-Cli {
    if (-not (Get-Command arduino-cli -ErrorAction SilentlyContinue)) {
        Write-Error "arduino-cli not found. Run '.\build.ps1 install' or see README.md."
    }
}

function Find-Port {
    if ($Port) { return $Port }
    $json = arduino-cli board list --format json | ConvertFrom-Json
    $ports = @($json.detected_ports | Where-Object { $_.port.protocol -eq 'serial' })
    $withBoard = @($ports | Where-Object { $_.matching_boards })
    $pick = if ($withBoard.Count -gt 0) { $withBoard[0] } elseif ($ports.Count -gt 0) { $ports[0] } else { $null }
    if (-not $pick) {
        Write-Error "No serial port found. Plug the board in or pass -Port COMx."
    }
    $addr = $pick.port.address
    Write-Host "Using port $addr" -ForegroundColor Cyan
    return $addr
}

function Invoke-Build {
    $extra = @()
    if ($RawDump) {
        Write-Host "RAW_DUMP=1: events will not be decoded" -ForegroundColor Yellow
        $extra += '--build-property'
        $extra += 'build.extra_flags=-DRAW_DUMP=1'
    }
    Write-Host "Compiling $sketch for profile '$Profile'..." -ForegroundColor Cyan
    # The vendored NeoNextion library emits many -Wwrite-strings warnings, so
    # warnings are left at arduino-cli's default (off). Pass -Verbose to see them.
    arduino-cli compile --profile $Profile @extra $sketch
    if ($LASTEXITCODE -ne 0) { throw "compile failed" }
}

function Invoke-Upload {
    $p = Find-Port
    Write-Host "Uploading to $p..." -ForegroundColor Cyan
    arduino-cli upload --profile $Profile -p $p $sketch
    if ($LASTEXITCODE -ne 0) { throw "upload failed" }
}

function Invoke-Monitor {
    $p = Find-Port
    Write-Host "Monitor on $p at $Baud baud (Ctrl+C to exit)" -ForegroundColor Cyan
    arduino-cli monitor -p $p --config "baudrate=$Baud"
}

switch ($Task) {
    'install' {
        winget install --id ArduinoSA.CLI -e --accept-source-agreements --accept-package-agreements
        Write-Host "Open a new terminal so arduino-cli is on PATH, then run .\build.ps1" -ForegroundColor Green
    }
    'ports'   { Assert-Cli; arduino-cli board list }
    'build'   { Assert-Cli; Invoke-Build }
    'upload'  { Assert-Cli; Invoke-Build; Invoke-Upload }
    'monitor' { Assert-Cli; Invoke-Monitor }
    'all'     { Assert-Cli; Invoke-Build; Invoke-Upload; Invoke-Monitor }
}
