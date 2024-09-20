local project_name = "rime.toy"
target(project_name)
  set_kind(binary)
  add_files("src/*.cpp")
  add_includedirs("./include")
  add_links("user32", "Shlwapi", "imm32", "rime")
  add_cxflags("/utf-8")
	if is_mode("debug") then
		add_cxflags("/Zi")
		add_ldflags("/DEBUG", {force = true})
	end
  add_defines("UNICODE")
	set_languages("c++17")
  if is_arch("x64") then
    add_linkdirs("lib64")
  elseif is_arch("x86") then
    add_linkdirs("lib")
  end

  after_build(function(target)
		os.mv(path.join(target:targetdir(), project_name .. ".exe"), "$(projectdir)")
		if is_mode("debug") then
			os.mv(path.join(target:targetdir(), project_name .. ".pdb"), "$(projectdir)")
		end
    if is_arch("x86") then
      os.cp("$(projectdir)/lib/rime.dll", "$(projectdir)")
    else
      os.cp("$(projectdir)/lib64/rime.dll", "$(projectdir)")
    end
  end)
