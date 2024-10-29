local project_name = "rime.toy"

target(project_name)
  set_kind(binary)
  set_languages("c++17")
  add_files("src/*.cpp", "src/*.rc")
  add_includedirs("./include")
  add_links("user32", "Shlwapi", "imm32", "rime", "gdi32", "Shell32", "d2d1", "dwrite")
  if is_plat('windows') then
    add_cxflags("/utf-8")
    if is_mode("debug") then
      add_cxflags("/Zi")
      add_ldflags("/DEBUG", {force = true})
    end
  elseif is_plat('mingw') then
    add_ldflags('-static-libgcc -static-libstdc++ -static', {force=true})
  end

  add_defines("UNICODE", "_UNICODE")
  if is_plat('mingw') then
    add_ldflags("-municode -mwindows", {force = true})
  else
    add_ldflags("/SUBSYSTEM:WINDOWS")
  end

  if is_arch("x64") or is_arch("x86_64") then
    add_linkdirs("lib64")
  elseif is_arch("x86") then
    add_linkdirs("lib")
  end

  after_build(function(target)
    os.cp(path.join(target:targetdir(), project_name .. ".exe"), "$(projectdir)")
    if is_mode("debug") then
      os.cp(path.join(target:targetdir(), project_name .. ".pdb"), "$(projectdir)")
    end
    if is_arch("x86") then
      os.cp("$(projectdir)/lib/rime.dll", "$(projectdir)")
    else
      os.cp("$(projectdir)/lib64/rime.dll", "$(projectdir)")
    end
  end)
