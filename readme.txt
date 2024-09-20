rime.toy 
  一个在Windows 下免安装试玩Rime的输入法玩具（！！！注意是玩具！！！）

使用方法：
  运行rime.toy.exe，在窗口内打字可以在控制台中看到候选情况，像正常输入法一样选择候选，插入输入的应用中
  共享数据目录在当前目录的shared\下，用户目录在当前目录的usr\下，日志目录在%TEMP%\rime.toy\下
  方案修改的项目，和其他rime引擎的输入法类似（建议usr下修改xxx.custom.yaml文件）

相关项目：
  方案文件和opencc文件，取自 rime/weasel  https://github.com/rime/weasel
  后端引擎，采用 rime/librime  https://github.com/rime/librime


已知问题：
  1. CapsLock指示灯潜在可能不太准确（尤其是如果您使用了类似小狼毫里那样用CapsLock来选词的情况）
  2. 建议在英文键盘下使用，避免潜在的可能和当前的tsf输入法冲突
  3. 管理员权限下的使用，要使用管理员权限启动rime.toy.exe
  4. 使用快捷键（如Shift_L）切换中英文的时候，未能清空当前的composition
  5. 没有复杂的UI样式，全是控制台（短期内无计划做ui）
  6. 不是所有应用中都可以输入文字
