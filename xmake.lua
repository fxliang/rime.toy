local project_name = "rime.toy"

local version_major = "0"
local version_minor = "0"
local version_patch = "3"
local version = "\"" .. version_major .. "." .. version_minor .. "." .. version_patch .. "\""
add_defines("VERSION_INFO=="..version)

add_includedirs("./include")
add_defines("UNICODE", "_UNICODE", "_WIN32_WINNT=0x0603")
includes("WeaselUI")

target(project_name)
  set_kind(binary)
  set_languages("c++17")
  add_files("src/*.cpp", "src/*.rc")
  add_includedirs("./include")
  add_links("user32", "Shlwapi", "shcore", "rime", "gdi32", "Shell32", "d2d1", "dwrite", 'dxgi', 'd3d11', 'dcomp', "oleaut32", "uiautomationcore", "ole32", "oleacc")
  add_deps('WeaselUI')
  if is_plat('windows') then
    add_cxflags("/utf-8")
    if is_mode("debug") then
      add_cxflags("/Zi")
      add_ldflags("/DEBUG", {force = true})
    end
    add_ldflags("/SUBSYSTEM:WINDOWS")
  elseif is_plat('mingw') then
    add_ldflags('-static-libgcc -static-libstdc++ -static', {force=true})
    add_ldflags("-municode -mwindows", {force = true})
  end

  if is_arch("x64") or is_arch("x86_64") then
    add_linkdirs("lib64")
  elseif is_arch("x86") then
    add_linkdirs("lib")
  end
  -- copy build output to $(projectdir)
  after_build(function(target)
    os.cp(path.join(target:targetdir(), project_name .. ".exe"), "$(projectdir)")
    if is_mode("debug") and is_plat('windows') then
      os.cp(path.join(target:targetdir(), project_name .. ".pdb"), "$(projectdir)")
    end
    if is_arch("x86") then
      os.cp("$(projectdir)/lib/rime.dll", "$(projectdir)")
    else
      os.cp("$(projectdir)/lib64/rime.dll", "$(projectdir)")
    end
  end)
  -- generate src/app.rc before build
  before_build(function (target)
    import("core.base.text")
    local rc_template = path.join(os.projectdir(), "src/app.rc.in")
    local rc_output = path.join(os.projectdir(), "src/app.rc")
    local content = io.readfile(rc_template)
    content = content:gsub("${VERSION_MAJOR}", version_major)
    content = content:gsub("${VERSION_MINOR}", version_minor)
    content = content:gsub("${VERSION_PATCH}", version_patch)
    io.writefile(rc_output, content)
    target:add("files", rc_output)
  end)
