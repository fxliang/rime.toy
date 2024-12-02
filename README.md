# rime.toy 
  一个在Windows 下免安装试玩Rime的输入法玩具

使用纯direct3d/direct2d/directwrite实现的UI界面绘制,以实现更好的图形绘制性能, 无WTL依赖，mingw或msvc都可构建通过

![rime.toy](./intro.gif)

## 使用方法：
  运行rime.toy.exe，在鼠标指针位置显示候选框，像正常输入法一样选择候选，插入输入的应用中
  共享数据目录在当前目录的`shared\`下，用户目录在当前目录的`usr\`下，日志目录在当前目录`log\`下
  方案修改的项目，和其他rime引擎的输入法类似（建议usr下修改`xxx.custom.yaml`文件）

## 相关项目：
  样式文件，方案文件和opencc文件，取自 [rime/weasel](https://github.com/rime/weasel)

  后端引擎，采用 [rime/librime](https://github.com/rime/librime)


## 已知问题：
  1. 建议在英文键盘下使用，避免潜在的可能和当前的tsf输入法冲突(微软输入法可正常)
  2. 管理员权限下的使用，可能要使用管理员权限启动rime.toy.exe
  3. 只有简单的UI界面，窗口鼠标指针跟随，小狼毫的部分界面特性未移植实现
  4. 可能不是所有应用中都可以输入文字

## TODO
  - [ ] 补写UI部分，实现目前小狼毫UI的全部功能
  - [ ] 暂无(未想到)
