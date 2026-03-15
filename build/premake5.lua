workspace "MyCoolProject"
   configurations { "Debug", "Release" }
   platforms { "x64" }

project("MyCoolVM")
   kind("ConsoleApp")
   language("C")

   targetdir("../bin/%{cfg.buildcfg}")

   toolset("clang")
   linker("LLD")

   includedirs { "../src/**" }
   files { "../src/**.h", "../src/**.c" }

   buildoptions {
      "-Wall",
      "-Wextra",
      "-Wpedantic",
   }

   -- filter "toolset:msc*"
   --    buildoptions "/std=c++17"
   -- filter "toolset:gcc* or clang*"
   --    buildoptions "--std=c++17"
   -- filter {}

   filter "system:windows"
      defines { "_CRT_SECURE_NO_WARNINGS" }

   filter "system:linux"
      -- linkoptions { "`wx-config --libs`" }
      links { "m" }

   filter "configurations:Debug"
      defines { "DEBUG" }
      symbols "On"

   filter "configurations:Release"
      defines { "NDEBUG" }
      buildoptions { "-Werror" }

      optimize "On"
