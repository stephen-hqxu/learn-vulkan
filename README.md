# Learn Vulkan

This is a repository holding source code of a sample application when I was learning Vulkan, a low-level platform-independent graphics and compute API according to [The Khronos Group](https://khronos.org/registry/vulkan).

## Motivation

If you are not yet familiar with Vulkan (VK), you might have heard of some similar APIs such as OpenGL (GL) and DirectX (DX). GL is believed to be the predecessor of VK and has been widely used to develop GUI on desktop and mobile devices; while DX is more well-known in video game industry, although it's limited to Windows only.

I don't have any experience with DX as I don't really like to bound myself to a single platform. I have been using GL to develop graphics applications since 2015, renderers and mini-game engines in particular. At start, GL seemed to be acceptable to me for implementing real-time graphics applications; it is reasonably low-level to provide sufficient flexibility and performance. Nevertheless, hardware industry leaped significantly in recent years, and our computers get increasing more powerful. I start to hit the ceiling of GL and feel its incapability of harnessing the power of modern computers.

GL has many fundamentally unintuitive designs that makes it difficult to use in large projects; for example, the infamous global state may bring surprising side effects if developer is not being cautious. Although modern GL introduces many new extensions to alleviate, it does not completely resolve these old problems.

**Do I need to learn Vulkan?**

Absolutely no need if you don't find it necessary. I decided to abandon GL completely and move on to use VK in my future projects, but that is due to the fact that I am working with large projects requiring aggressive computational power, which already hits the limits of GL. Even many criticisms have been said, GL still has an important place in general, non-intense graphical applications.

**Is Vulkan difficult for beginners?**

VK API is very intimidating at first glance as it is extremely verbose and frustrating. But once you understand the logical flow of a VK application, it can be realised that VK API is actually very sensibly designed, and getting used to its API is only a matter of time. For me, it takes about a few weeks to get used to it before I felt comfortable enough to progress further on my own without tutorial.

The [Vulkan Specification](https://registry.khronos.org/vulkan/) is a vital resource for all VK developers, and I strongly recommend having it opened when writing VK application.

The API aside, most knowledge for graphics programming is applicable to VK as well. You are required to have a strong mathematical (especially linear algebra) and programming foundation. You may want to study GL first before getting into VK if you are not yet experienced.

## Sample Overview

The application contains a number of samples, each of them explores different aspects and rendering techniques in Vulkan. The overall logic and syntax of VK GLSL is pretty similar to GL GLSL, only differing in a few Vulkan-exclusive features, such as descriptor set decoration. Therefore I did not dive deep into writing shader code (texture, environment mapping, lighting, shadows, etc.) because I am mostly trying to familiarise myself with the Vulkan API.

:warning: Even though attempts are made to ensure a reasonable code readability and performance, please be alerted that the code is still boilerplate. It is only intended for education purposes and should **NOT** be used as a best-practice guideline in production-ready application.

### DrawTrangle

**Keywords** Vulkan basics, mip-map generation, descriptor buffer, instancing, dynamic rendering, push constant

This is my first Vulkan application made by following the [Vulkan Tutorial](https://vulkan-tutorial.com/) by *Alexander Overvoorde*, which is an invaluable beginner guide if you are embarking on the daunting Vulkan journey as well. All I did was following logic in the tutorial while producing code on my own, so implementation will be vastly different from that in the tutorial code repo.

The sample draws multiple instances of quads lined up as a spiral staircase.

![draw-triangle](https://user-images.githubusercontent.com/77457215/265590962-018e0a69-9f53-44bc-9380-f00b5eabdc08.png)

### SimpleTerrain

**Keywords** shader buffer reference, tessellation, compute shader

This sample aims to test how Vulkan performs when comes to terrain rendering, which is one of my favourite topic. The terrain heightmap, normalmap images and some shader code are generated and copy-pasted from one of [my procedural generation project](https://github.com/stephen-hqxu/superterrainplus).

![simple-terrain](https://user-images.githubusercontent.com/77457215/265590996-75e4de5d-bbf5-4687-b6e4-2ec42c632a5f.png)

### SimpleWater

**Keywords** acceleration structure build, acceleration structure compaction, push descriptor, ray query

I have had a very hard time getting photorealistic water dynamically rendered in real-time previously. Classic algorithms such as planar or screen-space reflection are very limited on producing a convincing output. This sample demonstrates use of one of the most exciting features in Vulkan, which brings computer graphics to the next-level, and facilitate rendering of advanced effects.

Ray query is used to find the closest hit positions of reflection and refraction ray in fragment shader, and use them to find reflection and refraction colours for water surface shading. The sample might be inefficient due to large section of divergence (ray tracing pipeline should be better-suited), but it works good enough for now.

![simple-water](https://user-images.githubusercontent.com/77457215/265591021-5662b853-f02e-4c39-a149-62386d958bff.png)

## Build Instruction

It is required to build the application with a compliant compiler with full support to **ISO C++20** language standard.

The application is built with CMake; the minimum CMake version is 3.21 (mainly due to use of `findVulkan` and its imported targets). The build step is similar to most CMake projects; please make sure you have all required dependencies installed to CMake library search path, see below for more details.

### External Dependencies

The following libraries are included in the repo, and you do not need to do anything:

- stb_image

It is only required to have the following libraries installed to CMake path:

- GLFW
- GLM

The application is developed based on **Vulkan 1.3**, therefore a valid Vulkan SDK is required. The following optional components are included with SDK, and you should have them installed.

- Volk
- Vulkan Memory Allocator

### Run

Please run the compiled executable from command prompt and follow the console output, it requires certain command line arguments to execute.