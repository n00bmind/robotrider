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
 

internal const u32 kDisplayBufferWidth = 1920;
internal const u32 kDisplayBufferHeight = 1080;



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
        Gnmx::generateInputOffsetsCache( offsetsTable, Gnm::kShaderStagePs, result );

    Gnm::registerResource( nullptr, ownerHandle, result->getBaseAddress(), shaderInfo.m_gpuShaderCodeSize,
                           "PsShader", Gnm::kResourceTypeShaderBaseAddress, 0 );

    return result;
}

internal Gnmx::VsShader*
LoadVSFromFile( const char *path, PS4RendererState* state, Gnmx::InputOffsetsCache* offsetsTable = nullptr )
{
    Gnmx::VsShader *result = nullptr;

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
        result = InitializeVSWithAllocators( readFile.contents, &state->onionArena, &state->garlicArena,
                                             state->ownerHandle, offsetsTable );
        globalPlatform.DEBUGFreeFileMemory( readFile.contents );
    }
    else
        INVALID_CODE_PATH

    return result;
}

internal Gnmx::PsShader*
LoadPSFromFile( const char *path, PS4RendererState* state, Gnmx::InputOffsetsCache* offsetsTable = nullptr )
{
    Gnmx::PsShader *result = nullptr;

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
        result = InitializePSWithAllocators( readFile.contents, &state->onionArena, &state->garlicArena,
                                             state->ownerHandle, offsetsTable );
        globalPlatform.DEBUGFreeFileMemory( readFile.contents );
    }
    else
        INVALID_CODE_PATH

    return result;
}

internal void
PS4LoadShaders( PS4RendererState* state )
{
    const char* filename = nullptr;

    filename = "default_vv.sb";
    state->testVS = LoadVSFromFile( filename, state );
    if( !state->testVS )
    {
        LOG( ".ERROR :: Couldn't load shader %s", filename );
        INVALID_CODE_PATH
    }

    filename = "plain_color_p.sb";
    state->testPS = LoadPSFromFile( filename, state );
    if( !state->testPS )
    {
        LOG( ".ERROR :: Couldn't load shader %s", filename );
        INVALID_CODE_PATH
    }

    filename = "clear_p.sb";
    state->clearPS = LoadPSFromFile( filename, state, &state->clearOffsetsTable );
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
    // TODO Shouldn't we be doing this in the load function instead?
    Gnmx::generateInputOffsetsCache( &state->vsOffsetsTable, Gnm::kShaderStageVs, state->testVS );
    Gnmx::generateInputOffsetsCache( &state->psOffsetsTable, Gnm::kShaderStagePs, state->testPS );
}


// TODO This needs more testing
// In particular, docs advise to "Draw a full-screen quad at maximum depth with the depth-test feature disabled".
// We're disabling the depth-test (while still writing to the z buffer), but since the full-screen quad is "embedded"
// in Gnm, we have no way of knowing at which depth it's being rendered, so test against geometry with very "deep" Z coords.
internal void
ClearRenderTarget( const Gnm::RenderTarget& renderTarget, sce::Gnmx::GnmxGfxContext* gfxc, const v4 &color,
                   PS4RendererState* state )
{
    // TODO Most of this stuff is done as part of our default config and is probably duplicated
    gfxc->setRenderTarget( 0, &renderTarget );
    gfxc->setDepthRenderTarget( nullptr );
    gfxc->setPsShader( state->clearPS, &state->clearOffsetsTable );

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
    Gnmx::renderFullScreenQuad( gfxc );
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
    u32 vertexBufferSize = 1024*1024;
    TexturedVertex* vertexBuffer = PUSH_ARRAY_ALIGNED( &state.garlicArena, vertexBufferSize, TexturedVertex,
                                                       Gnm::kAlignmentOfBufferInBytes );
    u32 indexBufferSize = vertexBufferSize * 8;
    u32* indexBuffer = PUSH_ARRAY_ALIGNED( &state.garlicArena, indexBufferSize, u32, Gnm::kAlignmentOfBufferInBytes );

    state.renderCommands = InitRenderCommands( renderBuffer, renderBufferSize,
                                               vertexBuffer, vertexBufferSize,
                                               indexBuffer, indexBufferSize );
    ASSERT( state.renderCommands.isValid );
    state.renderCommands.width = kDisplayBufferWidth;
    state.renderCommands.height = kDisplayBufferHeight;


    // Initialize the vertex buffers pointing to each vertex element
    state.vertexBufferDescriptors[kVertexPosition].initAsVertexBuffer( &vertexBuffer->p,
                                                                       Gnm::kDataFormatR32G32B32Float,
                                                                       sizeof( TexturedVertex ),
                                                                       vertexBufferSize );

    state.vertexBufferDescriptors[kVertexColor].initAsVertexBuffer( &vertexBuffer->color,
                                                                    Gnm::kDataFormatR8G8B8A8Unorm,
                                                                    sizeof( TexturedVertex ),
                                                                    vertexBufferSize );

    state.vertexBufferDescriptors[kVertexUv].initAsVertexBuffer( &vertexBuffer->uv,
                                                                 Gnm::kDataFormatR32G32Float,
                                                                 sizeof( TexturedVertex ),
                                                                 vertexBufferSize );

    return state;
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
    // FIXME We disable the depth target in the ClearRenderTarget call, so look into that!
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


#if !RELEASE
    DebugState* debugState = (DebugState*)gameMemory->debugStorage;
    debugState->totalDrawCalls = 0;
    debugState->totalPrimitiveCount = 0;
    debugState->totalVertexCount = 0;
#endif

    m4 mProjView = CreatePerspectiveMatrix( (r32)commands.width / commands.height, commands.camera.fovYDeg );
    mProjView = mProjView * commands.camera.mTransform;


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

            case RenderEntryType::RenderEntryLines:
            {
                RenderEntryLines *entry = (RenderEntryLines *)entryHeader;

#if 1
                Gnm::PrimitiveSetup lineSetup;
                lineSetup.setPolygonMode( Gnm::kPrimitiveSetupPolygonModeLine, Gnm::kPrimitiveSetupPolygonModeLine );
                lineSetup.setCullFace( Gnm::kPrimitiveSetupCullFaceNone );
                gfxc.setPrimitiveSetup( lineSetup );
                gfxc.setLineWidth( 8 );

                gfxc.setRenderTarget( 0, &backBuffer->renderTarget );
                gfxc.setDepthRenderTarget( nullptr );

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
                gfxc.setVertexBuffers( Gnm::kShaderStageVs, 0, kVertexElemCount, state->vertexBufferDescriptors );

                // Allocate the vertex shader constants from the command buffer
                ShaderConstants *constants = (ShaderConstants*)gfxc.allocateFromCommandBuffer( sizeof(ShaderConstants),
                                                                                               Gnm::kEmbeddedDataAlignment4 );

                // Initialize the vertex shader constants
                ASSERT( constants );
                constants->m_WorldViewProj = mProjView;

                Gnm::Buffer constBuffer;
                constBuffer.initAsConstantBuffer( constants, sizeof( ShaderConstants ) );
                gfxc.setConstantBuffers( Gnm::kShaderStageVs, 0, 1, &constBuffer );


                // Submit a draw call
                gfxc.setPrimitiveType( Gnm::kPrimitiveTypeLineList );
                gfxc.drawIndexAuto( entry->lineCount * 2, entry->vertexBufferOffset, 0, Gnm::kDrawModifierDefault );

                v4 vertices[1024][2];
                for( int i = 0; i < entry->lineCount; ++i )
                {
                    vertices[i][0] = mProjView * V4( state->renderCommands.vertexBuffer.base[ entry->vertexBufferOffset + 2 * i ].p, 1 );
                    vertices[i][0] = vertices[i][0] / vertices[i][0].w;
                    vertices[i][1] = mProjView * V4( state->renderCommands.vertexBuffer.base[ entry->vertexBufferOffset + 2 * i + 1 ].p, 1 );
                    vertices[i][1] = vertices[i][1] / vertices[i][1].w;
                }
#endif

#if !RELEASE
                debugState->totalDrawCalls++;
                debugState->totalPrimitiveCount += entry->lineCount;
#endif
            } break;

            default:
            {
                LOG( "ERROR :: Unsupported RenderEntry type [%d]", entryHeader->type );
                //INVALID_CODE_PATH;
            } break;
        }

        baseAddress += entryHeader->size;
    }

#if 0
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
    gfxc.setVertexBuffers( Gnm::kShaderStageVs, 0, kVertexElemCount, state->vertexBufferDescriptors );

    // Allocate the vertex shader constants from the command buffer
    ShaderConstants *constants = (ShaderConstants*)gfxc.allocateFromCommandBuffer( sizeof(ShaderConstants),
                                                                                   Gnm::kEmbeddedDataAlignment4 );

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
        LOG( ".ERROR :: Cannot allocate vertex shader constants" );
    }


    // Submit a draw call
    gfxc.setPrimitiveType( Gnm::kPrimitiveTypeTriList );
    gfxc.setIndexSize( Gnm::kIndexSize32 );
    gfxc.drawIndex( ARRAYCOUNT(kIndexData), state->renderCommands.indexBuffer.base );
#endif
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
