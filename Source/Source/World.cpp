#include "stdafx.h"
//#include "block.h"
#include "camera.h"
#include "generation.h"
#include "World.h"
#include "pipeline.h"
#include "mesh.h"
#include "texture.h"
#include "frustum.h"
#include "transform.h"
#include "mesh_comp.h"
#include "render_data.h"
#include "chunk.h"
#include "sun.h"
#include <chrono>
#include <execution>
#include "vendor/ctpl_stl.h"
#include "input.h"
#include "pick.h"
#include "settings.h"
#include <functional>
#include "editor.h"

#include <set>
#include <memory>
#include "collision_shapes.h"
#include "ImGuiBonus.h"
#include <Engine.h>
#include <Pipeline.h>
#include "Renderer.h"
#include "Interface.h"

using namespace std::chrono;

namespace World
{
	namespace
	{
	}


	// for now this function is where we declare objects
	void World::Init()
	{
		//cameras_.push_back(new Camera(kControlCam));
		//cameras_.push_back(new Camera(CameraType::kControlCam));
		auto cam = new Camera(CameraType::kControlCam);
		cam->scrollMove = false;
		cam->SetPos({ 0, 5, 0 });
		Renderer::GetPipeline()->AddCamera(cam);

		//config = std::make_shared<btDefaultCollisionConfiguration>();
		//dispatcher = std::make_shared<btCollisionDispatcher>(config.get());
		//btInterface = std::make_shared<btSimpleBroadphase>();


		high_resolution_clock::time_point benchmark_clock_ = high_resolution_clock::now();

		chunkManager_.Init();
		WorldGen::InitNoiseFuncs();
		//Editor::level = this;
		Editor::chunkManager = &chunkManager_;
		//Editor::renderer = &renderer_;
		PrefabManager::InitPrefabs();
		BiomeManager::InitializeBiomes();
		//chunkManager_.SetCurrentLevel(this);
		chunkManager_.SetLoadDistance(200.f);
		chunkManager_.SetUnloadLeniency(100.f);
		//renderer_.Init();
		//renderer_.chunkManager_ = &chunkManager_;

		duration<double> benchmark_duration_ = duration_cast<duration<double>>(high_resolution_clock::now() - benchmark_clock_);
		//std::cout << benchmark_duration_.count() << std::endl;
		sun_ = new Sun();

		Engine::PushUpdateCallback(Update, 0);
		Engine::PushRenderCallback([] { hud_.Update(); }, 5);
	}

	void Shutdown()
	{
		for (auto& obj : objects_)
			delete obj;

		//for (auto& cam : cameras_)
		//	delete cam;
	}


	// update every object in the level
	void World::Update()
	{
		// update each camera
		if (!Interface::activeCursor)
			for (auto& cam : Renderer::GetPipeline()->GetAllCameras())
				cam->Update(Engine::GetDT());

		if (doCollisionTick)
			CheckCollision();

		chunkManager_.Update();
		CheckInteraction();
		sun_->Update();
		Renderer::SetDirLight(&sun_->GetDirLight());
		Renderer::SetSun(sun_);
		
		//Renderer::DrawAll();
		Editor::Update();
		//hud_.Update();
		//DrawImGui();
	}




	// TODO: move this into own file for physics/collision or something, just don't have it here
	void World::CheckCollision()
	{
		auto cam = Renderer::GetPipeline()->GetCamera(0);
		ImGui::Begin("Collision");
		Box camBox(*cam);
		auto min = glm::ivec3(glm::floor(camBox.min));
		auto max = glm::ivec3(glm::ceil(camBox.max));
		auto mapcomp = [&camBox](const Box& a, const Box& b)->bool
		{
			return glm::distance(camBox.GetPosition(), a.GetPosition()) < glm::distance(camBox.GetPosition(), b.GetPosition());
		};
		std::set<Box, decltype(mapcomp)> blocks(mapcomp);
		for (int x = min.x; x < max.x; x++)
		{
			for (int y = min.y; y < max.y; y++)
			{
				for (int z = min.z; z < max.z; z++)
				{
					Box block({ x, y, z });
					blocks.insert(block);
					ImGui::Text("Checking (%d, %d, %d)", x, y, z);
				}
			}
		}
		for (auto& blockBox : blocks)
		{
			auto pos = blockBox.blockpos;
			if (GetBlockAt(pos).GetType() != BlockType::bAir)
			{
				// the normal of each face.
				constexpr glm::vec3 faces[6] =
				{
					{-1, 0, 0 }, // 'left' face normal (-x direction)
					{ 1, 0, 0 }, // 'right' face normal (+x direction)
					{ 0, 1, 0 }, // 'bottom' face normal (-y direction)
					{ 0,-1, 0 }, // 'top' face normal (+y direction)
					{ 0, 0,-1 }, // 'far' face normal (-z direction)
					{ 0, 0, 1 }  // 'near' face normal (+z direction)
				};

				// distance of collided box to the face.
				float distances[6] =
				{
					(blockBox.max.x - camBox.min.x), // distance of box 'b' to face on 'left' side of 'a'.
					(camBox.max.x - blockBox.min.x), // distance of box 'b' to face on 'right' side of 'a'.
					(camBox.max.y - blockBox.min.y), // distance of box 'b' to face on 'bottom' side of 'a'.
					(blockBox.max.y - camBox.min.y), // distance of box 'b' to face on 'top' side of 'a'.
					(blockBox.max.z - camBox.min.z), // distance of box 'b' to face on 'far' side of 'a'.
					(camBox.max.z - blockBox.min.z), // distance of box 'b' to face on 'near' side of 'a'.
				};

				//https://www.gamedev.net/forums/topic/567310-platform-game-collision-detection/
				int collidedFace;
				float collisionDepth = std::numeric_limits<float>::max();
				glm::vec3 collisionNormal(0);
				// scan each face, make sure the box intersects,
				// and take the face with least amount of intersection
				// as the collided face.
				std::string sfaces[] = { "left", "right", "bottom", "top", "far", "near" };
				for (int i = 0; i < 6; i++)
				{
					//// box does not intersect face. So boxes don't intersect at all.
					//if (distances[i] < 0.0f)
					//	return false;

					// face of least intersection depth. That's our candidate.
					if ((i == 0) || (distances[i] < collisionDepth))
					{
						collidedFace = i;
						collisionNormal = faces[i];
						collisionDepth = distances[i];
					}
				}
				int normalComp = collisionNormal.x ? 0 : collisionNormal.y ? 1 : 2;
				//auto newPos = cam->oldPos;
				auto refl = glm::normalize(glm::reflect(glm::normalize(cam->GetPos() - cam->oldPos + .000001f), -collisionNormal));
				//refl[normalComp] /= 2;
				glm::vec3 newPos = cam->GetPos();
				if (GetBlockAt(pos).GetType() == BlockType::bWater)
				{
					cam->velocity_.y *= glm::pow(.987f, Engine::GetDT() * 100);
					if (Input::Keyboard().down[GLFW_KEY_SPACE])
						cam->velocity_.y += 8.f * Engine::GetDT();
					else
						cam->velocity_.y += 1.5f * Engine::GetDT();
					cam->velocity_.y = glm::clamp(cam->velocity_.y, -2000.f, 5.f);
				}
				else
				{
					newPos = cam->GetPos() + collisionDepth * -collisionNormal * 1.000f;
					ImGui::Text("%s", sfaces[collidedFace].c_str());
					ImGui::Text("Reflection: (%.2f, %.2f, %.2f)", refl.x, refl.y, refl.z);
					cam->velocity_[normalComp] = 0;
				}
				cam->SetPos(newPos);
				camBox = Box(*cam);
			}
		}
		//ImGui::NewLine();
		ImGui::End();
	}


	void World::CheckInteraction()
	{
		checkBlockPlacement();
		checkBlockDestruction();
		checkBlockPick();
	}


	// force updates a block in a location
	void World::UpdateBlockAt(glm::ivec3 wpos, Block bl)
	{
		Block block = bl;
		block.SetWriteStrength(0xf);
		chunkManager_.UpdateBlock(wpos, block);
	}


	// updates a block in a location IFF the new block has a sufficiently high write strength
	void World::GenerateBlockAt(glm::ivec3 wpos, Block b)
	{
		chunkManager_.UpdateBlock(wpos, b);
	}


	void World::GenerateBlockAtCheap(glm::ivec3 wpos, Block b)
	{
		chunkManager_.UpdateBlockCheap(wpos, b);
	}


	Block World::GetBlockAt(glm::ivec3 wpos)
	{
		return chunkManager_.GetBlock(wpos);
	}


	void World::checkBlockPlacement()
	{
		if (Input::Mouse().pressed[GLFW_MOUSE_BUTTON_2])
		{
			raycast(
				Renderer::GetPipeline()->GetCamera(0)->GetPos(),
				Renderer::GetPipeline()->GetCamera(0)->front,
				5,
				std::function<bool(glm::vec3, BlockPtr, glm::vec3)>
				([&](glm::vec3 pos, BlockPtr block, glm::vec3 side)->bool
			{
				if (!block || block->GetType() == BlockType::bAir)
					return false;

				UpdateBlockAt(pos + side, hud_.GetSelected());

				return true;
			}
			));
		}
	}


	void World::checkBlockDestruction()
	{
		if (Input::Mouse().pressed[GLFW_MOUSE_BUTTON_1] &&
			!ImGui::IsAnyItemHovered() &&
			!ImGui::IsAnyItemActive() &&
			!ImGui::IsAnyItemFocused())
		{
			raycast(
				Renderer::GetPipeline()->GetCamera(0)->GetPos(),
				Renderer::GetPipeline()->GetCamera(0)->front,
				5,
				std::function<bool(glm::vec3, BlockPtr, glm::vec3)>
				([&](glm::vec3 pos, BlockPtr block, glm::vec3 side)->bool
			{
				if (!block || block->GetType() == BlockType::bAir)
					return false;

				UpdateBlockAt(pos, BlockType::bAir);

				return true;
			}
			));
		}
	}


	void World::checkBlockPick()
	{
		if (Input::Mouse().pressed[GLFW_MOUSE_BUTTON_3] &&
			!ImGui::IsAnyItemHovered() &&
			!ImGui::IsAnyItemActive() &&
			!ImGui::IsAnyItemFocused())
		{
			raycast(
				Renderer::GetPipeline()->GetCamera(0)->GetPos(),
				Renderer::GetPipeline()->GetCamera(0)->front,
				5,
				std::function<bool(glm::vec3, BlockPtr, glm::vec3)>
				([&](glm::vec3 pos, BlockPtr block, glm::vec3 side)->bool
			{
				if (!block || block->GetType() == BlockType::bAir)
					return false;

				hud_.SetSelected(block->GetType());

				return true;
			}
			));
		}
	}
}