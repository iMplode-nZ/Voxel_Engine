#pragma once
#include "chunk.h"
#include "block.h"
#include "camera.h"
#include "pipeline.h"

#include <set>
#include <unordered_set>
#include <atomic>
#include <stack>

typedef struct Chunk* ChunkPtr;
typedef class Level* LevelPtr;
//class ChunkLoadManager;

namespace Utils
{
	struct ChunkPtrKeyEq
	{
		bool operator()(const ChunkPtr& first, const ChunkPtr& second) const
		{
			//ASSERT(first != second);
			if (first == second)
				return false;
			glm::vec3 wposA = glm::vec3(first->GetPos() * Chunk::GetChunkSize());
			glm::vec3 wposB = glm::vec3(second->GetPos() * Chunk::GetChunkSize());
			glm::vec3 cam = Render::GetCamera()->GetPos();
			return
				glm::distance(wposA, cam) <
				glm::distance(wposB, cam);
		}
	};
}


// Interfaces (is this the correct term?) with the Chunk class to
// manage how and when chunk block and mesh data is generated, and
// when that data is sent to the GPU.
// Also manages updates to blocks and lighting in chunks to determine
// when a chunk needs to be remeshed.

// TODO: make this an abstract class, then send its current contents to a
// new class called "InfiniteChunkManager" or something like that,
// then create another called "FixedChunkManager", both derived from
// this class.
class ChunkManager
{
public:
	ChunkManager();
	~ChunkManager();
	void Init();

	// interaction
	void Update(LevelPtr level);
	void UpdateChunk(ChunkPtr chunk);
	void UpdateChunk(const glm::ivec3 wpos); // update chunk at block position
	void UpdateBlock(const glm::ivec3& wpos, Block bl);
	void UpdateBlockCheap(const glm::ivec3& wpos, Block block);
	void UpdateBlockLight(const glm::ivec3 wpos, const Light light);
	Block GetBlock(glm::ivec3 wpos); // wrapper
	Light GetLight(glm::ivec3 wpos); // wrapper
	BlockPtr GetBlockPtr(const glm::ivec3 wpos);
	LightPtr GetLightPtr(const glm::ivec3 wpos);
	void UpdatedChunk(ChunkPtr chunk);
	void ReloadAllChunks(); // for when big things change

	// getters
	float GetLoadDistance() const { return loadDistance_; }
	float GetUnloadLeniency() const { return unloadLeniency_; }

	// setters
	void SetCurrentLevel(LevelPtr level) { level_ = level; }
	void SetLoadDistance(float d) { loadDistance_ = d; }
	void SetUnloadLeniency(float d) { unloadLeniency_ = d; }

	void SaveWorld(std::string fname);
	void LoadWorld(std::string fname);

	friend class Level; // so level can display debug info
private:
	// functions
	void checkUpdateChunkNearBlock(const glm::ivec3& pos, const glm::ivec3& near);

	void removeFarChunks();
	void createNearbyChunks();

	// generates actual blocks
	void chunk_generator_thread_task();
	//std::set<ChunkPtr, Utils::ChunkPtrKeyEq> generation_queue_;
	std::unordered_set<ChunkPtr> generation_queue_;
	std::mutex chunk_generation_mutex_;
	std::vector<std::thread*> chunk_generator_threads_;

	// generates meshes for ANY UPDATED chunk
	void chunk_mesher_thread_task();
	//std::set<ChunkPtr, Utils::ChunkPtrKeyEq> mesher_queue_;
	//std::set<ChunkPtr> mesher_queue_;
	std::unordered_set<ChunkPtr> mesher_queue_;
	std::mutex chunk_mesher_mutex_;
	std::vector<std::thread*> chunk_mesher_threads_;
	std::atomic_int debug_cur_pool_left = { 0 };

	// NOT multithreaded task
	void chunk_buffer_task();
	//std::set<ChunkPtr, Utils::ChunkPtrKeyEq> buffer_queue_;
	std::unordered_set<ChunkPtr> buffer_queue_;
	std::mutex chunk_buffer_mutex_;


	// DEBUG does everything in a serial fashion
	// chunk_buffer_task must be called after this
	void chunk_gen_mesh_nobuffer();


	std::unordered_set<ChunkPtr> delayed_update_queue_;

	// new light intensity to add
	void lightPropagateAdd(glm::ivec3 wpos, Light nLight, bool skipself = true);

	// removed light intensity
	void lightPropagateRemove(glm::ivec3 wpos);

	void sunlightPropagateAdd(glm::ivec3 wpos);

	// vars
	float loadDistance_;
	float unloadLeniency_;
	std::vector<ChunkPtr> updatedChunks_;
	std::vector<ChunkPtr> genChunkList_;
	LevelPtr level_ = nullptr;
};