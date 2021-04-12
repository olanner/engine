#pragma once

// NUMBERS
constexpr uint32_t	NumSwapchainImages = 3;
constexpr char		FirstFontGlyph = ' ';
constexpr char		LastFontGlyph = '~';
static_assert(FirstFontGlyph < LastFontGlyph);
constexpr uint32_t	NumGlyphsPerFont = LastFontGlyph + 1 - FirstFontGlyph;

// INDICES
//	SHADER SET BINDINGS
constexpr uint32_t SamplerSetSamplersBinding = 0;

constexpr uint32_t ImageSetImages2DBinding = 0;
constexpr uint32_t ImageSetImages2DArrayBinding = 1;
constexpr uint32_t ImageSetImagesCubeBinding = 2;
constexpr uint32_t ImageSetImagesStorageBinding = 3;

constexpr uint32_t GlobalsSetBinding = 0;

// LIMITS
//	OBJECTS
constexpr uint32_t	MaxNumMeshesLoaded = 512;
constexpr uint32_t	MaxNumImages2D = 8192;
constexpr uint32_t	MaxNumImages2DArray = 8192;
constexpr uint32_t	MaxNumImagesCube = 128;
constexpr uint32_t	MaxNumStorageImages = 8;
constexpr uint32_t	MaxNumSamplers = 1;
constexpr uint32_t	MaxNumUniforms = 128;
constexpr uint32_t	MaxNumFonts = 128;

constexpr uint32_t	MaxNumShaderModulesPerShader = 8;

//	FRAME VALID
constexpr uint32_t	MaxNumInstances = 512;
constexpr uint32_t	MaxNumInstanceStructures = 8;
constexpr uint32_t	MaxNumGlyphInstances = 1024;

constexpr uint32_t	MaxNumTransfers = 128;
constexpr uint32_t	MaxNumImmediateTransfers = 32;

