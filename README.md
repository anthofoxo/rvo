# RVO Engine
RVO Engine is in an early state and in rapid development. This will change and break!

RVO Requires OpenGL 4.6 to run along with `ARB_shading_language_include`.

## Goals
The goals of RVO are pretty simple.
1. Full control over graphics rendering APIs.
1. Good looking graphics at minimal cost to performance.
1. Custom and flexible engine.

## Building
### Prerequisites
1. A build toolchain. Typically `Visual Studio` or `g++` and `make`.
RVO includes all required dependencies and build files in the repo.
2. [premake5](https://premake.github.io/). This is our project generator. Download and install this.

## Initial Setup
1. Clone the repository. `git clone --recurse -j12 <url>`
2. Build the project files. `premake5 vs2022` or `premake5 gmake`
3. You're done. The project is configured and ready to run.

## Third Party Dependencies
### Submodules
* [GLFW 3.4](https://github.com/glfw/glfw/tree/3.4)
* [Dear ImGui v1.92.2-docking](https://github.com/ocornut/imgui/tree/v1.92.2-docking)
* [glm 1.0.1](https://github.com/g-truc/glm/tree/1.0.1)
* [spdlog v1.15.3](https://github.com/gabime/spdlog/tree/v1.15.3)
* [EnTT v3.15.0](https://github.com/skypjack/entt/tree/v3.15.0)
### Source Tree
* [glad2](https://gen.glad.sh/)
* * Generated with 4.6 Core with ARB_shader_language_include
* [happly](https://github.com/nmwsharp/happly)
* [miniaudio](https://miniaud.io/)
* [stb_image](https://github.com/nothings/stb/blob/master/stb_image.h)
* [stb_vorbis](https://github.com/nothings/stb/blob/master/stb_vorbis.c)
* [RenderDoc](https://renderdoc.org/)