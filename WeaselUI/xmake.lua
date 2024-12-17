target("WeaselUI")
  set_kind("static")
  add_links("user32", "Shlwapi", "dwmapi","shcore", "gdi32", "Shell32", "d2d1", "dwrite", 'dxgi', 'd3d11', 'dcomp')
  add_links('windowscodecs', 'ole32')
  set_languages("c++17")
  add_files("./*.cpp")
  add_defines('INITGUID')
  if is_plat('windows') then
    add_cxflags("/utf-8")
    if is_mode("debug") then
      add_cxflags("/Zi")
      add_cxflags("-Fd$(buildir)/$(targetname).pdb")
      add_cxflags("/FS")
      add_ldflags("/DEBUG", {force = true})
    end
  elseif is_plat('mingw') then
    add_ldflags('-static-libgcc -static-libstdc++ -static', {force=true})
  end

