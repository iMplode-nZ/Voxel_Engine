#include "stdafx.h"
#include "ChunkVBOAllocator.h"
#include <vbo.h>
#include "chunk.h"
#include "ChunkMesh.h"

ChunkVBOAllocator::ChunkVBOAllocator(GLsizei size)
{
	// allocate uninitialized memory in VRAM
	vbo_ = std::make_unique<VBO>(nullptr, size, GL_STATIC_DRAW);

	// make one big null allocation
	allocs_.push_back({ nullptr, 0, size, glfwGetTime() });
}


ChunkVBOAllocator::~ChunkVBOAllocator()
{
}


bool ChunkVBOAllocator::Allocate(Chunk* chunk, void* data, GLsizei size)
{
	// find smallest NULL allocation that will fit
	Iterator small = allocs_.end();
	for (int i = 0; i < allocs_.size(); i++)
	{
		if (allocs_[i].chunk == nullptr && allocs_[i].size >= size) // potential allocation
		{
			if (small == allocs_.end())// initialize small
				small = allocs_.begin() + i;
			else if (allocs_[i].size < small->size)
				small = allocs_.begin() + i;
		}
	}

	// allocation failure
	if (small == allocs_.end())
		return false;

	// split free allocation
	allocationData newAlloc;
	newAlloc.chunk = chunk;
	newAlloc.offset = small->offset;
	newAlloc.size = size;
	newAlloc.time = glfwGetTime();

	small->offset += newAlloc.size;
	small->size -= newAlloc.size;

	allocs_.insert(small, newAlloc);
	
	glNamedBufferSubData(vbo_->GetID(), newAlloc.offset, newAlloc.size, data);
	return true;
}


bool ChunkVBOAllocator::Free(Chunk* chunk)
{
	if (chunk == nullptr) return false;
	auto it = std::find_if(allocs_.begin(), allocs_.end(), [&](const auto& a) { return a.chunk == chunk; });
	if (it == allocs_.end()) // failed to free
		return false;

	it->chunk = nullptr;
	maybeMerge(it);
	return true;
}


bool ChunkVBOAllocator::FreeOldest()
{
	// find and free the oldest allocation
	Iterator old = allocs_.end();
	for (int i = 0; i < allocs_.size(); i++)
	{
		if (allocs_[i].chunk != nullptr)
		{
			if (old == allocs_.end())
				old = allocs_.begin() + i;
			else if (allocs_[i].time < old->time)
				old = allocs_.begin() + i;
		}
	}

	// failed to find old node to free
	if (old == allocs_.end())
		return false;

	old->chunk = nullptr;
	maybeMerge(old);
	return true;
}


void ChunkVBOAllocator::maybeMerge(Iterator it)
{
	bool removeIt = false;
	bool removeNext = false;
	
	// merge with next alloc
	if (it != allocs_.end() - 1)
	{
		Iterator next = it + 1;
		if (next->chunk == nullptr)
		{
			it->size += next->size;
			removeIt = true;
		}
	}

	// merge with previous alloc
	if (it != allocs_.begin())
	{
		Iterator prev = it - 1;
		if (prev->chunk == nullptr)
		{
			prev->size += it->size;
			removeNext = true;
		}
	}

	// erase merged allocations
	if (removeIt && removeNext)
		allocs_.erase(it, it + 2); // this and next
	else if (removeIt)
		allocs_.erase(it - 1);     // just this
	else if (removeNext)
		allocs_.erase(it);         // just next
}


std::vector<DrawArraysIndirectCommand> ChunkVBOAllocator::GetCommands()
{
	std::vector<DrawArraysIndirectCommand> commands;
	int baseInstance = 0;

	for (const allocationData& alloc : allocs_)
	{
		if (alloc.chunk != nullptr)
		{
			DrawArraysIndirectCommand cmd;
			cmd.count = alloc.chunk->GetMesh().GetVertexCount();
			cmd.instanceCount = 1;
			cmd.firstIndex = alloc.offset;
			cmd.baseInstance = baseInstance++;
		}
	}

	return commands;
}


std::vector<glm::vec3> ChunkVBOAllocator::GetChunkPositions()
{
	std::vector<glm::vec3> res;
	for (const auto& alloc : allocs_)
	{
		if (alloc.chunk != nullptr)
			res.push_back(alloc.chunk->GetPos());
	}
	return res;
}