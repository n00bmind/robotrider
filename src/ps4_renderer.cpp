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

#if !SCE_GNMX_ENABLE_GFX_LCUE
#error LCUE must be enabled!
#endif

static const u32 kDisplayBufferWidth = 1920;
static const u32 kDisplayBufferHeight = 1080;


internal Gnmx::VsShader*
InitializeVSWithAllocators( const void* binaryData, MemoryArena* onionAllocator, MemoryArena* garlicAllocator,
                            Gnmx::InputOffsetsCache* offsetsTable = nullptr )
{
    Gnmx::ShaderInfo shaderInfo;
    Gnmx::parseShader( &shaderInfo, binaryData );

    void *shaderBinary = PUSH_SIZE_ALIGNED( garlicAllocator, shaderInfo.m_gpuShaderCodeSize, Gnm::kAlignmentOfShaderInBytes );
    void *shaderHeader = PUSH_SIZE_ALIGNED( onionAllocator, shaderInfo.m_vsShader->computeSize(), Gnm::kAlignmentOfBufferInBytes );

    memcpy( shaderBinary, shaderInfo.m_gpuShaderCode, shaderInfo.m_gpuShaderCodeSize );
    memcpy( shaderHeader, shaderInfo.m_vsShader, shaderInfo.m_vsShader->computeSize() );

    Gnmx::VsShader* result = (Gnmx::VsShader*)shaderHeader;
    result->patchShaderGpuAddress( shaderBinary );

    if( offsetsTable )
        // TODO According to the docs, this should be done before patching for efficiency, so test that!
        // Also, cache it somewhere
        Gnmx::generateInputOffsetsCache( offsetsTable, Gnm::kShaderStageVs, result );

    return result;
}

internal Gnmx::PsShader*
InitializePSWithAllocators( const void* binaryData, MemoryArena* onionAllocator, MemoryArena* garlicAllocator,
                            Gnmx::InputOffsetsCache* offsetsTable = nullptr )
{
    Gnmx::ShaderInfo shaderInfo;
    Gnmx::parseShader( &shaderInfo, binaryData );

    void *shaderBinary = PUSH_SIZE_ALIGNED( garlicAllocator, shaderInfo.m_gpuShaderCodeSize, Gnm::kAlignmentOfShaderInBytes );
    void *shaderHeader = PUSH_SIZE_ALIGNED( onionAllocator, shaderInfo.m_psShader->computeSize(), Gnm::kAlignmentOfBufferInBytes );

    memcpy( shaderBinary, shaderInfo.m_gpuShaderCode, shaderInfo.m_gpuShaderCodeSize );
    memcpy( shaderHeader, shaderInfo.m_psShader, shaderInfo.m_psShader->computeSize() );

    Gnmx::PsShader* result = (Gnmx::PsShader*)shaderHeader;
    result->patchShaderGpuAddress( shaderBinary );

    if( offsetsTable )
        // TODO According to the docs, this should be done before patching for efficiency, so test that!
        // Also, cache it somewhere
        Gnmx::generateInputOffsetsCache( offsetsTable, Gnm::kShaderStagePs, result );

    return result;
}

internal Gnmx::VsShader*
PS4LoadVS( const char *path, PS4RendererState* state, Gnmx::InputOffsetsCache* offsetsTable = nullptr )
{
    Gnmx::VsShader* result = nullptr;

    char absolutePath[MAX_PATH];
    PS4BuildAbsolutePath( path, PS4Path::Binary, absolutePath );
    DEBUGReadFileResult read = globalPlatform.DEBUGReadEntireFile( absolutePath );
    if( read.contents )
    {
        result = InitializeVSWithAllocators( read.contents, &state->onionArena, &state->garlicArena, offsetsTable );
        globalPlatform.DEBUGFreeFileMemory( read.contents );
    }
    else
        INVALID_CODE_PATH;

    return result;
}

internal Gnmx::PsShader*
PS4LoadPS( const char *path, PS4RendererState* state, Gnmx::InputOffsetsCache* offsetsTable = nullptr )
{
    Gnmx::PsShader* result = nullptr;

    char absolutePath[MAX_PATH];
    PS4BuildAbsolutePath( path, PS4Path::Binary, absolutePath );
    DEBUGReadFileResult read = globalPlatform.DEBUGReadEntireFile( absolutePath );
    if( read.contents )
    {
        result = InitializePSWithAllocators( read.contents, &state->onionArena, &state->garlicArena, offsetsTable );
        globalPlatform.DEBUGFreeFileMemory( read.contents );
    }
    else
        INVALID_CODE_PATH

    return result;
}

internal void
PS4LoadShaders( PS4RendererState* state )
{
    const char* filename = nullptr;

    filename = "test_vv.sb";
    state->testVS = PS4LoadVS( filename, state );
    if( !state->testVS )
    {
        LOG( ".ERROR :: Couldn't load shader %s", filename );
        INVALID_CODE_PATH
    }

    filename = "test_p.sb";
    state->testPS = PS4LoadPS( filename, state );
    if( !state->testPS )
    {
        LOG( ".ERROR :: Couldn't load shader %s", filename );
        INVALID_CODE_PATH
    }

    filename = "clear_p.sb";
    state->clearPS = PS4LoadPS( filename, state, &state->clearOffsetsTable );
    if( !state->clearPS )
    {
        LOG( ".ERROR :: Couldn't load shader %s", filename );
        INVALID_CODE_PATH
    }

    // Generate the fetch shader for the VS stage
    state->fsMemory = PUSH_SIZE_ALIGNED( &state->garlicArena, Gnmx::computeVsFetchShaderSize( state->testVS ),
                                         Gnm::kAlignmentOfFetchShaderInBytes );
    if( !state->fsMemory )
    {
        LOG( ".ERROR :: Cannot allocate memory for fetch shader" );
        INVALID_CODE_PATH
    }

    Gnm::FetchShaderInstancingMode* instancingData = nullptr;
    Gnmx::generateVsFetchShader( state->fsMemory, &state->fsModifier, state->testVS, instancingData,
                                 instancingData == nullptr ? 0 : 256 );

    // Generate the shader input caches.
    // Using a pre-calculated shader input cache is optional with CUE but it
    // normally reduces the CPU time necessary to build the command buffers.
    // TODO Shouldn't we be doing this at load time instead?
    Gnmx::generateInputOffsetsCache( &state->vsOffsetsTable, Gnm::kShaderStageVs, state->testVS );
    Gnmx::generateInputOffsetsCache( &state->psOffsetsTable, Gnm::kShaderStagePs, state->testPS );
}


//////////////////////////////////////////////

struct Vertex
{
    float x, y, z;	// Position
    float r, g, b;	// Color
    float u, v;		// UVs
};

enum VertexElements
{
    kVertexPosition = 0,
    kVertexColor,
    kVertexUv,

    kVertexElemCount
};

static const Vertex kVertexData[] =
{
    // 2    3
    // +----+
    // |\   |
    // | \  |
    // |  \ |
    // |   \|
    // +----+
    // 0    1

    //   POSITION                COLOR               UV
    { -0.5f, -0.5f, 0.0f,    0.7f, 0.7f, 1.0f,    0.0f, 1.0f },
    { 0.5f, -0.5f, 0.0f,    0.7f, 0.7f, 1.0f,    1.0f, 1.0f },
    { -0.5f,  0.5f, 0.0f,    0.7f, 1.0f, 1.0f,    0.0f, 0.0f },
    { 0.5f,  0.5f, 0.0f,    1.0f, 0.7f, 1.0f,    1.0f, 0.0f },
};
static const uint16_t kIndexData[] =
{
    0, 1, 2,
    1, 3, 2
};



internal Gnm::Buffer vertexBuffers[kVertexElemCount];

#include <vectormath.h>
using namespace sce::Vectormath::Simd::Aos;

struct Matrix4GPU
{
    v4 x;
    v4 y;
    v4 z;
    v4 w;

    Matrix4GPU &operator=( const Matrix4 &rhs )
    {
        memcpy( this, &rhs, sizeof( *this ) );
        return *this;
    }
};

struct ShaderConstants
{
    Matrix4GPU m_WorldViewProj;
};

Vertex *vertexData;
uint16_t *indexData;


//////////////////////////////////////////////


PS4RendererState
PS4InitRenderer()
{
    PS4RendererState state = {};

    u32 onionMemorySizeBytes = MEGABYTES( 16 );
    void* onionBlock = PS4AllocAndMap( 0, onionMemorySizeBytes, SCE_KERNEL_WB_ONION, KILOBYTES(64), 
                                       SCE_KERNEL_PROT_CPU_RW | SCE_KERNEL_PROT_GPU_ALL );
    u32 garlicMemorySizeBytes = MEGABYTES( 64 * 4 );
    void* garlicBlock = PS4AllocAndMap( 0, garlicMemorySizeBytes, SCE_KERNEL_WC_GARLIC, KILOBYTES(64), 
                                        SCE_KERNEL_PROT_CPU_WRITE | SCE_KERNEL_PROT_GPU_ALL );

    InitializeArena( &state.onionArena, (u8*)onionBlock, onionMemorySizeBytes );
    InitializeArena( &state.garlicArena, (u8*)garlicBlock, garlicMemorySizeBytes );

    PS4LoadShaders( &state );

    state.videoOutHandle = sceVideoOutOpen( 0, SCE_VIDEO_OUT_BUS_TYPE_MAIN, 0, NULL );
    if( state.videoOutHandle < 0 )
    {
        LOG( ".ERROR :: sceVideoOutOpen failed: 0x%08X", state.videoOutHandle );
        INVALID_CODE_PATH
    }
    // Initialize the flip rate: 0: 60Hz, 1: 30Hz or 2: 20Hz
    int ret = sceVideoOutSetFlipRate( state.videoOutHandle, 0 );
    if( ret != SCE_OK )
    {
        LOG( ".ERROR :: sceVideoOutSetFlipRate failed: 0x%08X", ret );
        INVALID_CODE_PATH
    }


    // Initialize the render contexts
    for( u32 i = 0; i < ARRAYCOUNT(state.renderContexts); ++i )
    {
        // NOTE Using LCUE

        // Calculate the size of the resource buffer for the given number of draw calls
        const u32 resourceBufferSizeInBytes =
            Gnmx::LightweightConstantUpdateEngine::computeResourceBufferSize(
                Gnmx::LightweightConstantUpdateEngine::kResourceBufferGraphics,
                100 ); // Number of batches

        // Allocate the LCUE resource buffer memory
        state.renderContexts[i].resourceBuffer = PUSH_SIZE_ALIGNED( &state.garlicArena, resourceBufferSizeInBytes,
                                                                     Gnm::kAlignmentOfBufferInBytes );

        if( !state.renderContexts[i].resourceBuffer )
        {
            LOG( ".ERROR :: Cannot allocate the LCUE resource buffer memory" );
            INVALID_CODE_PATH
        }

        // Allocate the draw command buffer
        const size_t kDcbSizeInBytes = MEGABYTES(2);
        state.renderContexts[i].dcbBuffer = PUSH_SIZE_ALIGNED( &state.onionArena, kDcbSizeInBytes,
                                                                Gnm::kAlignmentOfBufferInBytes );

        if( !state.renderContexts[i].dcbBuffer )
        {
            LOG( ".ERROR :: Cannot allocate the draw command buffer memory" );
            INVALID_CODE_PATH
        }

        // Initialize the GfxContext used by this rendering context
        state.renderContexts[i].gfxContext.init(
            state.renderContexts[i].dcbBuffer,		    // Draw command buffer address
            kDcbSizeInBytes,					        // Draw command buffer size in bytes
            state.renderContexts[i].resourceBuffer,	    // Resource buffer address
            resourceBufferSizeInBytes,			        // Resource buffer address in bytes
            nullptr );							        // Global resource table

        state.renderContexts[i].contextLabel = (volatile u32*)PUSH_SIZE_ALIGNED( &state.onionArena, 4, 8 );
        if( !state.renderContexts[i].contextLabel )
        {
            LOG( ".ERROR :: Cannot allocate a GPU label" );
            INVALID_CODE_PATH
        }

        state.renderContexts[i].contextLabel[0] = kRenderContextFree;
    }


    Gnm::GpuMode gpuMode = Gnm::getGpuMode();
    void *surfaceAddresses[ARRAYCOUNT(state.displayBuffers)];

    // Initialize the display buffers
    for( uint32_t i = 0; i < ARRAYCOUNT(state.displayBuffers); ++i )
    {
        // Compute the tiling mode for the render target
        Gnm::TileMode tileMode;
        Gnm::DataFormat format = Gnm::kDataFormatB8G8R8A8UnormSrgb;
        ret = GpuAddress::computeSurfaceTileMode(
            gpuMode,                                        // NEO or base
            &tileMode,										// Tile mode pointer
            GpuAddress::kSurfaceTypeColorTargetDisplayable,	// Surface type
            format,											// Surface format
            1 );											// Elements per pixel
        if( ret != SCE_OK )
        {
            LOG( ".ERROR :: GpuAddress::computeSurfaceTileMode: 0x%08X", ret );
            INVALID_CODE_PATH
        }

        // Initialize the render target descriptor
        Gnm::RenderTargetSpec spec;
        spec.init();
        spec.m_width = kDisplayBufferWidth;
        spec.m_height = kDisplayBufferHeight;
        spec.m_pitch = 0;
        spec.m_numSlices = 1;
        spec.m_colorFormat = format;
        spec.m_colorTileModeHint = tileMode;
        spec.m_minGpuMode = gpuMode;
        spec.m_numSamples = Gnm::kNumSamples1;
        spec.m_numFragments = Gnm::kNumFragments1;
        spec.m_flags.enableCmaskFastClear = 0;
        spec.m_flags.enableFmaskCompression = 0;
        ret = state.displayBuffers[i].renderTarget.init( &spec );
        if( ret != SCE_GNM_OK )
        {
            LOG( ".ERROR :: Failed initializing render target: 0x%08X", ret );
            INVALID_CODE_PATH
        }

        Gnm::SizeAlign sizeAlign = state.displayBuffers[i].renderTarget.getColorSizeAlign();
        // Allocate the render target memory
        surfaceAddresses[i] = PUSH_SIZE_ALIGNED( &state.garlicArena, sizeAlign.m_size, sizeAlign.m_align );
        if( !surfaceAddresses[i] )
        {
            LOG( ".ERROR :: Cannot allocate the render target memory" );
            INVALID_CODE_PATH
        }
        state.displayBuffers[i].renderTarget.setAddresses( surfaceAddresses[i], 0, 0 );
        state.displayBuffers[i].displayIndex = i;
    }


    // Initialize the VideoOut buffer descriptor. The pixel format must
    // match with the render target data format, which in this case is
    // Gnm::kDataFormatB8G8R8A8UnormSrgb
    Gnm::RenderTarget& backRenderTarget = state.displayBuffers[0].renderTarget;
    SceVideoOutBufferAttribute videoOutBufferAttribute;
    sceVideoOutSetBufferAttribute(
        &videoOutBufferAttribute,
        SCE_VIDEO_OUT_PIXEL_FORMAT_B8_G8_R8_A8_SRGB,
        SCE_VIDEO_OUT_TILING_MODE_TILE,
        SCE_VIDEO_OUT_ASPECT_RATIO_16_9,
        backRenderTarget.getWidth(),
        backRenderTarget.getHeight(),
        backRenderTarget.getPitch() );

    // Register the buffers to the slot: [0..kDisplayBufferCount-1]
    ret = sceVideoOutRegisterBuffers(
        state.videoOutHandle,
        0, // Start index
        surfaceAddresses,
        ARRAYCOUNT(state.displayBuffers),
        &videoOutBufferAttribute );
    if( ret != SCE_OK )
    {
        LOG( ".ERROR :: sceVideoOutRegisterBuffers failed: 0x%08X", ret );
        INVALID_CODE_PATH
    }


    // Compute the tiling mode for the depth buffer
    Gnm::TileMode depthTileMode;
    Gnm::DataFormat depthFormat = Gnm::DataFormat::build( Gnm::kZFormat32Float );

    ret = GpuAddress::computeSurfaceTileMode(
        gpuMode,                                        // NEO or Base
        &depthTileMode,									// Tile mode pointer
        GpuAddress::kSurfaceTypeDepthOnlyTarget,		// Surface type
        depthFormat,									// Surface format
        1 );											// Elements per pixel
    if( ret != SCE_OK )
    {
        LOG( "GpuAddress::computeSurfaceTileMode: 0x%08X", ret );
        INVALID_CODE_PATH
    }

    Gnm::DepthRenderTargetSpec spec;
    spec.init();
    spec.m_width = kDisplayBufferWidth;
    spec.m_height = kDisplayBufferHeight;
    spec.m_pitch = 0;
    spec.m_numSlices = 1;
    spec.m_zFormat = depthFormat.getZFormat();
    spec.m_stencilFormat = Gnm::kStencilInvalid;
    spec.m_minGpuMode = gpuMode;
    spec.m_numFragments = Gnm::kNumFragments1;
    spec.m_flags.enableHtileAcceleration = 0;       // TODO Look into this
    
    ret = state.depthTarget.init( &spec );
    if( ret != SCE_GNM_OK )
    {
        LOG( ".ERROR :: Depth target initialization failed: 0x%08X", ret );
        INVALID_CODE_PATH
    }

    Gnm::SizeAlign depthTargetSizeAlign = state.depthTarget.getZSizeAlign();

    // Allocate the depth buffer
    void *depthMemory = PUSH_SIZE_ALIGNED( &state.garlicArena, depthTargetSizeAlign.m_size, depthTargetSizeAlign.m_align );
    if( !depthMemory )
    {
        LOG( "Cannot allocate the depth buffer" );
        INVALID_CODE_PATH
    }
    state.depthTarget.setAddresses( depthMemory, nullptr );


    // Create the event queue used to synchronize with end-of-pipe interrupts
    ret = sceKernelCreateEqueue( &state.eopEventQueue, "EOP QUEUE" );
    if( ret != SCE_OK )
    {
        LOG( "sceKernelCreateEqueue failed: 0x%08X", ret );
        INVALID_CODE_PATH
    }

    // Register for the end-of-pipe events
    ret = Gnm::addEqEvent( state.eopEventQueue, Gnm::kEqEventGfxEop, NULL );
    if( ret != SCE_OK )
    {
        LOG( "Gnm::addEqEvent failed: 0x%08X", ret );
        INVALID_CODE_PATH
    }


    // Init some test vertex data
    vertexData = PUSH_ARRAY_ALIGNED( &state.garlicArena, ARRAYCOUNT(kVertexData), Vertex,
                                     Gnm::kAlignmentOfBufferInBytes );
    indexData = PUSH_ARRAY_ALIGNED( &state.garlicArena, ARRAYCOUNT(kIndexData), uint16_t,
                                    Gnm::kAlignmentOfBufferInBytes );
    ASSERT( vertexData && indexData );

    // Copy the vertex/index data onto the GPU mapped memory
    COPY( kVertexData, vertexData, sizeof( kVertexData ) );
    COPY( kIndexData, indexData, sizeof( kIndexData ) );

    // Initialize the vertex buffers pointing to each vertex element
    vertexBuffers[kVertexPosition].initAsVertexBuffer(
        &vertexData->x,
        Gnm::kDataFormatR32G32B32Float,
        sizeof( Vertex ),
        ARRAYCOUNT( kVertexData ) );

    vertexBuffers[kVertexColor].initAsVertexBuffer(
        &vertexData->r,
        Gnm::kDataFormatR32G32B32Float,
        sizeof( Vertex ),
        ARRAYCOUNT( kVertexData ) );

    vertexBuffers[kVertexUv].initAsVertexBuffer(
        &vertexData->u,
        Gnm::kDataFormatR32G32Float,
        sizeof( Vertex ),
        ARRAYCOUNT( kVertexData ) );


    return state;
}

// TODO This needs more testing
// In particular, docs advise to "Draw a full-screen quad at maximum depth with the depth-test feature disabled".
// We're disabling the depth-test (while still writing to the z buffer), but since the full-screen quad is "embedded"
// in Gnm, we have no way of knowing at which depth it's being rendered, so test against geometry with very "deep" Z coords.
internal
void ClearRenderTarget( const Gnm::RenderTarget& renderTarget, const v4& color, const Gnmx::PsShader& clearPS,
                        const Gnmx::InputOffsetsCache& clearOffsetsTable, sce::Gnmx::GnmxGfxContext* gfxc )
{
    gfxc->setRenderTarget( 0, &renderTarget );
    gfxc->setDepthRenderTarget( (Gnm::DepthRenderTarget*)NULL );
    gfxc->setPsShader( &clearPS, &clearOffsetsTable );

    Gnm::BlendControl blendControl;
    blendControl.init();
    blendControl.setBlendEnable( false );
    gfxc->setBlendControl( 0, blendControl );
    gfxc->setRenderTargetMask( 0x0000000F );

    Gnm::DepthStencilControl dsc;
    dsc.init();
    dsc.setDepthControl( Gnm::kDepthControlZWriteEnable, Gnm::kCompareFuncNever );
    dsc.setDepthEnable( false );
    gfxc->setDepthStencilControl( dsc );
    gfxc->setDepthStencilDisable();
    gfxc->setupScreenViewport( 0, 0, renderTarget.getWidth(), renderTarget.getHeight(), 0.5f, 0.5f );

    v4 *constants = (v4*)gfxc->allocateFromCommandBuffer( sizeof(v4), Gnm::kEmbeddedDataAlignment4 );
    *constants = color;
    Gnm::Buffer constantBuffer;
    constantBuffer.initAsConstantBuffer( constants, sizeof(*constants) );
    gfxc->setConstantBuffers( Gnm::kShaderStagePs, 0, 1, &constantBuffer );

    gfxc->setCbControl( Gnm::kCbModeNormal, Gnm::kRasterOpCopy );
    sce::Gnmx::renderFullScreenQuad( gfxc );
}

void
PS4RenderToOutput( const RenderCommands& renderCommands, PS4RendererState* state )
{
    int ret;
    PS4RenderContext* renderContext = state->renderContexts + state->renderContextIndex;
    PS4DisplayBuffer* backBuffer = state->displayBuffers + state->backBufferIndex;

    Gnmx::GnmxGfxContext &gfxc = renderContext->gfxContext;

    // Wait until the context label has been written to make sure that the
    // GPU finished parsing the command buffers before overwriting them
    while( renderContext->contextLabel[0] != kRenderContextFree )
    {
        // Wait for the EOP event
        SceKernelEvent eopEvent;
        int count;
        ret = sceKernelWaitEqueue( state->eopEventQueue, &eopEvent, 1, &count, NULL );
        if( ret != SCE_OK )
        {
            LOG( "sceKernelWaitEqueue failed: 0x%08X", ret );
            INVALID_CODE_PATH
        }
    }

    // Reset the flip GPU label
    renderContext->contextLabel[0] = kRenderContextInUse;

    // Reset the graphical context and initialize the hardware state
    gfxc.reset();
    gfxc.initializeDefaultHardwareState();

    // The waitUntilSafeForRendering stalls the GPU until the scan-out
    // operations on the current display buffer have been completed.
    // This command is not blocking for the CPU.
    //
    // NOTE
    // This command should be used right before writing the display buffer.
    //
    gfxc.waitUntilSafeForRendering( state->videoOutHandle, backBuffer->displayIndex );




    ////////////////// TEST SETUP & DRAWING




    // Setup the viewport to match the entire screen.
    // The z-scale and z-offset values are used to specify the transformation
    // from clip-space to screen-space
    gfxc.setupScreenViewport(0, 0, backBuffer->renderTarget.getWidth(), backBuffer->renderTarget.getHeight(),
                             0.5f,		    // Z-scale
                             0.5f );	    // Z-offset

    // Bind the render & depth targets to the context
    gfxc.setRenderTarget( 0, &backBuffer->renderTarget );
    gfxc.setDepthRenderTarget( &state->depthTarget );

    // Clear the color and the depth target
    ClearRenderTarget( backBuffer->renderTarget, V4( 1.f, 0.f, 1.f, 1 ), *state->clearPS, state->clearOffsetsTable, &gfxc );

    // Enable z-writes using a less comparison function
    Gnm::DepthStencilControl dsc;
    dsc.init();
    dsc.setDepthControl( Gnm::kDepthControlZWriteEnable, Gnm::kCompareFuncLess );
    dsc.setDepthEnable( true );
    gfxc.setDepthStencilControl( dsc );

    // Cull clock-wise backfaces
    Gnm::PrimitiveSetup primSetupReg;
    primSetupReg.init();
    primSetupReg.setCullFace( Gnm::kPrimitiveSetupCullFaceBack );
    primSetupReg.setFrontFace( Gnm::kPrimitiveSetupFrontFaceCcw );
    gfxc.setPrimitiveSetup( primSetupReg );

    // Setup an additive blending mode
    Gnm::BlendControl blendControl;
    blendControl.init();
    blendControl.setBlendEnable( true );
    blendControl.setColorEquation(
                                  Gnm::kBlendMultiplierSrcAlpha,
                                  Gnm::kBlendFuncAdd,
                                  Gnm::kBlendMultiplierOneMinusSrcAlpha );
    gfxc.setBlendControl( 0, blendControl );

    // Setup the output color mask
    gfxc.setRenderTargetMask( 0xF );



    // Activate the VS and PS shader stages
    gfxc.setActiveShaderStages( Gnm::kActiveShaderStagesVsPs );
    gfxc.setVsShader( state->testVS, state->fsModifier, state->fsMemory, &state->vsOffsetsTable );
    gfxc.setPsShader( state->testPS, &state->psOffsetsTable );

    // Setup the vertex buffer used by the ES stage (vertex shader)
    // Note that the setXxx methods of GfxContext which are used to set
    // shader resources (e.g.: V#, T#, S#, ...) map directly on the
    // Constants Update Engine. These methods do not directly produce PM4
    // packets in the command buffer. The CUE gathers all the resource
    // definitions and creates a set of PM4 packets later on in the
    // gfxc.drawXxx method.
    gfxc.setVertexBuffers( Gnm::kShaderStageVs, 0, kVertexElemCount, vertexBuffers );

    // Allocate the vertex shader constants from the command buffer
    ShaderConstants *constants = static_cast<ShaderConstants*>(
                                                               gfxc.allocateFromCommandBuffer( sizeof( ShaderConstants ), Gnm::kEmbeddedDataAlignment4 ));

    // Initialize the vertex shader constants
    if( constants )
    {
        static float angle = 0.0f;
        angle += 1.0f / 120.0f;
        const float kAspectRatio = float( kDisplayBufferWidth ) / float( kDisplayBufferHeight );
        const Matrix4 rotationMatrix = Matrix4::rotationZ( angle );
        const Matrix4 scaleMatrix = Matrix4::scale( Vector3( 1, kAspectRatio, 1 ) );
        constants->m_WorldViewProj = (scaleMatrix * rotationMatrix);

        Gnm::Buffer constBuffer;
        constBuffer.initAsConstantBuffer( constants, sizeof( ShaderConstants ) );
        gfxc.setConstantBuffers( Gnm::kShaderStageVs, 0, 1, &constBuffer );
    }
    else
    {
        LOG( "Cannot allocate vertex shader constants" );
        INVALID_CODE_PATH
    }


    // Submit a draw call
    gfxc.setPrimitiveType( Gnm::kPrimitiveTypeTriList );
    gfxc.setIndexSize( Gnm::kIndexSize16 );
    gfxc.drawIndex( ARRAYCOUNT(kIndexData), indexData );
}

void
PS4SwapBuffers( PS4RendererState* state )
{
    PS4RenderContext* renderContext = state->renderContexts + state->renderContextIndex;
    PS4DisplayBuffer* backBuffer = state->displayBuffers + state->backBufferIndex;

    Gnmx::GnmxGfxContext &gfxc = renderContext->gfxContext;

    // Submit the command buffers, request a flip of the display buffer and
    // write the GPU label that determines the render context state (free)
    // and trigger a software interrupt to signal the EOP event queue
    // NOTE: for this basic sample we are submitting a single GfxContext
    // per frame. Submitting multiple GfxContext-s per frame is allowed.
    // Multiple contexts are processed in order, i.e.: they start in
    // submission order and end in submission order.
    int ret = gfxc.submitAndFlipWithEopInterrupt( state->videoOutHandle,
                                                  backBuffer->displayIndex,
                                                  SCE_VIDEO_OUT_FLIP_MODE_VSYNC,
                                                  0,
                                                  sce::Gnm::kEopFlushCbDbCaches,
                                                  const_cast<uint32_t*>(renderContext->contextLabel),
                                                  kRenderContextFree,
                                                  sce::Gnm::kCacheActionWriteBackAndInvalidateL1andL2 );
    if( ret != sce::Gnm::kSubmissionSuccess )
    {
        // Analyze the error code to determine whether the command buffers
        // have been submitted to the GPU or not
        if( ret & sce::Gnm::kStatusMaskError )
        {
            // Error codes in the kStatusMaskError family block submissions
            // so we need to mark this render context as not-in-flight
            renderContext->contextLabel[0] = kRenderContextFree;
        }

        LOG( "GfxContext::submitAndFlip failed: 0x%08X\n", ret );
        INVALID_CODE_PATH
    }

    // Signal the system that every draw for this frame has been submitted.
    // This function gives permission to the OS to hibernate when all the
    // currently running GPU tasks (graphics and compute) are done.
    ret = Gnm::submitDone();
    if( ret != SCE_OK )
    {
        LOG( "Gnm::submitDone failed: 0x%08X\n", ret );
        INVALID_CODE_PATH
    }

    // Rotate the display buffers & contexts
    state->backBufferIndex = (state->backBufferIndex + 1) % ARRAYCOUNT(state->displayBuffers);
    state->renderContextIndex = (state->renderContextIndex + 1) % ARRAYCOUNT(state->renderContexts);
}

void
PS4ShutdownRenderer( PS4RendererState* state )
{
    // Wait for the GPU to be idle before deallocating its resources
    for( uint32_t i = 0; i < ARRAYCOUNT(state->renderContexts); ++i )
    {
        if( state->renderContexts[i].contextLabel )
        {
            while( state->renderContexts[i].contextLabel[0] != kRenderContextFree )
            {
                sceKernelUsleep( 1000 );
            }
        }
    }

    // Unregister the EOP event queue
    int ret = Gnm::deleteEqEvent( state->eopEventQueue, Gnm::kEqEventGfxEop );
    if( ret != SCE_OK )
    {
        LOG( "Gnm::deleteEqEvent failed: 0x%08X\n", ret );
        INVALID_CODE_PATH
    }

    // Destroy the EOP event queue
    ret = sceKernelDeleteEqueue( state->eopEventQueue );
    if( ret != SCE_OK )
    {
        LOG( "sceKernelDeleteEqueue failed: 0x%08X\n", ret );
        INVALID_CODE_PATH
    }

    // Terminate the video output
    ret = sceVideoOutClose( state->videoOutHandle );
    if( ret != SCE_OK )
    {
        LOG( "sceVideoOutClose failed: 0x%08X\n", ret );
        INVALID_CODE_PATH
    }

    // Releasing manually each allocated resource is not necessary as we are
    // terminating the linear allocators for ONION and GARLIC here.
    PS4Free( &state->onionArena.base );
    state->onionArena = {};

    PS4Free( &state->garlicArena.base );
    state->garlicArena = {};
}
