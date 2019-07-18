#include "stdafx.h"
#include "vbo.h"
#include "vao.h"
#include "ibo.h"
#include "chunk.h"
#include "block.h"
#include "camera.h"
#include "shader.h"
#include "pipeline.h"
#include <mutex>
#include <sstream>

/*
	TODO: finish making buildBlockVertices() take block color OR do the thing below
	TODO: additional buffers for colors, normals, and other data that is same between vertices
	TODO: use IBOs to save GPU memory
*/

Concurrency::concurrent_unordered_map<glm::ivec3, Chunk*, Chunk::ivec3Hash> Chunk::chunks;

static std::mutex mtx;

Chunk::Chunk(bool active) : active_(active)
{
	vao_ = new VAO();

	float r = Utils::get_random(0, 1);
	float g = Utils::get_random(0, 1);
	float b = Utils::get_random(0, 1);
	colorTEMP = glm::vec4(r, g, b, 1.f);
}

Chunk::~Chunk()
{
	if (vao_)
		delete vao_;
	if (vbo_)
		delete vbo_;
}

void Chunk::Update()
{
	// build mesh after ensuring culled blocks are culled
	buildMesh();
}

void Chunk::Render()
{
	if (vbo_ && vao_)
	{
		vao_->Bind();
		vbo_->Bind();
		Chunk::chunks;
		glDrawArrays(GL_TRIANGLES, 0, vertexCount_);
	}
}

void Chunk::BuildBuffers()
{
	vao_->Bind();
	if (vbo_)
		delete vbo_;
	//if (ibo_)
	//	delete ibo_;
	//ibo_ = new IBO(&indices[0], indices.size());
	vbo_ = new VBO(&vertices[0], sizeof(float) * vertices.size(), GL_STATIC_DRAW);
	vbo_->Bind();
	vertexCount_ = vertices.size() / 10; // divisor = number of floats per vertex

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 10, (void*)0); // screenpos
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 10, (void*)(sizeof(float) * 3)); // color
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 10, (void*)(sizeof(float) * 6)); // normal
	glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(float) * 10, (void*)(sizeof(float) * 9)); // shininess
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);
	glEnableVertexAttribArray(3);
	glVertexAttribDivisor(0, 0);
	glVertexAttribDivisor(1, 0);
	glVertexAttribDivisor(2, 0);
	glVertexAttribDivisor(3, 0);
	vao_->Unbind();
	vbo_->Unbind();
	vertices.clear();
}

void Chunk::buildMesh()
{
	//Camera* cam = Render::GetCamera();

	vertices.reserve(CHUNK_SIZE * CHUNK_SIZE * 6 * 3); // one entire side of a chunk (assumed flat)
	for (int x = 0; x < CHUNK_SIZE; x++)
	{
		for (int y = 0; y < CHUNK_SIZE; y++)
		{
			for (int z = 0; z < CHUNK_SIZE; z++)
			{
				// skip fully transparent blocks
				if (blocks[ID3D(x, y, z, CHUNK_SIZE, CHUNK_SIZE)].GetType() == Block::bAir)
					continue;

				// check if each face would be obscured, and adds the ones that aren't to the vbo
				// obscured IF side is adjacent to opaque block
				// NOT obscured if side is adjacent to nothing or transparent block
				glm::ivec3 pos(x, y, z);
				buildBlockVertices(pos, Render::cube_norm_tex_vertices, 48);
			}
		}
	}
	// "buildbuffers" would normally happen here
	// 'vertices' is stored until "buildbuffers" is called
}

void Chunk::buildBlockVertices(const glm::ivec3 & pos, const float * data, int quadStride)
{
	std::vector<float> temp;
	int x = pos.x;
	int y = pos.y;
	int z = pos.z;

	// back
	temp = buildSingleBlockFace(glm::vec3(x, y, z - 1), quadStride, 0, data, pos);
	vertices.insert(vertices.end(), temp.begin(), temp.end());

	// front
	temp = buildSingleBlockFace(glm::ivec3(x, y, z + 1), quadStride, 1, data, pos);
	vertices.insert(vertices.end(), temp.begin(), temp.end());

	// left
	temp = buildSingleBlockFace(glm::ivec3(x - 1, y, z), quadStride, 2, data, pos);
	vertices.insert(vertices.end(), temp.begin(), temp.end());

	// right
	temp = buildSingleBlockFace(glm::ivec3(x + 1, y, z), quadStride, 3, data, pos);
	vertices.insert(vertices.end(), temp.begin(), temp.end());

	// bottom
	temp = buildSingleBlockFace(glm::ivec3(x, y - 1, z), quadStride, 4, data, pos);
	vertices.insert(vertices.end(), temp.begin(), temp.end());

	// top
	temp = buildSingleBlockFace(glm::ivec3(x, y + 1, z), quadStride, 5, data, pos);
	vertices.insert(vertices.end(), temp.begin(), temp.end());
}

//std::vector<float> Chunk::buildSingleBlockFace(glm::ivec3 near, int low, int high, int x, int y, int z)
std::vector<float> Chunk::buildSingleBlockFace(
	const glm::ivec3& nearFace,											// position of nearby block to check
	int quadStride, int curQuad, const float* data, // vertex + quad data
	const glm::ivec3& blockPos)											// position of current block
{
	std::vector<float> quad;
	quad.reserve(6 * 6);
	
	localpos nearblock = worldBlockToLocalPos(chunkBlockToWorldPos(nearFace));

	// critical section (probably)
	//std::unique_lock<std::mutex> lck(mtx, std::defer_lock);
	//lck.lock();
	if (!chunks[nearblock.chunk_pos])
		goto GenQuad;
	if (!chunks[nearblock.chunk_pos]->active_)
		goto GenQuad;
	if (chunks[nearblock.chunk_pos]->At(nearblock.block_pos).GetType() != Block::bAir)
		return quad;
	//lck.unlock();

GenQuad:
	// transform the vertices relative to the chunk
	// (the full world transformation will be completed in a shader)
	//glm::mat4 localTransform = glm::translate(glm::mat4(1.f),

	//	Utils::mapToRange(glm::vec3(blockPos), 0.f, (float)CHUNK_SIZE, -(float)CHUNK_SIZE / 2.0f, (float)CHUNK_SIZE/ 2.0f)); // scaled
	glm::mat4 localTransform = glm::translate(glm::mat4(1.f), glm::vec3(blockPos) + .5f); // non-scaled
		// add a random color to each quad
	//float r = Utils::get_random_r(0, 1);
	//float g = Utils::get_random_r(0, 1);
	//float b = Utils::get_random_r(0, 1);
	//glm::vec3 colory(r, g, b);
	//glm::vec3 colory(0, 1, 0);

	float shiny = Utils::get_random_r(0, 128);

	for (int i = quadStride * curQuad; i < quadStride * (curQuad + 1); i += 8) // += floats per vertex
	{
		glm::vec4 tri(
			data[i + 0], 
			data[i + 1], 
			data[i + 2], 
			1.f);
		tri = localTransform * tri;

		// pos
		quad.push_back(tri.x);
		quad.push_back(tri.y);
		quad.push_back(tri.z);

		// color
		quad.push_back(colorTEMP.r);
		quad.push_back(colorTEMP.g);
		quad.push_back(colorTEMP.b);

		// normals
		quad.push_back(data[i + 3]);
		quad.push_back(data[i + 4]);
		quad.push_back(data[i + 5]);

		// shininess (rng)
		quad.push_back(shiny);

		// texture
		//quad.push_back(data[i + 6]);
		//quad.push_back(data[i + 7]);
	}

	return quad;
}

static std::ostream& operator<<(std::ostream& o, glm::ivec3 v)
{
	return o << '('
		<< v.x << ", "
		<< v.y << ", "
		<< v.z << ')';
}

// prints to console if errors are found
void TestCoordinateStuff()
{
	using namespace std;
	for (auto& p : Chunk::chunks)
	{
		using namespace std;
		//cout << p.first << '\n';
		if (!p.second)
			continue;
		if (p.first != p.second->GetPos())
			cout << "Hash key " << p.first 
			<< " does not match chunk internal position" 
			<< p.second->GetPos() << '\n';

		for (int x = 0; x < Chunk::CHUNK_SIZE; x++)
		{
			for (int y = 0; y < Chunk::CHUNK_SIZE; y++)
			{
				for (int z = 0; z < Chunk::CHUNK_SIZE; z++)
				{
					glm::ivec3 posLocal(x, y, z);
					glm::ivec3 posActual = p.second->chunkBlockToWorldPos(posLocal);
					localpos posCalc = Chunk::worldBlockToLocalPos(posActual);

					if (posCalc.chunk_pos != p.first)
						cout << "Calculated chunk position " << 
						posCalc.chunk_pos << 
						" does not match hash key " << p.first;
					if (posCalc.block_pos != posLocal)
						cout << "Calculated block position " <<
						posCalc.block_pos <<
						" does not match local pos " << posLocal;
				}
			}
		}
	}
}