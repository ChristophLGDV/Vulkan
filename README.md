# C++ Vulkan examples and demos

<img src="./documentation/images/vulkanlogoscene.png" alt="Vulkan demo scene" height="256px">

This is a fork of [Sascha Willems](https://github.com/SaschaWillems) excellent [Vulkan examples](https://github.com/SaschaWillems/Vulkan) with some modifications.  

* All of the code except for the VulkanDebug stuff has been ported to use the [nVidia VKCPP wrapper](https://github.com/nvpro-pipeline/vkcpp)
* All platform specific code for Windows and Linux has been consolidated to use [GLFW 3.2](http://www.glfw.org/)
* Project files for Visual Studio have been removed in favor of a pure [CMake](https://cmake.org/) based system
* Binary files have been removed in favor of CMake external projects
* Enable validation layers by default when building in debug mode
* Avoid excessive use of vkDeviceWaitIdle and vkQueueWaitIdle
* Avoid excessive use of explicit image layout transitions, instead using implicit transitions via the RenderPass and Subpass definitions
* A number of fixes to bugs that prevent the original examples from running on Visual Studio 2012 have been fixed (not necessarily all though)

## Known issues

* I've only tested so far on Windows using Visual Studio 2012 and Visual Studio 2015.  
* I'm still cleaning up after the migration to VKCPP so the code isn't as clean as it could be.  Lots of unnecessary function parameters and structure assignments remain

Assorted C++ examples for [Vulkan(tm)](https://www.khronos.org/vulkan/), the new graphics and compute API from Khronos.

# Building

Use the provided CMakeLists.txt for use with [CMake](https://cmake.org) to generate a build configuration for your toolchain.

# Examples 

This information comes from the [original repository readme](https://github.com/SaschaWillems/Vulkan/blob/master/README.md)

## Beginner Examples

### [Context](examples/init/context.cpp)

Basic example of creating a Vulkan instance, physical device, queue, command pool, 
etc.  However, it does not create a rendering window and produces no graphical 
output, instead printing out some basic information of about the current device.

### [Swap Chain](examples/init/swapchain.cpp)

Create a window and a Vulkan swap chain connected to it.  Uses an empty command 
buffer to clear the frame with a different color for each swap chain image.  This is 
the most basic possible application that colors any pixels on a surface.

### [Triangle](examples/init/triangle.cpp)
<img src="./documentation/screenshots/basic_triangle.png" height="96px" align="right">

Most basic example that renders geometry. Renders a colored triangle using an 
indexed vertex buffer. Vertex and index data are uploaded to device local memory 
using so-called "staging buffers". Uses a single pipeline with basic shaders loaded 
from SPIR-V and and single uniform block for passing matrices that is updated on 
changing the view.

This example is far more explicit than the other examples and is meant to be a 
starting point for learning Vulkan from the ground up. Much of the code is 
boilerplate that you'd usually encapsulate in helper functions and classes (which is 
what the other examples do).
<br><br>

### [Triangle Revisited](examples/init/triangleRevisited.cpp)
<img src="./documentation/screenshots/basic_triangle.png" height="96px" align="right">

A repeat of the triangle example, except this time using the base class that will be 
used for all future examples.  Much of the boilerplate from the previous example has 
been moved into the base class or helper functions.  

This is the first example that allows you to resize the window, demonstrating the 
ability to create the swap chain and any objects which depend on the swap chain.

### [Triangle Animated](examples/init/triangleAnimated.cpp)
<img src="./documentation/screenshots/basic_triangle.png" height="96px" align="right">

Another repeat of the triangle example, this time showing a mechanism by which we 
can make modifications each frame to a uniform buffer containing the projection and 
view matrices.  

## Intermediate Examples

### [Gears](examples/basic/gears.cpp)
<img src="./documentation/screenshots/basic_gears.png" height="96px" align="right">

Vulkan interpretation of glxgears. Procedurally generates separate meshes for each 
gear, with every mesh having it's own uniform buffer object for animation. Also 
demonstrates how to use different descriptor sets.
<br><br>

### [Texture mapping](examples/basic/texture.cpp)
<img src="./documentation/screenshots/basic_texture.png" height="96px" align="right">

Loads a single texture and displays it on a simple quad. Shows how to upload a 
texture including mip maps to the gpu in an optimal (tiling) format. Also 
demonstrates how to display the texture using a combined image sampler with 
anisotropic filtering enabled.
<br><br>

### [Cubemaps](examples/basic/texturecubemap.cpp)
<img src="./documentation/screenshots/texture_cubemap.png" height="96px" align="right">

Building on the basic texture loading example a cubemap is loaded into host visible 
memory and then transformed into an optimal format for the GPU.

The demo uses two different pipelines (and shader sets) to display the cubemap as a 
skybox (background) and as a source for reflections.
<br><br>

### [Texture array](examples/basic/texturearray.cpp)
<img src="./documentation/screenshots/texture_array.png" height="96px" align="right">

Texture arrays allow storing of multiple images in different layers without any 
interpolation between the layers.
This example demonstrates the use of a 2D texture array with instanced rendering. 
Each instance samples from a different layer of the texture array.
<br><br>

### [Particle system](examples/basic/particlefire.cpp)
<img src="./documentation/screenshots/particlefire.png" height="96px" align="right">

Point sprite based particle system simulating a fire. Particles and their attributes 
are stored in a host visible vertex buffer that's updated on the CPU on each frame. 
Also makes use of pre-multiplied alpha for rendering particles with different 
blending modes (smoke and fire) in one single pass.

### [Pipelines](examples/basic/pipelines.cpp)
<img src="./documentation/screenshots/basic_pipelines.png" height="96px" align="right">

[Pipeline state objects](https://www.khronos.org/registry/vulkan/specs/1.0/xhtml/vkspec.html#pipelines) 
replace the biggest part of the dynamic state machine from OpenGL, baking state 
information for culling, blending, rasterization, etc. and shaders into a fixed stat 
that can be optimized much easier by the implementation.

This example uses three different PSOs for rendering the same scene with different 
visuals and shaders and also demonstrates the use 
[pipeline derivatives](https://www.khronos.org/registry/vulkan/specs/1.0/xhtml/vkspec.html#pipelines-pipeline-derivatives).
<br><br>

### [Mesh loading and rendering](examples/basic/pipelines.cpp)
<img src="./documentation/screenshots/basic_mesh.png" height="96px" align="right">

Uses [assimp](https://github.com/assimp/assimp) to load a mesh from a common 3D 
format including a color map. The mesh data is then converted to a fixed vertex 
layout matching the shader vertex attribute bindings.
<br><br>

### [Multi sampling](examples/basic/multisampling.cpp)
<img src="./documentation/screenshots/multisampling.png" height="96px" align="right">

Demonstrates the use of resolve attachments for doing multisampling. Instead of 
doing an explicit resolve from a multisampled image this example creates 
multisampled attachments for the color and depth buffer and sets up the render pass 
to use these as resolve attachments that will get resolved to the visible frame 
buffer at the end of this render pass. To highlight MSAA the example renders a mesh 
with fine details against a bright background. Here is a 
[screenshot without MSAA](./documentation/screenshots/multisampling_nomsaa.png) to 
compare.
<br><br>

### [Mesh instancing](examples/basic/instancing.cpp)
<img src="./documentation/screenshots/instancing.jpg" height="96px" align="right">

Shows the use of instancing for rendering many copies of the same mesh using 
different attributes and textures. A secondary vertex buffer containing instanced 
data, stored in device local memory, is used to pass instance data to the shader via 
vertex attributes with a per-instance step rate. The instance data also contains a 
texture layer index for having different textures for the instanced meshes.
<br><br>

### [Indirect rendering](examples/basic/indirect.cpp)

Shows the use of a shared vertex buffer containing multiple shapes to rendering 
numerous instances of each shape with only one draw call.
<br><br>

### [Push constants](examples/basic/pushconstants.cpp)
<img src="./documentation/screenshots/push_constants.png" height="96px" align="right">

Demonstrates the use of push constants for updating small blocks of shader data with 
high speed (and without having to use a uniform buffer). Displays several light 
sources with position updates through a push constant block in a separate command 
buffer.
<br><br>

### [Skeletal animation](examples/basic/skeletalanimation.cpp)
<img src="./documentation/screenshots/mesh_skeletalanimation.png" height="96px" align="right">

Based on the mesh loading example, this example loads and displays a rigged COLLADA 
model including animations. Bone weights are extracted for each vertex and are 
passed to the vertex shader together with the final bone transformation matrices for 
vertex position calculations.
<br><br>

### [(Tessellation shader) PN-Triangles](examples/basic/tessellation.cpp)
<img src="./documentation/screenshots/tess_pntriangles.jpg" height="96px" align="right">

Generating curved PN-Triangles on the GPU using tessellation shaders to add details 
to low-polygon meshes, based on [this paper](http://alex.vlachos.com/graphics/CurvedPNTriangles.pdf), 
with shaders from 
[this tutorial](http://onrendering.blogspot.de/2011/12/tessellation-on-gpu-curved-pn-triangles.html).
<br><br>

### [Spherical environment mapping](examples/basic/sphericalenvmapping.cpp)
<img src="./documentation/screenshots/spherical_env_mapping.png" height="96px" align="right">

Uses a (spherical) material capture texture containing environment lighting and 
reflection information to fake complex lighting. The example also uses a texture 
array to store (and select) several material caps that can be toggled at runtime.

 The technique is based on [this article](https://github.com/spite/spherical-environment-mapping).
<br><br>

### [(Geometry shader) Normal debugging](examples/basic/geometryshader.cpp)
<img src="./documentation/screenshots/geom_normals.png" height="96px" align="right">

Renders the vertex normals of a complex mesh with the use of a geometry shader. The 
mesh is rendered solid first and the a geometry shader that generates lines from the 
face normals is used in the second pass.
<br><br>

### [Distance field fonts](examples/basic/distancefieldfonts.cpp)
<img src="./documentation/screenshots/font_distancefield.png" height="96px" align="right">

Instead of just sampling a bitmap font texture, a texture with per-character signed 
distance fields is used to generate high quality glyphs in the fragment shader. This 
results in a much higher quality than common bitmap fonts, even if heavily zoomed.

Distance field font textures can be generated with tools like 
[Hiero](https://github.com/libgdx/libgdx/wiki/Hiero).
<br><br>

### [Vulkan demo scene](examples/basic/vulkanscene.cpp)
<img src="./documentation/screenshots/vulkan_scene.png" height="96px" align="right">

More of a playground than an actual example. Renders multiple meshes with different 
shaders (and pipelines) including a background.
<br><br>

## Offscreen Rendering Examples

Demonstrate the use of offsreen framebuffer for rendering effects

### [Offscreen rendering](examples/offscreen/offscreen.cpp)
<img src="./documentation/screenshots/basic_offscreen.png" height="96px" align="right">

Uses a separate framebuffer (that is not part of the swap chain) and a texture 
target for offscreen rendering. The texture is then used as a mirror.
<br><br>

### [Radial blur](examples/offscreen/radialblur.cpp)
<img src="./documentation/screenshots/radial_blur.png" height="96px" align="right">

Demonstrates basic usage of fullscreen shader effects. The scene is rendered 
offscreen first, gets blitted to a texture target and for the final draw this 
texture is blended on top of the 3D scene with a radial blur shader applied.
<br><br>

### [Bloom](examples/offscreen/bloom.cpp)
<img src="./documentation/screenshots/bloom.png" height="96px" align="right">

Implements a bloom effect to simulate glowing parts of a 3D mesh. A two pass 
gaussian blur (horizontal and then vertical) is used to generate a blurred low res 
version of the scene only containing the glowing parts of the 3D mesh. This then 
gets blended onto the scene to add the blur effect.
<br><br>

### [Deferred shading](examples/offscreen/deferred.cpp)
<img src="./documentation/screenshots/deferred_shading.png" height="96px" align="right">

Demonstrates the use of multiple render targets to fill a G-Buffer for deferred 
shading.

Deferred shading collects all values (color, normal, position) into different render 
targets in one pass thanks to multiple render targets, and then does all shading and 
lighting calculations based on these in screen space, thus allowing for much more 
light sources than traditional forward renderers.
<br><br>

## Compute Examples

Demonstrate the use of compute shaders to achieve effects

### [(Compute shader) Particle system](examples/compute/computeparticles.cpp)
<img src="./documentation/screenshots/compute_particles.jpg" height="96px" align="right">

Attraction based particle system. A shader storage buffer is used to store particle 
on which the compute shader does some physics calculations. The buffer is then used 
by the graphics pipeline for rendering with a gradient texture for. Demonstrates the 
use of memory barriers for synchronizing vertex buffer access between a compute and 
graphics pipeline
<br><br>

### [(Compute shader) Ray tracing](examples/compute/raytracing.cpp)
<img src="./documentation/screenshots/compute_raytracing.png" height="96px" align="right">

Implements a simple ray tracer using a compute shader. No primitives are rendered by 
the traditional pipeline except for a fullscreen quad that displays the ray traced 
results of the scene rendered by the compute shaders. Also implements shadows and 
basic reflections.
<br><br>

### [(Compute shader) Image processing](examples/compute/computeshader.cpp)
<img src="./documentation/screenshots/compute_imageprocessing.jpg" height="96px" align="right">

Demonstrates the use of a separate compute queue (and command buffer) to apply 
different convolution kernels on an input image in realtime.
<br><br>

## Advanced Examples

### [(Tessellation shader) Displacement mapping](examples/advanced/displacement.cpp)
<img src="./documentation/screenshots/tess_displacement.jpg" height="96px" align="right">

Uses tessellation shaders to generate additional details and displace geometry based 
on a displacement map (heightmap).
<br><br>

### [Parallax mapping](examples/advanced/parallaxmapping.cpp)
<img src="./documentation/screenshots/parallax_mapping.jpg" height="96px" align="right">

Like normal mapping, parallax mapping simulates geometry on a flat surface. In 
addition to normal mapping a heightmap is used to offset texture coordinates 
depending on the viewing angle giving the illusion of added depth.
<br><br>

### [(Extension) VK_EXT_debug_marker](examples/advanced/debugmarker.cpp)
<img src="./documentation/screenshots/ext_debugmarker.jpg" width="170px" align="right">

Example application to be used along with 
[this tutorial](http://www.saschawillems.de/?page_id=2017) for demonstrating the use 
of the new VK_EXT_debug_marker extension. Introduced with Vulkan 1.0.12, it adds 
functionality to set debug markers, regions and name objects for advanced debugging 
in an offline graphics debugger like [RenderDoc](http://www.renderdoc.org).

<br>

## OpenGL & VR Examples

Interaction with OpenGL and OpenGL-reliant systems such as the Oculus or OpenVR SDKs

### [OpenGL Interoperability](examples/windows/glinterop.cpp)

Demonstrates using an offscreen Vulkan renderer to create images which are then 
blitted to an OpenGL framebuffer for on-screen display
<br><br>

### [Oculus SDK Usage](examples/windows/vr_oculus.cpp)
<img src="./documentation/screenshots/vr.png" height="96px" align="right">

Demonstrates using the Oculus SDK with an offscreen Vulkan renderer to create images 
which are then passed to an the SDK for display on an HMD
<br><br>

### [OpenVR SDK Usage](examples/windows/vr_openvr.cpp)
<img src="./documentation/screenshots/vr.png" height="96px" align="right">

Demonstrates using the OpenVR SDK with an offscreen Vulkan renderer to create images 
which are then passed to an the SDK for display on an HMD
<br><br>

## Broken Examples 

These examples are not functioning as intended yet.

### [Occlusion queries](examples/broken/occlusionquery.cpp)
<img src="./documentation/screenshots/occlusion_queries.png" height="96px" align="right">

#### FIXME the queies seem to work, but generate validation errors every frame 

Shows how to use occlusion queries to determine object visibility depending on the number of passed samples for a given object. Does an occlusion pass first, drawing all objects (and the occluder) with basic shaders, then reads the query results to conditionally color the objects during the final pass depending on their visibility.
<br><br>

### [Multi threaded command buffer generation](examples/broken/multithreading.cpp)
<img src="./documentation/screenshots/multithreading.png" height="96px" align="right">
This example demonstrates multi threaded command buffer generation. All available hardware threads are used to generated n secondary command buffers concurrent, with each thread also checking object visibility against the current viewing frustum. Command buffers are rebuilt on each frame.

Once all threads have finished (and all secondary command buffers have been constructed), the secondary command buffers are executed inside the primary command buffer and submitted to the queue.
<br><br>

### [Text overlay (Multi pass)](examples/broken/textoverlay.cpp)
<img src="./documentation/screenshots/textoverlay.png" height="96px" align="right">

Renders a 2D text overlay on top of an existing 3D scene. The example implements a text overlay class with separate descriptor sets, layouts, pipelines and render pass to detach it from the rendering of the scene. The font is generated by loading glyph data from a [stb font file](http://nothings.org/stb/font/) into a buffer that's copied to the font image.

After rendering the scene, the second render pass of the text overlay class loads the contents of the first render pass and displays text on top of it using blending.

### [Shadowmapping](examples/broken/shadowmapping.cpp)
<img src="./documentation/screenshots/shadowmapping.png" height="96px" align="right">

Shows how to implement directional dynamic shadows with a single shadow map in two passes. Pass one renders the scene from the light's point of view and copies the depth buffer to a depth texture.
The second pass renders the scene from the camera's point of view using the depth texture to compare the depth value of the texels with the one stored in the depth texture to determine whether a texel is shadowed or not and also applies a PCF filter for smooth shadow borders.
To avoid shadow artifacts the dynamic depth bias state ([vkCmdSetDepthBias](https://www.khronos.org/registry/vulkan/specs/1.0/man/html/vkCmdSetDepthBias.html)) is used to apply a constant and slope dept bias factor.

### [Omnidirectional shadow mapping](examples/broken/shadowmappingomni.cpp)
<img src="./documentation/screenshots/shadow_omnidirectional.png" height="96px" align="right">

Uses a dynamic 32 bit floating point cube map for a point light source that casts shadows in all directions (unlike projective shadow mapping).
The cube map faces contain the distances from the light sources, which are then used in the scene rendering pass to determine if the fragment is shadowed or not.
<br><br>

# Credits

This information comes from the [original repository readme](https://github.com/SaschaWillems/Vulkan/blob/master/README.md)

Thanks to the authors of these libraries :
- [OpenGL Mathematics (GLM)](https://github.com/g-truc/glm)
- [OpenGL Image (GLI)](https://github.com/g-truc/gli)
- [Open Asset Import Library](https://github.com/assimp/assimp)
- [Tiny obj loader](https://github.com/syoyo/tinyobjloader)

And a huge thanks to the Vulkan Working Group, Vulkan Advisory Panel, the fine people at [LunarG](http://www.lunarg.com), Baldur Karlsson ([RenderDoc](https://github.com/baldurk/renderdoc)) and everyone from the different IHVs that helped me get the examples up and working on their hardware!

## Attributions / Licenses
Please note that (some) models and textures use separate licenses. Please comply to these when redistributing or using them in your own projects :
- Cubemap used in cubemap example by [Emil Persson(aka Humus)](http://www.humus.name/)
- Armored knight model used in deferred example by [Gabriel Piacenti](http://opengameart.org/users/piacenti)
- Voyager model by [NASA](http://nasa3d.arc.nasa.gov/models)
- Astroboy COLLADA model copyright 2008 Sony Computer Entertainment Inc.
- Old deer model used in tessellation example by [Čestmír Dammer](http://opengameart.org/users/cdmir)
- Hidden treasure scene used in pipeline and debug marker examples by [Laurynas Jurgila](http://www.blendswap.com/user/PigArt)
- Textures used in some examples by [Hugues Muller](http://www.yughues-folio.com)
- Updated compute particle system shader by [Lukas Bergdoll](https://github.com/Voultapher)
- Vulkan scene model (and derived models) by [Dominic Agoro-Ombaka](http://www.agorodesign.com/) and [Sascha Willems](http://www.saschawillems.de)
- Vulkan and the Vulkan logo are trademarks of the [Khronos Group Inc.](http://www.khronos.org)

## External resources
- [LunarG Vulkan SDK](https://vulkan.lunarg.com)
- [Official list of Vulkan resources](https://www.khronos.org/vulkan/resources)
- [Vulkan API specifications](https://www.khronos.org/registry/vulkan/specs/1.0/apispec.html) ([quick reference cards](https://www.khronos.org/registry/vulkan/specs/1.0/refguide/Vulkan-1.0-web.pdf))
- [SPIR-V specifications](https://www.khronos.org/registry/spir-v/specs/1.0/SPIRV.html)
- [My personal view on Vulkan (as a hobby developer)](http://www.saschawillems.de/?p=1886)
