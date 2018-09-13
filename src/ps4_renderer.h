/*
The MIT License

Copyright (c) 2017 Oscar Peñas Pariente <oscarpp80@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef __PS4_RENDERER_H__
#define __PS4_RENDERER_H__ 

enum PS4RenderContextState
{
    kRenderContextFree = 0,
    kRenderContextInUse = 1,
};

struct PS4RenderContext
{
    Gnmx::GnmxGfxContext    gfxContext;
    void*                   resourceBuffer;
    void*                   dcbBuffer;
    volatile u32*           contextLabel;
};

struct PS4DisplayBuffer
{
    Gnm::RenderTarget       renderTarget;
    int                     displayIndex;
};

struct PS4ShaderProgram
{
    Gnmx::VsShader* vertexShader;
    Gnmx::PsShader* fragmentShader;
};

struct PS4RendererState
{
    MemoryArena onionArena;
    MemoryArena garlicArena;

    int videoOutHandle;

    // Since we're using one context per frame, this represents the number of frames that can be "in flight"
    PS4RenderContext renderContexts[2];
    u32 renderContextIndex;

    // Using a triple buffering scheme
    // NOTE Not sure what the advange is of having more buffers than contexts here !?
    PS4DisplayBuffer displayBuffers[3];
    u32 backBufferIndex;

    Gnm::DepthRenderTarget depthTarget;
    SceKernelEqueue eopEventQueue;

    Gnmx::PsShader* clearPS;
    Gnmx::InputOffsetsCache clearOffsetsTable;

    Gnmx::VsShader* testVS;
    Gnmx::InputOffsetsCache vsOffsetsTable;
    Gnmx::PsShader* testPS;
    Gnmx::InputOffsetsCache psOffsetsTable;
    void* fsMemory;
    u32 fsModifier;
};


#endif /* __PS4_RENDERER_H__ */
