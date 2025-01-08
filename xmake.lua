local project_name = "rime.toy"

add_includedirs("./include")
add_defines("UNICODE", "_UNICODE", "_WIN32_WINNT=0x0603", "TOY_FEATURE")
includes("WeaselUI")

target(project_name)
  set_kind(binary)
  set_languages("c++17")
  add_files("src/*.cpp", "src/*.rc")
  add_includedirs("./include")
  add_links("user32", "Shlwapi", "shcore", "rime", "gdi32", "Shell32", "d2d1",
  "dwrite", 'dxgi', 'd3d11', 'dcomp', "oleaut32", "uiautomationcore", "ole32",
  "oleacc", "imm32", "advapi32")
  add_deps('WeaselUI')
  if is_plat('windows') then
    add_cxflags("/utf-8")
    add_cxflags("/Zi")
    add_cxflags("-Fd$(buildir)/$(targetname).pdb")
    add_cxflags("/FS")
    add_ldflags("/DEBUG", {force = true})
    add_ldflags("/SUBSYSTEM:WINDOWS")
  elseif is_plat('mingw') then
    add_ldflags('-static-libgcc -static-libstdc++ -static', {force=true})
    add_ldflags("-municode -mwindows", {force = true})
  end

  add_linkdirs(is_arch("x86") and "lib" or "lib64")

  -- copy build output to $(projectdir)
  after_build(function(target)
    local prjoutput = path.join(target:targetdir(), target:filename())
    print("copy " .. prjoutput .. " to $(projectdir)")
    os.trycp(prjoutput, "$(projectdir)")
    if is_plat('windows') then
      os.trycp(path.join(target:targetdir(), target:name() .. ".pdb"), "$(projectdir)")
    end
    local rimelib=(is_arch('x86') and path.join("$(projectdir)", "lib/rime.dll")
      or path.join("$(projectdir)", "lib64/rime.dll"))
    print("copy " .. rimelib .. " to $(projectdir)")
    os.trycp(rimelib, "$(projectdir)")
  end)

  local version_major = "0"
  local version_minor = "0"
  local version_patch = "5"
  local version = "\"" .. version_major .. "." .. version_minor .. "." .. version_patch .. "\""
  add_defines("VERSION_INFO=="..version)
  -- generate src/rime.toy.rc before build if needed
  on_load(function (target)
    import("core.base.text")
    local rc_template = path.join(os.projectdir(), "src/rime.toy.rc.in")
    local rc_output = path.join(os.projectdir(), "src/rime.toy.rc")
    local function generate_rc()
      local content = io.readfile(rc_template)
      content = content:gsub("${VERSION_MAJOR}", version_major)
      content = content:gsub("${VERSION_MINOR}", version_minor)
      content = content:gsub("${VERSION_PATCH}", version_patch)
      local commit_id = os.iorun("git rev-parse --short HEAD"):gsub("\n", "")
      content = content:gsub("${TAG_SUFFIX}", commit_id)
      io.writefile(rc_output, content)
    end
    local function check_version()
      local rc = io.readfile(rc_output)
      local commit_id = os.iorun("git rev-parse --short HEAD"):gsub("\n", "")
      local version_str = version_major..'.'..version_minor..'.'..version_patch..'.'..commit_id
      return string.find(rc, version_str)
    end
    -- no rc file, or version info not match
    if not os.isfile(rc_output) or not check_version() then
      print("generate new rc file: " .. rc_output)
      generate_rc()
    end
  end)
