param(
    [ValidateRange(1, 2147483647)]
    [int]$Frame = 120,
    [string]$OutputDirectory = (Join-Path $PSScriptRoot '..\evidence\A06')
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$binDirectory = Join-Path $repoRoot 'bin'
$editorPath = Join-Path $binDirectory 'PiccoloEditor.exe'
$outputDirectoryPath = [System.IO.Path]::GetFullPath($OutputDirectory)
$captureTemplate = Join-Path $outputDirectoryPath 'piccolo-a06'
$capturePath = "${captureTemplate}_capture.rdc"

if (-not (Test-Path -LiteralPath $editorPath)) {
    throw "PiccoloEditor.exe was not found at $editorPath. Build the Release target first."
}

if (Test-Path -LiteralPath $capturePath) {
    throw "Capture already exists: $capturePath. Choose another -OutputDirectory to avoid overwriting it."
}

New-Item -ItemType Directory -Force -Path $outputDirectoryPath | Out-Null

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
    Write-Host "Starting Piccolo. RenderDoc capture will be requested on engine frame $Frame."
    Write-Host "Capture target: $capturePath"
    Write-Host 'Close the editor after the capture has been written.'
    & $editorPath
}
finally {
    Set-Location -LiteralPath $previousLocation
    foreach ($name in $environmentNames) {
        [Environment]::SetEnvironmentVariable($name, $previousEnvironment[$name], 'Process')
    }
}
