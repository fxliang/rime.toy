target("WeaselUI")
  set_kind("static")
  add_links("user32", "Shlwapi", "shcore", "gdi32", "Shell32", "d2d1", "dwrite", 'dxgi', 'd3d11', 'dcomp')
  add_files("./*.cpp")
  if is_plat('windows') then
    add_cxflags("/utf-8")
    if is_mode("debug") then
      add_cxflags("/Zi")
      add_ldflags("/DEBUG", {force = true})
    end
  elseif is_plat('mingw') then
    add_ldflags('-static-libgcc -static-libstdc++ -static', {force=true})
  end

