$zipFileName = "rime.toy.zip"
# 定义要打包的文件和目录
$itemsToArchive = @("shared", "rime.toy.exe", "rime.dll", "LICENSE.txt", "readme.txt")
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
Write-Host "Files and directories have been archived into $zipFileName"
