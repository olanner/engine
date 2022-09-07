
#pragma once

typedef uint32_t	QueueFamilyIndex;
typedef uint32_t	MemTypeIndex;

struct PixelValue
{
	uint8_t r,g,b,a;
};

enum class MeshID;
enum class ImageID;
enum class CubeID;
enum class UniformID;
enum class GeoStructID;
enum class InstanceStructID;
enum class FontID;
enum class AllocationSubmissionID;

enum QueueFamilyType
{
	QUEUE_FAMILY_GRAPHICS,
	QUEUE_FAMILY_COMPUTE,
	QUEUE_FAMILY_TRANSFER,
	QUEUE_FAMILY_COUNT,
};
using QueueFamilyIndices = std::array<QueueFamilyIndex, QUEUE_FAMILY_COUNT>;