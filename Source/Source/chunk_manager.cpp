#include "stdafx.h"
#include "chunk_load_manager.h"
#include "chunk.h"
#include "level.h"
#include "chunk_manager.h"
#include "generation.h"
#include "pipeline.h"

#include <algorithm>
#include <execution>

ChunkManager::ChunkManager()
{
	Chunk::chunks.max_load_factor(0.7f);
	loadDistance_ = 0;
	unloadLeniency_ = 0;
	maxLoadPerFrame_ = 0;
	loadManager_ = new ChunkLoadManager();
	//updatedChunks_.reserve(2000); // big number
}

void ChunkManager::Update(LevelPtr level)
{
	std::for_each(
		std::execution::par,
		Chunk::chunks.begin(),
		Chunk::chunks.end(),
		[](auto& p)
	{
		if (p.second)
			p.second->Update();
	});

	ProcessUpdatedChunks();
	createNearbyChunks();
	generateNewChunks();
}

void ChunkManager::UpdateBlock(glm::ivec3& wpos, Block::BlockType t, unsigned char writeStrength)
{
	localpos p = Chunk::worldBlockToLocalPos(wpos);
	BlockPtr block = Chunk::AtWorld(wpos);
	ChunkPtr chunk = Chunk::chunks[p.chunk_pos];

	if (block)
	{
		// write policy: skip if new block is WEAKER than current block (same strength WILL overwrite)
		if (writeStrength < block->WriteStrength())
			return;
	}

	// create empty chunk if it's null
	if (!chunk)
	{
		Chunk::chunks[p.chunk_pos] = chunk = new Chunk(true);
		chunk->SetPos(p.chunk_pos);
		chunk->generate_ = true;
	}

	if (!block) // reset block if it's invalid
		block = &chunk->At(p.block_pos);
	block->SetType(t, writeStrength);

	// add to update list if it ain't
	if (!isChunkInUpdateList(chunk))
		updatedChunks_.push_back(chunk);

	// check if adjacent to opaque blocks in nearby chunks, then update those chunks if it is
	glm::ivec3 dirs[] =
	{
		{ -1,  0,  0 },
		{  1,  0,  0 },
		{  0, -1,  0 },
		{  0,  1,  0 },
		{  0,  0, -1 },
		{  0,  0,  1 }

		// TODO: add 8 more cases for diagonals (AO)
	};
	for (const auto& dir : dirs)
	{
		checkUpdateChunkNearBlock(wpos, dir);
	}
}

// perform no checks, therefore the chunk must be known prior to placing the block
void ChunkManager::UpdateBlockCheap(glm::ivec3& wpos, Block block)
{
	*Chunk::AtWorld(wpos) = block;
	//UpdatedChunk(Chunk::chunks[Chunk::worldBlockToLocalPos(wpos).chunk_pos]);
}

Block ChunkManager::GetBlock(glm::ivec3 wpos)
{
	BlockPtr block = Chunk::AtWorld(wpos);
	if (!block)
		return Block();
	return *block;
}

void ChunkManager::UpdatedChunk(ChunkPtr chunk)
{
	if (isChunkInUpdateList(chunk))
		updatedChunks_.push_back(chunk);
}

void ChunkManager::ReloadAllChunks()
{
	for (const auto& p : Chunk::chunks)
	{
		if (p.second)
			if (!isChunkInUpdateList(p.second))
				updatedChunks_.push_back(p.second);
	}
}

void ChunkManager::ProcessUpdatedChunks()
{
	std::for_each(
		std::execution::par,
		updatedChunks_.begin(),
		updatedChunks_.end(),
		[](ChunkPtr& chunk)
	{
		if (chunk && !chunk->NeedsLoading())
			chunk->BuildMesh();
	});

	// this operation cannot be parallelized
	std::for_each(
		std::execution::seq,
		updatedChunks_.begin(),
		updatedChunks_.end(),
		[](ChunkPtr& chunk)
	{
		if (chunk && !chunk->NeedsLoading())
			chunk->BuildBuffers();
	});

	updatedChunks_.clear();
}

bool ChunkManager::isChunkInUpdateList(ChunkPtr c)
{
	for (auto& chunk : updatedChunks_)
	{
		if (chunk == c)
			return true;
	}
	return false;
}

void ChunkManager::checkUpdateChunkNearBlock(const glm::ivec3& pos, const glm::ivec3& near)
{
	localpos p1 = Chunk::worldBlockToLocalPos(pos);
	localpos p2 = Chunk::worldBlockToLocalPos(pos + near);
	if (p1.chunk_pos == p2.chunk_pos)
		return;

	// update chunk if near block is NOT air/invisible
	BlockPtr cb = Chunk::AtWorld(pos);
	BlockPtr nb = Chunk::AtWorld(pos + near);
	if (cb && nb && nb->GetType() != Block::bAir)
		if (!isChunkInUpdateList(Chunk::chunks[p2.chunk_pos]))
			updatedChunks_.push_back(Chunk::chunks[p2.chunk_pos]);
}

void ChunkManager::createNearbyChunks()
{
	std::vector<ChunkPtr> deleteList;
	Utils::erase_if(
		Chunk::chunks,
		[&](auto& p)->bool
	{
		float dist = glm::distance(glm::vec3(p.first * Chunk::CHUNK_SIZE), Render::GetCamera()->GetPos());
		if (p.second && dist > loadDistance_ + unloadLeniency_)
		{
			deleteList.push_back(p.second);
			return true;
		}
		return false;
	});

	for (ChunkPtr p : deleteList)
		delete p;

	// delete far away chunks, then create chunks that are close
	std::for_each(
		std::execution::seq,
		Chunk::chunks.begin(),
		Chunk::chunks.end(),
		[&](auto& p)
	{
		float dist = glm::distance(glm::vec3(p.first * Chunk::CHUNK_SIZE), Render::GetCamera()->GetPos());
		// generate null chunks within distance
		if (!p.second && dist <= loadDistance_)
		{
			p.second = new Chunk(true);
			p.second->SetPos(p.first);
			p.second->generate_ = true;
		}
	});
}

void ChunkManager::generateNewChunks()
{
//	std::for_each(
//	Chunk::chunks.begin(),
//	Chunk::chunks.end(),
//	[&](auto& p)
//{
//	float dist = glm::distance(glm::vec3(p.first * Chunk::CHUNK_SIZE), Render::GetCamera()->GetPos());
//	if (p.second && dist <= loadDistance_)
//		if (p.second->generate_)
//			genChunkList_.push_back(p.second);
//});
//
//	// sort chunks to update by distance from camera
//	std::sort(
//	std::execution::par,
//	genChunkList_.begin(),
//	genChunkList_.end(),
//	[&](ChunkPtr& a, ChunkPtr& b)->bool
//{
//	return glm::distance(Render::GetCamera()->GetPos(), glm::vec3(a->GetPos() * Chunk::CHUNK_SIZE)) <
//				 glm::distance(Render::GetCamera()->GetPos(), glm::vec3(b->GetPos() * Chunk::CHUNK_SIZE));
//});

	//genChunkList_.erase(genChunkList_.begin(), genChunkList_.begin() + maxLoadPerFrame_);
	//genChunkList_.clear();

#if USE_MULTITHREADED_CHUNK_LOADER
	// version 2.0
	for (auto& p : Chunk::chunks)
	{
		float dist = glm::distance(glm::vec3(p.first * Chunk::CHUNK_SIZE), Render::GetCamera()->GetPos());
		if (p.second && !p.second->InLoadQueue() && p.second->NeedsLoading())
			if (dist <= loadDistance_)
				loadManager_->Push(p.second);
	}
#else
	// generate each chunk that needs to be generated
	unsigned maxgen = maxLoadPerFrame_;
	std::for_each(
		std::execution::seq,
		Chunk::chunks.begin(),
		Chunk::chunks.end(),
		[&](auto& p)
	{
		float dist = glm::distance(glm::vec3(p.first * Chunk::CHUNK_SIZE), Render::GetCamera()->GetPos());
		ChunkPtr chunk = p.second;
		if (chunk && maxgen > 0 && dist <= loadDistance_)
		{
			if (chunk->generate_)
			{
#if MARCHED_CUBES
				WorldGen::Generate3DNoiseChunk(chunk->GetPos(), level_);
#else
				WorldGen::GenerateChunk(chunk->GetPos(), level_);
#endif
				chunk->SetGenerate(false);
				chunk->SetLoaded(true);
				chunk->SetIsLoading(false);
				maxgen--;
			}
			chunk->Update();
		}
			});
#endif
}
