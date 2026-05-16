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

project("MyCoolVM")
   kind("ConsoleApp")
   language("C")
   cdialect("C23")

   targetdir("bin/%{cfg.buildcfg}")

   postbuildcommands {
      "{COPYDIR} %{wks.location}/res %{cfg.targetdir}"
   }

   toolset("clang")
   linker("LLD")

   includedirs { "src/**" }
   files { "src/**.h", "src/**.c" }

   buildoptions {
      "-Wall",
      "-Wextra",
      "-Wpedantic",
   }

   filter "system:windows"
      defines { "_CRT_SECURE_NO_WARNINGS" }

   filter "system:linux"
      links { "m" }

   filter "configurations:Debug"
      defines { "DEBUG" }
      symbols "On"

   filter "configurations:Release"
      defines { "NDEBUG" }
      buildoptions { "-Werror" }

      optimize "On"
