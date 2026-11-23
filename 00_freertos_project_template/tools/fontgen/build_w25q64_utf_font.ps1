$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $PSScriptRoot
$RepoRoot = Split-Path -Parent $Root

$SubsetFile = Join-Path $PSScriptRoot "subset_chars.txt"
$DumpDir = Join-Path $PSScriptRoot "sample_out_noto_full"
$FontInfo = Join-Path $DumpDir "font_info.json"
$OutBin = Join-Path $PSScriptRoot "utf_font_w25q64.bin"

$FontPath = "C:\Windows\Fonts\NotoSansSC-VF.ttf"
$Symbols = (Get-Content $SubsetFile -Raw -Encoding UTF8).Replace("`r", "").Replace("`n", "")

if (-not (Test-Path $FontPath)) {
    throw "Font not found: $FontPath"
}

New-Item -ItemType Directory -Force -Path $DumpDir | Out-Null

npx lv_font_conv `
  --font $FontPath `
  --symbols $Symbols `
  --size 16 `
  --bpp 1 `
  --format dump `
  --full-info `
  -o $DumpDir

python (Join-Path $PSScriptRoot "generate_utf_font.py") `
  --input $FontInfo `
  --output $OutBin

Write-Output "Generated external UTF font binary: $OutBin"
Write-Output "Flash target address in W25Q64: 0x00C00000 (12MB)"
