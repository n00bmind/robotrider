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

#if !RELEASE && !defined(SCE_GNM_DEBUG)
#error Non-release build should define SCE_GNM_DEBUG!
#endif


internal const u32 kDisplayBufferWidth = 1920;
internal const u32 kDisplayBufferHeight = 1080;

internal
PS4ShaderProgram globalShaderPrograms[] =
{
    {
        ShaderProgramName::PlainColor,
        "default_vv.sb",
        nullptr,
        "plain_color_p.sb",
    },
    //{
        //ShaderProgramName::FlatShading,
        //"default_vv.sb",
        //"face_normal.gs.sb",
        //"flat_p.sb",
////         { "inPosition", "inTexCoords", "inColor" },
////         { "mTransform" },
    //},
};



internal Gnmx::VsShader*
InitializeVSWithAllocators( const void* binaryData, MemoryArena* onionAllocator, MemoryArena* garlicAllocator,
                            Gnm::OwnerHandle ownerHandle, Gnmx::InputOffsetsCache* offsetsTable = nullptr )
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

    Gnm::registerResource( nullptr, ownerHandle, result->getBaseAddress(), shaderInfo.m_gpuShaderCodeSize,
                           "VsShader", Gnm::kResourceTypeShaderBaseAddress, 0 );

    return result;
}

internal Gnmx::PsShader*
InitializePSWithAllocators( const void* binaryData, MemoryArena* onionAllocator, MemoryArena* garlicAllocator,
                            Gnm::OwnerHandle ownerHandle, Gnmx::InputOffsetsCache* offsetsTable = nullptr )
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
        // Generate the shader input caches.
        // Using a pre-calculated shader input cache is optional with CUE but it
        // normally reduces the CPU time necessary to build the command buffers.
        Gnmx::generateInputOffsetsCache( offsetsTable, Gnm::kShaderStagePs, result );

    Gnm::registerResource( nullptr, ownerHandle, result->getBaseAddress(), shaderInfo.m_gpuShaderCodeSize,
                           "PsShader", Gnm::kResourceTypeShaderBaseAddress, 0 );

    return result;
}

internal bool
LoadVSFromFile( const char *path, PS4RendererState* state, PS4Shader* result )
{
    Gnmx::VsShader *shader = nullptr;

    char absolutePath[MAX_PATH];
    if( path[0] != '/' )
    {
        // If path is relative, use assets location to complete it
        PS4BuildAbsolutePath( path, PS4Path::Binary, absolutePath );
        path = absolutePath;
    }

    DEBUGReadFileResult readFile = globalPlatform.DEBUGReadEntireFile( path );
    if( readFile.contents )
    {
        shader = InitializeVSWithAllocators( readFile.contents, &state->onionArena, &state->garlicArena,
                                             state->ownerHandle, &result->offsetsTable );
        globalPlatform.DEBUGFreeFileMemory( readFile.contents );
    }
    else
    {
        INVALID_CODE_PATH
        return false;
    }

    result->vs = shader;
    return true;
}

internal bool
LoadPSFromFile( const char *path, PS4RendererState* state, PS4Shader* result )
{
    Gnmx::PsShader *shader = nullptr;

    char absolutePath[MAX_PATH];
    if( path[0] != '/' )
    {
        // If path is relative, use assets location to complete it
        PS4BuildAbsolutePath( path, PS4Path::Binary, absolutePath );
        path = absolutePath;
    }

    DEBUGReadFileResult readFile = globalPlatform.DEBUGReadEntireFile( path );
    if( readFile.contents )
    {
        shader = InitializePSWithAllocators( readFile.contents, &state->onionArena, &state->garlicArena,
                                             state->ownerHandle, &result->offsetsTable );
        globalPlatform.DEBUGFreeFileMemory( readFile.contents );
    }
    else
    {
        INVALID_CODE_PATH
        return false;
    }

    result->ps = shader;
    return true;
}

internal void
PS4LoadShaders( PS4RendererState* state )
{
    const char* filename = nullptr;

    filename = "clear_p.sb";
    bool ret = LoadPSFromFile( filename, state, &state->clearPS );
    if( !ret )
    {
        LOG( ".ERROR :: Couldn't load clear shader %s", filename );
        INVALID_CODE_PATH
    }

    for( u32 i = 0; i < ARRAYCOUNT(globalShaderPrograms); ++i )
    {
        PS4ShaderProgram& prg = globalShaderPrograms[i];

        Gnm::ActiveShaderStages stages = Gnm::kActiveShaderStagesVsPs;
        if( prg.gsFilename )
            stages = Gnm::kActiveShaderStagesEsGsVsPs;
        prg.activeStages = stages;

        // Vertex shader
        ret = LoadVSFromFile( prg.vsFilename, state, (PS4Shader*)&prg.vertexShader );
        if( !ret )
        {
            LOG( ".ERROR :: Couldn't load vertex shader %s", prg.vsFilename );
            INVALID_CODE_PATH
        }

        // Generate the fetch shader for the VS stage
        prg.vertexShader.fsMemory =
            PUSH_SIZE_ALIGNED( &state->garlicArena,
                               Gnmx::computeVsFetchShaderSize( prg.vertexShader.shader.vs ),
                               Gnm::kAlignmentOfFetchShaderInBytes );
        if( !prg.vertexShader.fsMemory )
        {
            LOG( ".ERROR :: Cannot allocate memory for fetch shader" );
            INVALID_CODE_PATH
        }
        Gnm::FetchShaderInstancingMode* instancingData = nullptr;
        Gnmx::generateVsFetchShader( prg.vertexShader.fsMemory, &prg.vertexShader.fsModifier,
                                     prg.vertexShader.shader.vs,
                                     instancingData, instancingData == nullptr ? 0 : 256 );

        // Pixel shader
        ret = LoadPSFromFile( prg.psFilename, state, &prg.pixelShader );
        if( !ret )
        {
            LOG( ".ERROR :: Couldn't load pixel shader %s", prg.psFilename );
            INVALID_CODE_PATH
        }
    }
}


internal void
ClearRenderTarget( const Gnm::RenderTarget& renderTarget, Gnmx::GnmxGfxContext* gfxc, const v4 &color,
                   PS4RendererState* state )
{
    gfxc->setRenderTarget( 0, &renderTarget );
    gfxc->setDepthRenderTarget( &state->depthTarget );

    gfxc->setPsShader( state->clearPS.ps, &state->clearPS.offsetsTable );
    v4 *constants = (v4*)gfxc->allocateFromCommandBuffer( sizeof( v4 ), Gnm::kEmbeddedDataAlignment4 );
    *constants = color;
    Gnm::Buffer constantBuffer;
    constantBuffer.initAsConstantBuffer( constants, sizeof( *constants ) );
    gfxc->setConstantBuffers( Gnm::kShaderStagePs, 0, 1, &constantBuffer );

    Gnm::BlendControl blendControl;
    blendControl.init();
    blendControl.setBlendEnable( false );
    gfxc->setBlendControl( 0, blendControl );

    Gnm::DbRenderControl dbRenderControl;
    dbRenderControl.init();
    dbRenderControl.setDepthClearEnable( true );
    gfxc->setDbRenderControl( dbRenderControl );
    gfxc->setDepthClearValue( 1.0f );

    Gnm::DepthStencilControl dsc;
    dsc.init();
    dsc.setDepthControl( Gnm::kDepthControlZWriteEnable, Gnm::kCompareFuncAlways );
    dsc.setStencilFunction( Gnm::kCompareFuncNever );
    dsc.setDepthEnable( true );
    gfxc->setDepthStencilControl( dsc );
    gfxc->setupScreenViewport( 0, 0, renderTarget.getWidth(), renderTarget.getHeight(), 0.5f, 0.5f );

    gfxc->setCbControl( Gnm::kCbModeNormal, Gnm::kRasterOpCopy );
    Gnmx::renderFullScreenQuad( gfxc );


    // Restore all relevant state
    dsc.init();
    dsc.setDepthControl( Gnm::kDepthControlZWriteEnable, Gnm::kCompareFuncLess );
    dsc.setDepthEnable( true );
    gfxc->setDepthStencilControl( dsc );

    dbRenderControl.init();
    gfxc->setDbRenderControl( dbRenderControl );

    blendControl.init();
    blendControl.setBlendEnable( true );
    blendControl.setColorEquation(
        Gnm::kBlendMultiplierSrcAlpha,
        Gnm::kBlendFuncAdd,
        Gnm::kBlendMultiplierOneMinusSrcAlpha );
    gfxc->setBlendControl( 0, blendControl );
}

internal void
InitVertexBuffers( PS4RendererState* state, TexturedVertex* bufferBase, sz entriesCount )
{
    ASSERT( ARRAYCOUNT(state->vertexBufferSpecs) == ARRAYCOUNT(state->vertexBuffers) );

    // TODO Would it be better to have all this data in a single buffer?
    for( u32 i = 0; i < ARRAYCOUNT(state->vertexBufferSpecs); ++i )
    {
        PS4VertexBufferSpec& spec = state->vertexBufferSpecs[i];
        state->vertexBuffers[i].initAsVertexBuffer( (u8*)bufferBase + spec.offset,
                                                    spec.format,
                                                    spec.stride,
                                                    entriesCount );
    }
}


PLATFORM_ALLOCATE_TEXTURE(PS4AllocateTexture)
{
    Gnm::TextureSpec texSpec;
    texSpec.init();
    texSpec.m_width = width;
    texSpec.m_height = height;
    texSpec.m_numSlices = 1;
    texSpec.m_numMipLevels = 1;
    texSpec.m_numFragments = sce::Gnm::kNumFragments1;
    texSpec.m_format = Gnm::kDataFormatR8G8B8A8Unorm;   // kDataFormatR8G8B8A8UnormSrgb ?
    texSpec.m_textureType = Gnm::kTextureType2d;
    texSpec.m_tileModeHint = Gnm::kTileModeDisplay_LinearAligned;
    texSpec.m_minGpuMode = Gnm::getGpuMode();

    PS4RendererState* renderer = globalPlatformState.renderer;

    // FIXME Change signature so caller can specify where to put this
    Gnm::Texture* texture = renderer->textures + renderer->firstFreeTextureIndex++;
    int ret = texture->init( &texSpec );
    ASSERT( ret == 0 );

    Gnm::SizeAlign sa = texture->getSizeAlign();
    void* videoMemory = PUSH_SIZE_ALIGNED( &renderer->garlicArena, sa.m_size, sa.m_align );
    COPY( data, videoMemory, sa.m_size );
    texture->setBaseAddress( videoMemory );

    if( renderer->ownerHandle == 0 )
        Gnm::registerOwner( &renderer->ownerHandle, "robotrider" );

    Gnm::registerResource( nullptr, renderer->ownerHandle, texture->getBaseAddress(), sa.m_size,
                           "Texture", Gnm::kResourceTypeTextureBaseAddress, 0 );

    return texture;
}

PLATFORM_DEALLOCATE_TEXTURE(PS4DeallocateTexture)
{
    // TODO
}


PS4RendererState
PS4InitRenderer()
{
    int ret = 0;

    PS4RendererState state = {};

    // Initialize the WB_ONION memory allocator
    static const size_t kOnionMemorySize = MEGABYTES(16);
    void* onionBlock = PS4AllocAndMap( 0, kOnionMemorySize, SCE_KERNEL_WB_ONION, KILOBYTES( 64 ),
                                       SCE_KERNEL_PROT_CPU_RW | SCE_KERNEL_PROT_GPU_ALL );
    InitializeArena( &state.onionArena, (u8*)onionBlock, kOnionMemorySize );

    // Initialize the WC_GARLIC memory allocator
    // NOTE: CPU reads from GARLIC write-combined memory have a very low bandwidth so we disable them
    static const size_t kGarlicMemorySize = MEGABYTES(64 * 4);
    void* garlicBlock = PS4AllocAndMap( 0, kGarlicMemorySize, SCE_KERNEL_WC_GARLIC, KILOBYTES( 64 ),
                                        SCE_KERNEL_PROT_CPU_WRITE | SCE_KERNEL_PROT_GPU_ALL );
    InitializeArena( &state.garlicArena, (u8*)garlicBlock, kGarlicMemorySize );


#if !RELEASE
    // TODO Enable this when we find out how!
//     ret = sceGpuDebuggerEnableTargetSideSupport();
//     ASSERT( ret == 0 );
#endif
    Gnm::registerOwner( &state.ownerHandle, "robotrider" );
    // Load the shader binaries from disk
    PS4LoadShaders( &state );


    // Open the video output port
    state.videoOutHandle = sceVideoOutOpen( 0, SCE_VIDEO_OUT_BUS_TYPE_MAIN, 0, NULL );
    if( state.videoOutHandle < 0 )
    {
        LOG( ".ERROR :: sceVideoOutOpen failed: 0x%08X", state.videoOutHandle );
        INVALID_CODE_PATH
    }

    // Initialize the flip rate: 0: 60Hz, 1: 30Hz or 2: 20Hz
    ret = sceVideoOutSetFlipRate( state.videoOutHandle, 0 );
    if( ret != SCE_OK )
    {
        LOG( ".ERROR :: sceVideoOutSetFlipRate failed: 0x%08X", ret );
        INVALID_CODE_PATH
    }


    // Initialize all the render contexts
    for( u32 i = 0; i < ARRAYCOUNT(state.renderContexts); ++i )
    {
        // Calculate the size of the resource buffer for the given number of draw calls
        const u32 resourceBufferSizeInBytes =
            Gnmx::LightweightConstantUpdateEngine::computeResourceBufferSize(
                Gnmx::LightweightConstantUpdateEngine::kResourceBufferGraphics,
                100 ); // Number of batches

                       // Allocate the LCUE resource buffer memory
        state.renderContexts[i].resourceBuffer = PUSH_SIZE_ALIGNED( &state.garlicArena,
            resourceBufferSizeInBytes, Gnm::kAlignmentOfBufferInBytes );

        if( !state.renderContexts[i].resourceBuffer )
        {
            LOG( ".ERROR :: Cannot allocate the LCUE resource buffer memory" );
            INVALID_CODE_PATH
        }

        // Allocate the draw command buffer (2 MBs)
        const size_t kDcbSizeInBytes = MEGABYTES(2);
        state.renderContexts[i].dcbBuffer = PUSH_SIZE_ALIGNED( &state.onionArena,
            kDcbSizeInBytes, Gnm::kAlignmentOfBufferInBytes );

        if( !state.renderContexts[i].dcbBuffer )
        {
            LOG( ".ERROR :: Cannot allocate the draw command buffer memory" );
            INVALID_CODE_PATH
        }

        // Initialize the GfxContext used by this rendering context
        state.renderContexts[i].gfxContext.init(
            state.renderContexts[i].dcbBuffer,		// Draw command buffer address
            kDcbSizeInBytes,					    // Draw command buffer size in bytes
            state.renderContexts[i].resourceBuffer,	// Resource buffer address
            resourceBufferSizeInBytes,			    // Resource buffer address in bytes
            nullptr );								// Global resource table

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

    // Initialize all the display buffers
    for( u32 i = 0; i < ARRAYCOUNT(state.displayBuffers); ++i )
    {
        // Compute the tiling mode for the render target
        Gnm::TileMode tileMode;
        Gnm::DataFormat format = Gnm::kDataFormatB8G8R8A8UnormSrgb;
        ret = GpuAddress::computeSurfaceTileMode(
            gpuMode, // NEO or base
            &tileMode,										// Tile mode pointer
            GpuAddress::kSurfaceTypeColorTargetDisplayable,	// Surface type
            format,											// Surface format
            1 );												// Elements per pixel
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
            LOG( ".ERROR :: Cannot init render target: 0x%08X", ret );
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

    PS4DisplayBuffer *backBuffer = &state.displayBuffers[0];

    // Initialization the VideoOut buffer descriptor. The pixel format must
    // match with the render target data format, which in this case is
    // Gnm::kDataFormatB8G8R8A8UnormSrgb
    SceVideoOutBufferAttribute videoOutBufferAttribute;
    sceVideoOutSetBufferAttribute(
        &videoOutBufferAttribute,
        SCE_VIDEO_OUT_PIXEL_FORMAT_B8_G8_R8_A8_SRGB,
        SCE_VIDEO_OUT_TILING_MODE_TILE,
        SCE_VIDEO_OUT_ASPECT_RATIO_16_9,
        backBuffer->renderTarget.getWidth(),
        backBuffer->renderTarget.getHeight(),
        backBuffer->renderTarget.getPitch() );

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
        gpuMode, // NEO or Base
        &depthTileMode,									// Tile mode pointer
        GpuAddress::kSurfaceTypeDepthOnlyTarget,		// Surface type
        depthFormat,									// Surface format
        1 );												// Elements per pixel
    if( ret != SCE_OK )
    {
        LOG( ".ERROR :: GpuAddress::computeSurfaceTileMode: 0x%08X", ret );
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
        LOG( ".ERROR :: Cannot init depth target: 0x%08X", ret );
        INVALID_CODE_PATH
    }

    Gnm::SizeAlign depthTargetSizeAlign = state.depthTarget.getZSizeAlign();

    // Allocate the depth buffer
    void *depthMemory = PUSH_SIZE_ALIGNED( &state.garlicArena, depthTargetSizeAlign.m_size, depthTargetSizeAlign.m_align );
    if( !depthMemory )
    {
        LOG( ".ERROR :: Cannot allocate the depth buffer" );
        INVALID_CODE_PATH
    }
    state.depthTarget.setAddresses( depthMemory, nullptr );


    // Create the event queue for used to synchronize with end-of-pipe interrupts
    ret = sceKernelCreateEqueue( &state.eopEventQueue, "EOP QUEUE" );
    if( ret != SCE_OK )
    {
        LOG( ".ERROR :: sceKernelCreateEqueue failed: 0x%08X", ret );
        INVALID_CODE_PATH
    }

    // Register for the end-of-pipe events
    ret = Gnm::addEqEvent( state.eopEventQueue, Gnm::kEqEventGfxEop, NULL );
    if( ret != SCE_OK )
    {
        LOG( ".ERROR :: Gnm::addEqEvent failed: 0x%08X", ret );
        INVALID_CODE_PATH
    }


    // Initialize render commands and necessary buffers
    // TODO Decide a proper size for this
    u32 renderBufferSize = MEGABYTES( 4 );
    u8 *renderBuffer = (u8 *)PUSH_SIZE( &state.onionArena, renderBufferSize );

    // Init some test vertex data
    u32 maxVerticesCount = 1024*1024;
    TexturedVertex* vertexBuffer = PUSH_ARRAY_ALIGNED( &state.garlicArena, maxVerticesCount, TexturedVertex,
                                                       Gnm::kAlignmentOfBufferInBytes );
    u32 maxIndicesCount = maxVerticesCount * 8;
    u32* indexBuffer = PUSH_ARRAY_ALIGNED( &state.garlicArena, maxIndicesCount, u32, Gnm::kAlignmentOfBufferInBytes );

    state.renderCommands = InitRenderCommands( renderBuffer, renderBufferSize,
                                               vertexBuffer, maxVerticesCount,
                                               indexBuffer, maxIndicesCount );
    ASSERT( state.renderCommands.isValid );
    state.renderCommands.width = kDisplayBufferWidth;
    state.renderCommands.height = kDisplayBufferHeight;


    // Initialize the vertex buffers pointing to each vertex element
    PS4VertexBufferSpec specs[kVertexElemCount] =
    {
        { OFFSETOF(TexturedVertex, p),      sizeof(TexturedVertex), Gnm::kDataFormatR32G32B32Float },
        { OFFSETOF(TexturedVertex, color),  sizeof(TexturedVertex), Gnm::kDataFormatR8G8B8A8Unorm },
        { OFFSETOF(TexturedVertex, uv),     sizeof(TexturedVertex), Gnm::kDataFormatR32G32Float },
    };
    COPY( specs, state.vertexBufferSpecs, sizeof(specs) );
    InitVertexBuffers( &state, vertexBuffer, maxVerticesCount );

    // HACK Remove this!
    globalPlatformState.renderer = &state;


    state.white = 0xFFFFFFFF;
    state.whiteTexture = *(Gnm::Texture*)PS4AllocateTexture( &state.white, 1, 1, false );

    return state;
}

internal void
PS4UseProgram( ShaderProgramName programName, Gnmx::GnmxGfxContext* gfxc, PS4RendererState* state )
{
    if( programName == ShaderProgramName::None )
    {
        // TODO 

        state->activeProgram = nullptr;
    }
    else
    {
        if( state->activeProgram && state->activeProgram->name == programName )
            // Nothing to do
            return;

        int programIndex = -1;
        for( u32 i = 0; i < ARRAYCOUNT(globalShaderPrograms); ++i )
        {
            if( globalShaderPrograms[i].name == programName )
            {
                programIndex = i;
                break;
            }
        }

        if( programIndex == -1 )
        {
            LOG( "ERROR :: Unknown shader program [%d]", programName );
        }
        else
        {
            PS4ShaderProgram& prg = globalShaderPrograms[programIndex];

            gfxc->setActiveShaderStages( prg.activeStages );
            gfxc->setVsShader( prg.vertexShader.shader.vs, prg.vertexShader.fsModifier, prg.vertexShader.fsMemory,
                               &prg.vertexShader.shader.offsetsTable );
            gfxc->setPsShader( prg.pixelShader.ps, &prg.pixelShader.offsetsTable );

            gfxc->setVertexBuffers( Gnm::kShaderStageVs, 0, kVertexElemCount, state->vertexBuffers );

            // Allocate the vertex shader constants from the command buffer
            ShaderConstants *constants = (ShaderConstants*)gfxc->allocateFromCommandBuffer( sizeof( ShaderConstants ),
                                                                                            Gnm::kEmbeddedDataAlignment4 );

            // Initialize the vertex shader constants
            ASSERT( constants );
            constants->m_WorldViewProj = state->mCurrentProjView;

            Gnm::Buffer constBuffer;
            constBuffer.initAsConstantBuffer( constants, sizeof( ShaderConstants ) );
            gfxc->setConstantBuffers( Gnm::kShaderStageVs, 0, 1, &constBuffer );

            state->activeProgram = &prg;
        }
    }
}

void
PS4RenderToOutput( const RenderCommands &commands, PS4RendererState* state, GameMemory* gameMemory )
{
    int ret = 0;

    PS4RenderContext *renderContext = &state->renderContexts[state->renderContextIndex];
    PS4DisplayBuffer *backBuffer = &state->displayBuffers[state->backBufferIndex];

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
            LOG( ".ERROR :: sceKernelWaitEqueue failed: 0x%08X", ret );
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


    //
    // New frame setup
    //

    // Setup the viewport to match the entire screen.
    // The z-scale and z-offset values are used to specify the transformation
    // from clip-space to screen-space
    gfxc.setupScreenViewport( 0,			// Left
                              0,			// Top
                              backBuffer->renderTarget.getWidth(),
                              backBuffer->renderTarget.getHeight(),
                              0.5f,		    // Z-scale
                              0.5f );		// Z-offset

    // Bind the render & depth targets to the context
    gfxc.setRenderTarget( 0, &backBuffer->renderTarget );
    gfxc.setDepthRenderTarget( &state->depthTarget );

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

    gfxc.setLineWidth( 8 );

    Gnm::Sampler bilinearSampler;
    bilinearSampler.init();
    bilinearSampler.setXyFilterMode( Gnm::kFilterModeBilinear, Gnm::kFilterModeBilinear );
    bilinearSampler.setMipFilterMode( Gnm::kMipFilterModeLinear );
    // TODO Understand SRGB and do the right thing regarding gamma!
    bilinearSampler.setForceDegamma( true );


#if !RELEASE
    DebugState* debugState = (DebugState*)gameMemory->debugStorage;
    debugState->totalDrawCalls = 0;
    debugState->totalPrimitiveCount = 0;
    debugState->totalVertexCount = 0;
#endif

    m4 mProjView = CreatePerspectiveMatrix( (r32)commands.width / commands.height, commands.camera.fovYDeg );
    mProjView = mProjView * commands.camera.mTransform;
    state->mCurrentProjView = mProjView;


    // Process all commands in the render buffer
    const RenderBuffer &buffer = commands.renderBuffer;
    for( u32 baseAddress = 0; baseAddress < buffer.size; /**/ )
    {
        RenderEntry *entryHeader = (RenderEntry *)(buffer.base + baseAddress);

        switch( entryHeader->type )
        {
            case RenderEntryType::RenderEntryClear:
            {
                RenderEntryClear *entry = (RenderEntryClear *)entryHeader;

                // Clear the color and the depth target
                ClearRenderTarget( backBuffer->renderTarget, &gfxc, entry->color, state );
            } break;

            case RenderEntryType::RenderEntryTexturedTris:
            {
                RenderEntryTexturedTris *entry = (RenderEntryTexturedTris *)entryHeader;

                // Submit a draw call
                gfxc.setPrimitiveType( Gnm::kPrimitiveTypeTriList );
                gfxc.setIndexSize( Gnm::kIndexSize32 );
                gfxc.setIndexOffset( entry->vertexBufferOffset );
                gfxc.drawIndex( entry->indexCount, state->renderCommands.indexBuffer.base + entry->indexBufferOffset );
//                                 entry->vertexBufferOffset, 0, Gnm::kDrawModifierDefault );       // !! Requires compiling with -indirect-draw !!

#if !RELEASE
                debugState->totalDrawCalls++;
                debugState->totalVertexCount += entry->vertexCount;
                debugState->totalPrimitiveCount += (entry->indexCount / 3);
#endif
            } break;

            case RenderEntryType::RenderEntryLines:
            {
                RenderEntryLines *entry = (RenderEntryLines *)entryHeader;

//                 Gnm::PrimitiveSetup lineSetup;
//                 lineSetup.setPolygonMode( Gnm::kPrimitiveSetupPolygonModeLine, Gnm::kPrimitiveSetupPolygonModeLine );
//                 lineSetup.setCullFace( Gnm::kPrimitiveSetupCullFaceNone );
//                 gfxc.setPrimitiveSetup( lineSetup );

                // Submit a draw call
                gfxc.setPrimitiveType( Gnm::kPrimitiveTypeLineList );
                gfxc.setIndexOffset( entry->vertexBufferOffset );
                gfxc.drawIndexAuto( entry->lineCount * 2 );

#if !RELEASE
                debugState->totalDrawCalls++;
                debugState->totalPrimitiveCount += entry->lineCount;
#endif
            } break;

            case RenderEntryType::RenderEntryMaterial:
            {
                RenderEntryMaterial* entry = (RenderEntryMaterial*)entryHeader;
                Material* material = entry->material;

                Gnm::Texture* texture = material ? (Gnm::Texture*)material->diffuseMap : &state->whiteTexture;
                if( texture )
                {
                    gfxc.setTextures( Gnm::kShaderStagePs, 0, 1, texture );
                    gfxc.setSamplers( Gnm::kShaderStagePs, 0, 1, &bilinearSampler );
                }
            } break;

            case RenderEntryType::RenderEntryProgramChange:
            {
                RenderEntryProgramChange* entry = (RenderEntryProgramChange*)entryHeader;

                PS4UseProgram( entry->programName, &gfxc, state );
            } break;

            default:
            {
                LOG( "ERROR :: Unsupported RenderEntry type [%d]", entryHeader->type );
                //INVALID_CODE_PATH;
            } break;
        }

        baseAddress += entryHeader->size;
    }

    // Don't keep active program for next frame
    PS4UseProgram( ShaderProgramName::None, &gfxc, state );
}

void
PS4SwapBuffers( PS4RendererState* state )
{
    int ret = 0;

    PS4RenderContext *renderContext = &state->renderContexts[state->renderContextIndex];
    PS4DisplayBuffer *backBuffer = &state->displayBuffers[state->backBufferIndex];

    Gnmx::GnmxGfxContext &gfxc = renderContext->gfxContext;

    // Submit the command buffers, request a flip of the display buffer and
    // write the GPU label that determines the render context state (free)
    // and trigger a software interrupt to signal the EOP event queue
    // NOTE: for this basic sample we are submitting a single GfxContext
    // per frame. Submitting multiple GfxContext-s per frame is allowed.
    // Multiple contexts are processed in order, i.e.: they start in
    // submission order and end in submission order.
    ret = gfxc.submitAndFlipWithEopInterrupt( state->videoOutHandle,
                                              backBuffer->displayIndex,
                                              SCE_VIDEO_OUT_FLIP_MODE_VSYNC,
                                              0,
                                              sce::Gnm::kEopFlushCbDbCaches,
                                              const_cast<u32*>(renderContext->contextLabel),
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

        LOG( ".ERROR :: GfxContext::submitAndFlip failed: 0x%08X", ret );
    }

    // Signal the system that every draw for this frame has been submitted.
    // This function gives permission to the OS to hibernate when all the
    // currently running GPU tasks (graphics and compute) are done.
    ret = Gnm::submitDone();
    if( ret != SCE_OK )
    {
        LOG( ".ERROR :: Gnm::submitDone failed: 0x%08X", ret );
    }

    // Rotate the display buffers & contexts
    state->backBufferIndex = (state->backBufferIndex + 1) % ARRAYCOUNT(state->displayBuffers);
    state->renderContextIndex = (state->renderContextIndex + 1) % ARRAYCOUNT(state->renderContexts);
}

void
PS4ShutdownRenderer( PS4RendererState* state )
{
    int ret = 0;

    // Wait for the GPU to be idle before deallocating its resources
    for( u32 i = 0; i < ARRAYCOUNT(state->renderContexts); ++i )
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
    ret = Gnm::deleteEqEvent( state->eopEventQueue, Gnm::kEqEventGfxEop );
    if( ret != SCE_OK )
    {
        LOG( ".ERROR :: Gnm::deleteEqEvent failed: 0x%08X", ret );
    }

    // Destroy the EOP event queue
    ret = sceKernelDeleteEqueue( state->eopEventQueue );
    if( ret != SCE_OK )
    {
        LOG( ".ERROR :: sceKernelDeleteEqueue failed: 0x%08X", ret );
    }

    // Terminate the video output
    ret = sceVideoOutClose( state->videoOutHandle );
    if( ret != SCE_OK )
    {
        LOG( ".ERROR :: sceVideoOutClose failed: 0x%08X", ret );
    }

    // Releasing manually each allocated resource is not necessary as we are
    // terminating the linear allocators for ONION and GARLIC here.
    PS4Free( &state->onionArena.base );
    state->onionArena = {};
    PS4Free( &state->garlicArena.base );
    state->garlicArena = {};
}


ImGuiContext *
PS4InitImGui()
{
    // FIXME Create an arena (when we have dynamic arenas) for ImGui and pass a custom allocator/free here
    // (and don't use new for the atlas!)
    ImGuiContext *context = ImGui::CreateContext();
    ImGui::SetCurrentContext( context );

    // Build texture atlas
    ImGuiIO &io = ImGui::GetIO();
    io.Fonts = new ImFontAtlas();

    u8 *pixels;
    i32 width, height;
    // Load as RGBA 32-bits (75% of the memory is wasted, but default font is so small)
    // because it is more likely to be compatible with user's existing shaders.
    // If your ImTextureId represent a higher-level concept than just a GL texture id,
    // consider calling GetTexDataAsAlpha8() instead to save on GPU memory.
    io.Fonts->GetTexDataAsRGBA32( &pixels, &width, &height );

    // TODO 
    //

    return context;
}

void
PS4RenderImGui()
{
    ImGui::Render();
}

