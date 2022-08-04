#pragma once

// NUMBERS
constexpr int	NumSwapchainImages = 3;
constexpr char	FirstFontGlyph = ' ';
constexpr char	LastFontGlyph = '~';
static_assert(FirstFontGlyph < LastFontGlyph);
constexpr int	NumGlyphsPerFont = LastFontGlyph + 1 - FirstFontGlyph;

// INDICES
//	SHADER SET BINDINGS
constexpr int SamplerSetSamplersBinding = 0;

constexpr int ImageSetImages2DBinding = 0;
constexpr int ImageSetImagesCubeBinding = 1;
constexpr int ImageSetImagesStorageBinding = 2;

constexpr int GlobalsSetBinding = 0;

// LIMITS
//	OBJECTS
constexpr int	MaxNumMeshesLoaded = 512;
constexpr int	MaxNumImages = 1024;
constexpr int	MaxNumImagesCube = 128;
constexpr int	MaxNumStorageImages = 8;
constexpr int	MaxNumSamplers = 1;
constexpr int	MaxNumUniforms = 128;
constexpr int	MaxNumFonts = 128;

constexpr int	MaxNumShaderModulesPerShader = 8;

//	FRAME VALID
constexpr int	MaxNumInstances = 512;
constexpr int	MaxNumInstanceStructures = 8;
constexpr int	MaxNumSpriteInstances = 1024;

constexpr int	MaxNumTransfers = 128;
constexpr int	MaxNumImmediateTransfers = 32;

