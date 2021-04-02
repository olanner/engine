#pragma once

struct MeshRenderCommand
{
	MeshID id;
	Mat4f transform;
};

inline bool
operator<(
	const MeshRenderCommand& left, 
	const MeshRenderCommand& right)
{
	return left.id < right.id;
}