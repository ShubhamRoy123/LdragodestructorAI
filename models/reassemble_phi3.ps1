$ErrorActionPreference = "Stop"

$modelDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$outputPath = Join-Path $modelDir "phi3.gguf"
$expectedSha256 = "8A83C7FB9049A9B2E92266FA7AD04933BB53AA1E85136B7B30F1B8000FF2EDEF"

if (Test-Path -LiteralPath $outputPath) {
    throw "Refusing to overwrite existing model: $outputPath"
}

$parts = Get-ChildItem -LiteralPath $modelDir -Filter "phi3.gguf.part*" | Sort-Object Name
if ($parts.Count -eq 0) {
    throw "No phi3.gguf.part* files found in $modelDir"
}

$output = [System.IO.File]::Create($outputPath)
try {
    foreach ($part in $parts) {
        $input = [System.IO.File]::OpenRead($part.FullName)
        try {
            $input.CopyTo($output)
        } finally {
            $input.Dispose()
        }
    }
} finally {
    $output.Dispose()
}

$actualSha256 = (Get-FileHash -LiteralPath $outputPath -Algorithm SHA256).Hash
if ($actualSha256 -ne $expectedSha256) {
    throw "Reassembled model hash mismatch. Expected $expectedSha256 but got $actualSha256"
}

Write-Host "Reassembled $outputPath"
