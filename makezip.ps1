param([string]$zipFileName = "rime.toy.zip")
# 定义要打包的文件和目录
$itemsToArchive = @("shared", "rime.toy.exe", "rime.dll", "rime.toy.json", "restart_rime.toy.bat", "LICENSE.txt", "README.md")
$pdbFiles = @("rime.toy.pdb", "rime.pdb")
# 删除已存在的压缩包（如果存在）
if (Test-Path $zipFileName) {
  Remove-Item $zipFileName
}
# 打包文件和目录
foreach ($item in $itemsToArchive) {
  if (Test-Path $item) {
    Compress-Archive -Path $item -Update -DestinationPath $zipFileName
  } else {
    Write-Host "Warning: $item does not exist and will not be included in the archive."
  }
}
$pdbZipFileName = [System.IO.Path]::ChangeExtension($zipFileName, ".pdbs.zip")
foreach ($pdbFile in $pdbFiles) {
  if (Test-Path $pdbFile) {
    Compress-Archive -Path $pdbFile -Update -DestinationPath $pdbZipFileName
  } else {
    Write-Host "Warning: $pdbFile does not exist and will not be included in the PDB archive."
  }
}
Write-Host "Files and directories have been archived into $zipFileName"
