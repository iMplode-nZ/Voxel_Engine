#include "stdafx.h"

#include "vao.h"
#include "render.h"

VAO::VAO()
{
	glGenVertexArrays(1, &_rendererID);
	glBindVertexArray(_rendererID);
}

VAO::~VAO()
{
	glDeleteVertexArrays(1, &_rendererID);
}

void VAO::AddBuffer(const VBO & vb, const VBOlayout & layout)
{
	Bind();
	vb.Bind();
	const auto& elements = layout.GetElements();
	GLuint offset = 0;
	for (size_t i = 0; i < elements.size(); i++)
	{
		const auto& element = elements[i];
		glEnableVertexAttribArray(i);
		glVertexAttribPointer(i, element.count, element.type, element.normalized, 
			layout.GetStride(), (const void*)offset);
		offset += element.count * VBOElement::TypeSize(element.type);
	}
}

void VAO::Bind() const
{
	glBindVertexArray(_rendererID);
}

void VAO::Unbind() const
{
	glBindVertexArray(0);
}
