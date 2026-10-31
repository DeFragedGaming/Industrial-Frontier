
$ErrorActionPreference = "Stop"

$ProjectRoot = "C:\Users\DeFra\Documents\Projects\IndustrialFrontier"
$ToolRoot = Join-Path $ProjectRoot "tools\DeveloperPresence"

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host " INDUSTRIAL FRONTIER" -ForegroundColor Cyan
Write-Host " DEVELOPER PRESENCE SETUP" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

if (-not (Test-Path $ProjectRoot)) {
    Write-Host "Industrial Frontier project directory not found." -ForegroundColor Red
    exit 1
}

$Directories = @(
    "src",
    "include",
    "config",
    "assets",
    "logs"
)

foreach ($Directory in $Directories) {
    $Path = Join-Path $ToolRoot $Directory

    New-Item -ItemType Directory -Path $Path -Force | Out-Null

    Write-Host "[CREATED] $Directory" -ForegroundColor Green
}

$ApplicationId = Read-Host "Enter your Discord Application ID (leave blank for now)"

$Configuration = @{
    applicationName = "Industrial Frontier"
    studio = "CyberTrace Studios"
    applicationId = $ApplicationId
    activity = @{
        details = "Developing Industrial Frontier"
        state = "Working on C++ and OpenGL"
    }
    settings = @{
        showElapsedTime = $true
        reconnectAutomatically = $true
        updateIntervalSeconds = 15
    }
}

$ConfigurationPath = Join-Path $ToolRoot "config\presence.json"

$Configuration |
    ConvertTo-Json -Depth 5 |
    Set-Content -Path $ConfigurationPath -Encoding UTF8

$Readme = @"
# Industrial Frontier Developer Presence

A standalone Discord Rich Presence companion for Industrial Frontier.

## Project

- Application: Industrial Frontier
- Studio: CyberTrace Studios
- Language: C++
- Platform: Windows

## Features Planned

- Discord Rich Presence
- Development session timer
- Configurable activity descriptions
- Automatic Discord reconnection
- Background operation
- Development task selection
- Windows system tray integration

## Configuration

Edit:

config/presence.json

Enter your Discord Application ID.

## Development

This companion is independent of the main Industrial Frontier executable.

It will eventually be built as its own Windows application.
"@

$ReadmePath = Join-Path $ToolRoot "README.md"

Set-Content -Path $ReadmePath -Value $Readme -Encoding UTF8

$GitIgnorePath = Join-Path $ToolRoot ".gitignore"

$GitIgnore = @"
logs/
*.log
*.tmp
"@

Set-Content -Path $GitIgnorePath -Value $GitIgnore -Encoding UTF8

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host " SETUP COMPLETE" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan

Write-Host ""
Write-Host "Created: $ToolRoot" -ForegroundColor Green
Write-Host ""
Write-Host "Files created:" -ForegroundColor Yellow
Write-Host "  config\presence.json"
Write-Host "  README.md"
Write-Host "  .gitignore"
Write-Host ""
Write-Host "Developer Presence source directories are ready."
Write-Host ""
Write-Host "Next step: Implement the Discord RPC companion."
Write-Host ""
