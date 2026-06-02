newaction {
   trigger = "clean",
   description = "Remove all generated and build files",
   execute = function()
      print("Cleaning...")
      os.rmdir("./bin")
      os.rmdir("./obj")
      os.remove("Makefile")
      os.remove("*.make")
      print("Done.")
   end
}

workspace "MyCoolProject"
   configurations { "Debug", "Release" }
   platforms { "x64" }

   language("C")
   cdialect("C23")
   toolset("clang")
   linker("LLD")
   includedirs { "src/**" }

   buildoptions {
      "-Wall",
      "-Wextra",
      "-Wpedantic",
   }

   filter "system:windows"
      defines { "_CRT_SECURE_NO_WARNINGS" }

   filter "configurations:Debug"
      defines { "DEBUG" }
      symbols "On"

   filter "configurations:Release"
      defines { "NDEBUG" }
      buildoptions { "-Werror" }
      optimize "On"

project "mclex"
   kind "StaticLib"
   targetdir("lib/%{cfg.buildcfg}")

   files { "src/lex/**.h", "src/lex/**.c" }

project "mcparse"
   kind "StaticLib"
   targetdir("lib/%{cfg.buildcfg}")

   files { "src/parse/**.h", "src/parse/**.c" }

project "MyCoolVM"
   kind "ConsoleApp"
   targetdir("bin/%{cfg.buildcfg}")

   postbuildcommands {
      "{COPYDIR} %{wks.location}/res %{cfg.targetdir}"
   }

   files {
      "src/main.c",
      "src/vm/**.h",    "src/vm/**.c",
      "src/mcf/**.h",   "src/mcf/**.c",
      "src/lib/**.h",   "src/lib/**.c",
      "src/heap/**.h",  "src/heap/**.c",
      "src/strtbl/**.h","src/strtbl/**.c",
   }

   libdirs { "lib/%{cfg.buildcfg}" }
   links { "mclex", "mcparse" }

   filter "system:linux"
      links { "m" }
