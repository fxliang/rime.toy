param (
    [switch]$n,
    [switch]$i
)

$TOY_SOURCE_PATH = @("src", "include", "WeaselUI")
$excludePatterns = Get-Content .exclude_pattern.txt

function ShouldExclude($filePath) {
  foreach ($pattern in $excludePatterns) {
    if ($filePath -like "*$pattern*") {
      return $true
    }
  }
  return $false
}

$filesToProcess = @()

$TOY_SOURCE_PATH | ForEach-Object {
  $filesToProcess += Get-ChildItem -Path $_ -Recurse -Include *.cpp, *.h | `
  Where-Object { -not (ShouldExclude $_.FullName) } | `
  ForEach-Object { $_.FullName }
}
$filesToProcess = $filesToProcess | Select-Object -Unique

if ($filesToProcess.Count -gt 0) {
  if ($n) {
    clang-format --verbose -i $filesToProcess
    Write-Host "Formatting done!"
  } elseif ($i) {
    clang-format --verbose -Werror --dry-run $filesToProcess
    if ($LASTEXITCODE -ne 0) {
      Write-Host "Please lint your code by './clang-format.ps1 -n'."
      exit 1
    }
    Write-Host "Format checking pass!"
  }
} else {
  Write-Host "No files to process."
}
