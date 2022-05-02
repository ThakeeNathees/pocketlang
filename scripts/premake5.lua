--
-- Copyright (c) 2020-2022 Thakee Nathees
-- Copyright (c) 2021-2022 Pocketlang Contributors
-- Distributed Under The MIT License
--
 
-- Requires premake5.0.0-alpha-12 or greater.

local build_dir = "../build/"
local src_dir = "../src/"
local cli_dir = "../cli/"

workspace "pocketlang"
  location (build_dir)
  configurations { "Debug", "Release" }
  platforms { "x86", "x64" }
  filter "platforms:x86"
    architecture "x86"
  filter "platforms:x64"
    architecture "x64"

-- Reusable project configurations.
function proj_commons(target_dir)
  language "C"
  cdialect "C99"    
  objdir (build_dir .. "%{cfg.buildcfg}/obj/")
  targetdir (build_dir .. "%{cfg.buildcfg}/" .. target_dir)
  
  filter "configurations:Debug"
    defines { "DEBUG" }
    symbols "On"
  filter "configurations:Release"
    defines { "NDEBUG" }
    optimize "On"    
  
  filter "system:Windows"
    staticruntime "On"
    systemversion "latest"
    postbuildcommands ""
    defines { "_CRT_SECURE_NO_WARNINGS" }
    
  filter {}
end

-- pocketlang sources as static library.
project "pocket_static"
  kind "StaticLib"
  targetname "pocket"
  proj_commons("lib/")
  files { src_dir .. "*.c",
          src_dir .. "**.h"}

-- pocketlang sources as shared library.
project "pocket_shared"
  kind "SharedLib"
  targetname "pocket"
  proj_commons("bin/")
  files { src_dir .. "*.c",
          src_dir .. "**.h"}
  defines { "PK_DLL", "PK_COMPILE" }

-- command line executable.
project "pocket_cli"
  kind "ConsoleApp"
  targetname "pocket"
  proj_commons("bin/")
  files { cli_dir .. "*.c",
          cli_dir .. "**.h" }
  includedirs ({ src_dir .. "include/" })
  links { "pocket_static" }
