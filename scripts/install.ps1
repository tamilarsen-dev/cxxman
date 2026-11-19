$ErrorActionPreference = "Stop"

$sourceExecutable = Join-Path $PSScriptRoot "cxxman.exe"
$sourceUninstaller = Join-Path $PSScriptRoot "uninstall.ps1"
if (-not (Test-Path -LiteralPath $sourceExecutable -PathType Leaf)) {
    throw "cxxman.exe must be next to install.ps1. Extract the full release ZIP first."
}
if (-not $env:LOCALAPPDATA) {
    throw "LOCALAPPDATA is not set; install cxxman manually and add its folder to PATH."
}

$installDirectory = Join-Path $env:LOCALAPPDATA "Programs\cxxman"
New-Item -ItemType Directory -Path $installDirectory -Force | Out-Null
Copy-Item -LiteralPath $sourceExecutable -Destination (Join-Path $installDirectory "cxxman.exe") -Force
if (Test-Path -LiteralPath $sourceUninstaller -PathType Leaf) {
    Copy-Item -LiteralPath $sourceUninstaller -Destination (Join-Path $installDirectory "uninstall.ps1") -Force
}

$userPath = [Environment]::GetEnvironmentVariable("Path", "User")
$pathEntries = @($userPath -split ";" | Where-Object { $_.Trim().TrimEnd("\") })
$alreadyPresent = $false
foreach ($entry in $pathEntries) {
    if ($entry.Trim().TrimEnd("\") -ieq $installDirectory.TrimEnd("\")) {
        $alreadyPresent = $true
        break
    }
}
if (-not $alreadyPresent) {
    $pathEntries += $installDirectory
    [Environment]::SetEnvironmentVariable("Path", ($pathEntries -join ";"), "User")
}

Write-Host "Installed cxxman to $installDirectory"
Write-Host "Open a new terminal, then run: cxxman --help"
Write-Host "To remove it later, run: & `"$installDirectory\uninstall.ps1`""
