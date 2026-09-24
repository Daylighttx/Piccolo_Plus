param(
    [ValidateRange(1, 2147483647)]
    [int]$Frame = 120
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$binDirectory = Join-Path $repoRoot 'bin'
$editorPath = Join-Path $binDirectory 'PiccoloEditor.exe'
$evidenceDirectory = Join-Path $repoRoot 'docs\render-learning\evidence\A05'
$captureTemplate = Join-Path $evidenceDirectory 'piccolo-a05'

if (-not (Test-Path -LiteralPath $editorPath)) {
    throw "PiccoloEditor.exe was not found at $editorPath. Build the Release target first."
}

New-Item -ItemType Directory -Force -Path $evidenceDirectory | Out-Null
$environmentNames = @(
    'ENABLE_VULKAN_RENDERDOC_CAPTURE',
    'PICCOLO_VULKAN_DEBUG_LABELS',
    'PICCOLO_RENDERDOC_CAPTURE_FRAME',
    'PICCOLO_RENDERDOC_CAPTURE_FILE'
)
$previousEnvironment = @{}
foreach ($name in $environmentNames) {
    $previousEnvironment[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
}
$previousLocation = Get-Location

try {
    $env:ENABLE_VULKAN_RENDERDOC_CAPTURE = '1'
    $env:PICCOLO_VULKAN_DEBUG_LABELS = '1'
    $env:PICCOLO_RENDERDOC_CAPTURE_FRAME = [string]$Frame
    $env:PICCOLO_RENDERDOC_CAPTURE_FILE = $captureTemplate

    Set-Location -LiteralPath $binDirectory
    Write-Host "Starting Piccolo. Capture will be requested on engine frame $Frame."
    Write-Host 'Close the editor after the capture has been written.'
    & $editorPath
}
finally {
    Set-Location -LiteralPath $previousLocation
    foreach ($name in $environmentNames) {
        [Environment]::SetEnvironmentVariable($name, $previousEnvironment[$name], 'Process')
    }
}
