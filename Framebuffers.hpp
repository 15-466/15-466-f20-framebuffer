#pragma once

#include "GL.hpp"
#include <glm/glm.hpp>

//A global set of framebuffers for use in various offscreen rendering effects:

struct Framebuffers {
	void realloc(glm::uvec2 const &drawable_size); //called to trigger (re-)allocation on window size change

	//current size of framebuffer attachments:
	glm::uvec2 size = glm::uvec2(0,0);

	//for the "hdr tonemapping" rendering example:
	struct {
		GLuint color_tex = 0; //GL_RGB16F color texture
		GLuint depth_rb = 0; //GL_DEPTH_COMPONENT24 renderbuffer
		GLuint fb = 0; // color0: color_tex , depth: depth_rb
	} hdr;
	void tone_map(); //copy hdr.color_tex to screen with tone mapping applied

	//for the "bloom" effect:
	struct {
		GLuint blur_x_tex = 0; //GL_RGB16F color texture for first pass of blur
		GLuint blur_x_fb = 0; // color0: blur_x_tex
	} bloom;
	void add_bloom(); //do a basic bloom effect on the hdr.color_tex
};

//the actual storage:
extern Framebuffers framebuffers;
//NOTE: I could have used a namespace and declared every element extern but that seemed more cumbersome to write
