workspace "game"

architecture "x86_64"
configurations { "debug", "release" }
startproject "game"

flags "MultiProcessorCompile"
language "C++"
cppdialect "C++latest"
cdialect "C17"
staticruntime "On"

kind "StaticLib"
targetdir "%{wks.location}/bin/%{cfg.system}_%{cfg.buildcfg}"
objdir "%{wks.location}/bin_int/%{cfg.system}_%{cfg.buildcfg}"

stringpooling "On"

filter "configurations:debug"
defines "_DEBUG"
runtime "Debug"
optimize "Debug"
symbols "On"

filter "configurations:release"
defines "NDEBUG"
runtime "Release"
optimize "Speed"
symbols "On"
linktimeoptimization "On"

filter "system:windows"
systemversion "latest"
defines "NOMINMAX"
buildoptions { "/EHsc", "/Zc:preprocessor", "/Zc:__cplusplus", "/experimental:c11atomics", "/permissive-", "/utf-8" }

group "dependencies"
for _, matchedfile in ipairs(os.matchfiles("premake/*.lua")) do
    include(matchedfile)
end
group ""

project "game"
location "source"
kind "ConsoleApp"

files {
    "%{prj.location}/**.cpp",
    "%{prj.location}/**.hpp",
    "%{prj.location}/**.c",
    "%{prj.location}/**.h",
}

includedirs {
    "%{wks.location}/vendor/glfw/include",
    "%{wks.location}/vendor/glad/include",
    "%{wks.location}/vendor/imgui",
    "%{wks.location}/vendor/glm",
    "%{wks.location}/vendor/spdlog/include",
    "%{wks.location}/vendor/entt/src",
    "%{wks.location}/vendor/imguizmo",
    "%{wks.location}/vendor/lua/src",
}

defines "GLFW_INCLUDE_NONE"
debugdir "%{wks.location}/working"

links { "glfw", "glad", "imgui", "imguizmo", "lua" }