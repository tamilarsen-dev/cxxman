$ErrorActionPreference = "Stop"

$installDirectory = $PSScriptRoot.TrimEnd("\")
$userPath = [Environment]::GetEnvironmentVariable("Path", "User")
$pathEntries = @(
    $userPath -split ";" |
        Where-Object { $_.Trim().TrimEnd("\") -and $_.Trim().TrimEnd("\") -ine $installDirectory }
)
[Environment]::SetEnvironmentVariable("Path", ($pathEntries -join ";"), "User")

Remove-Item -LiteralPath (Join-Path $installDirectory "cxxman.exe") -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath (Join-Path $installDirectory "uninstall.ps1") -Force -ErrorAction SilentlyContinue
if (Test-Path -LiteralPath $installDirectory -PathType Container) {
    $remainingItems = @(Get-ChildItem -LiteralPath $installDirectory -Force)
    if ($remainingItems.Count -eq 0) {
        Remove-Item -LiteralPath $installDirectory -Force
    }
}

Write-Host "Removed cxxman and its PATH entry. The local documentation database was kept."
Write-Host "Open a new terminal for the PATH change to take effect."
