#MaxHotkeysPerInterval 50

^RButton::  ; Ctrl+鼠标右键
    MouseGetPos, , , WinUnderMouse  ; 获取鼠标下的窗口句柄
    WinGet, ExStyle, ExStyle, ahk_id %WinUnderMouse%  ; 获取窗口的扩展样式
    if (ExStyle & 0x8)  ; 检查WS_EX_TOPMOST位是否设置
    {
        ; 如果已经是置顶窗口，则取消置顶
        WinSet, AlwaysOnTop, Off, ahk_id %WinUnderMouse%
    }
    else
    {
        ; 否则，设置窗口为置顶
        WinSet, AlwaysOnTop, On, ahk_id %WinUnderMouse%
    }
return

; Ctrl+鼠标滚轮向上增加透明度
^WheelUp::
    WinGet, active_id, ID, A  ; 获取当前活动窗口的ID
    WinGet, currentTransparency, Transparent, ahk_id %active_id%  ; 获取当前透明度
    if (currentTransparency == "")  ; 如果窗口当前不透明
        currentTransparency := 255
    newTransparency := currentTransparency + 25  ; 增加透明度
    if (newTransparency > 255)
        newTransparency := 255
    WinSet, Transparent, %newTransparency%, ahk_id %active_id%  ; 设置新的透明度
return

; Ctrl+鼠标滚轮向下减少透明度
^WheelDown::
    WinGet, active_id, ID, A  ; 获取当前活动窗口的ID
    WinGet, currentTransparency, Transparent, ahk_id %active_id%  ; 获取当前透明度
    if (currentTransparency == "")  ; 如果窗口当前不透明
        currentTransparency := 255
    newTransparency := currentTransparency - 25  ; 减少透明度
    if (newTransparency < 20)
        newTransparency := 20
    WinSet, Transparent, %newTransparency%, ahk_id %active_id%  ; 设置新的透明度
return

ToggleTransparent() {
    WinGet, ExStyle, ExStyle, A  ; 获取当前活动窗口的扩展样式
    if (ExStyle & 0x20)  ; 检查WS_EX_TRANSPARENT标志
    {
        ; 如果已经设置，取消点击穿透
        WinSet, ExStyle, -0x20, A
    }
    else
    {
        ; 否则，启用点击穿透
        WinSet, ExStyle, +0x20, A
    }
}
!RButton::  ; Alt+鼠标右键
ToggleTransparent()
return

!0::
ToggleTransparent()
return

!t::  ; 当按下Alt+T时
    WinGet, Style, Style, A  ; 获取当前活动窗口的样式
    WS_CAPTION := 0xC00000  ; WS_CAPTION样式的值
    if (Style & WS_CAPTION)
    {
        ; 如果窗口有标题栏，移除标题栏
        NewStyle := Style & ~WS_CAPTION
        WinSet, Style, %NewStyle%, A
    }
    else
    {
        ; 如果窗口没有标题栏，添加标题栏
        NewStyle := Style | WS_CAPTION
        WinSet, Style, %NewStyle%, A
    }
return
