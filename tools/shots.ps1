# Renders every screen to artifacts/*.png, and with -Check, compares them against the committed
# references in tools/golden (plan 5.2, layer 3). References change only through -Bless, which is a
# deliberate act: re-blessing after a real change is a reviewed decision, not a way to make the test
# pass.
param(
    [string]$Exe  = "$PSScriptRoot\..\build\Release\Opra.exe",
    [string]$Out  = "$PSScriptRoot\..\artifacts",
    [string]$Golden = "$PSScriptRoot\golden",
    [int]$Seconds = 6,
    [switch]$Check,
    [switch]$Bless
)

Add-Type -AssemblyName System.Drawing
$ErrorActionPreference = "Stop"

$golden = $Golden
# The thresholds. Rendered frames are not bit-identical across runs - a glyph can land on a
# different subpixel - so the comparison is perceptual: 8x8 blocks, and a block is "over" when its
# mean absolute channel difference exceeds BLOCK_TOL. A handful of over-blocks is antialiasing; a
# percent of the frame is a change.
$BLOCK = 8
$BLOCK_TOL = 6.0
$MAX_OVER_FRACTION = 0.005
$MAX_MEAN = 1.5

function Compare-Frame([string]$a, [string]$b) {
    $ia = [System.Drawing.Bitmap]::FromFile($a)
    $ib = [System.Drawing.Bitmap]::FromFile($b)
    $result = @{ mean = 0.0; over = 0; blocks = 0; size = $false }
    try {
        if ($ia.Width -ne $ib.Width -or $ia.Height -ne $ib.Height) {
            $result.size = $true
            return $result
        }
        $sum = 0.0; $count = 0; $over = 0; $blocks = 0
        for ($by = 0; $by -lt $ia.Height; $by += $BLOCK) {
            for ($bx = 0; $bx -lt $ia.Width; $bx += $BLOCK) {
                $bsum = 0.0; $bn = 0
                for ($y = $by; $y -lt [Math]::Min($by + $BLOCK, $ia.Height); $y += 2) {
                    for ($x = $bx; $x -lt [Math]::Min($bx + $BLOCK, $ia.Width); $x += 2) {
                        $ca = $ia.GetPixel($x, $y); $cb = $ib.GetPixel($x, $y)
                        $d = [Math]::Abs($ca.R - $cb.R) + [Math]::Abs($ca.G - $cb.G) +
                             [Math]::Abs($ca.B - $cb.B)
                        $bsum += $d / 3.0; $bn++
                    }
                }
                $bmean = if ($bn -gt 0) { $bsum / $bn } else { 0.0 }
                $sum += $bsum; $count += $bn; $blocks++
                if ($bmean -gt $BLOCK_TOL) { $over++ }
            }
        }
        $result.mean = if ($count -gt 0) { $sum / $count } else { 0.0 }
        $result.over = $over
        $result.blocks = $blocks
    } finally {
        $ia.Dispose(); $ib.Dispose()
    }
    return $result
}

if ($Check -or $Bless) { New-Item -ItemType Directory -Force -Path $golden | Out-Null }

if (-not (Test-Path $Exe)) { throw "Opra.exe not found at $Exe - build it first." }
New-Item -ItemType Directory -Force -Path $Out | Out-Null

$views = @(
    @{ Name = "startup";  Args = @("--view", "startup", "--seconds", "0") },
    @{ Name = "flight";   Args = @("--view", "flight") },
    @{ Name = "flight2";  Args = @("--view", "flight", "--density", "2") },
    @{ Name = "flight3";  Args = @("--view", "flight", "--density", "3") },
    @{ Name = "map";      Args = @("--view", "map", "--seconds", "2") },
    @{ Name = "chart";    Args = @("--view", "chart") },
    @{ Name = "help";     Args = @("--view", "help") },
    @{ Name = "models";   Args = @("--view", "models", "--seconds", "0") },
    @{ Name = "cutter";   Args = @("--view", "cutter") },
    @{ Name = "scene";    Args = @("--view", "none") },
    @{ Name = "pause";    Args = @("--view", "pause", "--seconds", "1") },
    @{ Name = "settings"; Args = @("--view", "settings", "--seconds", "1") }
)

foreach ($view in $views) {
    $bmp = Join-Path $Out "$($view.Name).bmp"
    $png = Join-Path $Out "$($view.Name).png"
    $arguments = @("--screenshot", $bmp, "--hidden") + $view.Args
    if ($view.Args -notcontains "--seconds") { $arguments += @("--seconds", $Seconds) }

    & $Exe @arguments | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "$($view.Name): Opra exited $LASTEXITCODE" }

    $image = [System.Drawing.Image]::FromFile($bmp)
    $image.Save($png, [System.Drawing.Imaging.ImageFormat]::Png)
    $image.Dispose()
    Remove-Item $bmp

    if ($Bless) {
        Copy-Item $png (Join-Path $golden "$($view.Name).png") -Force
        Write-Host ("{0,-9} -> {1}  (blessed)" -f $view.Name, (Split-Path $png -Leaf))
    } elseif ($Check) {
        $reference = Join-Path $golden "$($view.Name).png"
        if (-not (Test-Path $reference)) { throw "$($view.Name): no reference; run tools/shots.ps1 -Bless" }
        $diff = Compare-Frame $png $reference
        if ($diff.size) { throw "$($view.Name): reference is a different size" }
        $fraction = if ($diff.blocks -gt 0) { $diff.over / $diff.blocks } else { 0.0 }
        if ($fraction -gt $MAX_OVER_FRACTION -or $diff.mean -gt $MAX_MEAN) {
            throw ("{0}: FAILED - mean {1:N2}, {2} of {3} blocks over tolerance ({4:P2})" -f `
                   $view.Name, $diff.mean, $diff.over, $diff.blocks, $fraction)
        }
        Write-Host ("{0,-9} -> {1}  (ok, mean {2:N2})" -f $view.Name, (Split-Path $png -Leaf), $diff.mean)
    } else {
        Write-Host ("{0,-9} -> {1}" -f $view.Name, (Split-Path $png -Leaf))
    }
}
