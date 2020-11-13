#include "Framebuffers.hpp"
#include "Load.hpp"
#include "gl_compile_program.hpp"
#include "gl_check_fb.hpp"
#include "gl_errors.hpp"

#include <array>

Framebuffers framebuffers;

void Framebuffers::realloc(glm::uvec2 const &drawable_size) {
	if (drawable_size == size) return;
	size = drawable_size;

	//name texture if not yet named:
	if (hdr_color_tex == 0) glGenTextures(1, &hdr_color_tex);

	//resize texture:
	glBindTexture(GL_TEXTURE_2D, hdr_color_tex);
	glTexImage2D(GL_TEXTURE_2D, 0,
		GL_RGB16F, //<-- storage will be RGB 16-bit half-float
		size.x, size.y, 0, //width, height, border
		GL_RGB, GL_FLOAT, //<-- source data (if we were uploading it) would be floating point RGB
		nullptr //<-- don't upload data, just allocate on-GPU storage
	);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);

	//name renderbuffer if not yet named:
	if (hdr_depth_rb == 0) glGenRenderbuffers(1, &hdr_depth_rb);

	//resize renderbuffer:
	glBindRenderbuffer(GL_RENDERBUFFER, hdr_depth_rb);
	glRenderbufferStorage(GL_RENDERBUFFER,
		GL_DEPTH_COMPONENT24, //<-- storage will be 24-bit fixed point depth values
		size.x, size.y);
	glBindRenderbuffer(GL_RENDERBUFFER, 0);

	//set up framebuffer if not yet named:
	if (hdr_fb == 0) {
		glGenFramebuffers(1, &hdr_fb);
		glBindFramebuffer(GL_FRAMEBUFFER, hdr_fb);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, hdr_color_tex, 0);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, hdr_depth_rb);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	glBindFramebuffer(GL_FRAMEBUFFER, hdr_fb);
	gl_check_fb(); //<-- helper function to check framebuffer completeness
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	//-------------------

	if (blur_x_tex == 0) glGenTextures(1, &blur_x_tex);

	//resize texture:
	glBindTexture(GL_TEXTURE_2D, blur_x_tex);
	glTexImage2D(GL_TEXTURE_2D, 0,
		GL_RGB16F, //<-- storage will be RGB 16-bit half-float
		size.x, size.y, 0, //width, height, border
		GL_RGB, GL_FLOAT, //<-- source data (if we were uploading it) would be floating point RGB
		nullptr //<-- don't upload data, just allocate on-GPU storage
	);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);

	//set up framebuffer if not yet named:
	if (blur_x_fb == 0) {
		glGenFramebuffers(1, &blur_x_fb);
		glBindFramebuffer(GL_FRAMEBUFFER, blur_x_fb);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, blur_x_tex, 0);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	glBindFramebuffer(GL_FRAMEBUFFER, blur_x_fb);
	gl_check_fb(); //<-- helper function to check framebuffer completeness
	glBindFramebuffer(GL_FRAMEBUFFER, 0);


	GL_ERRORS();
}

struct ToneMapProgram {
	ToneMapProgram() {
		program = gl_compile_program(
			//vertex shader -- draws a fullscreen triangle using no attribute streams
			"#version 330\n"
			"void main() {\n"
			"	gl_Position = vec4(4 * (gl_VertexID & 1) - 1,  2 * (gl_VertexID & 2) - 1, 0.0, 1.0);\n"
			"}\n"
		,
			//fragment shader -- reads a HDR texture, maps to output pixel colors
			"#version 330\n"
			"uniform sampler2D TEX;\n"
			"out vec4 fragColor;\n"
			"void main() {\n"
			"	vec3 color = texelFetch(TEX, ivec2(gl_FragCoord.xy), 0).rgb;\n"
			//exposure-correction-style range compression:
			"		color = vec3(log(color.r + 1.0), log(color.g + 1.0), log(color.b + 1.0)) / log(2.0 + 1.0);\n"
			//weird color effect:
			//"		color = vec3(color.rg, gl_FragCoord.x/textureSize(TEX,0).x);\n"
			//basic gamma-style range compression:
			//"		color = vec3(pow(color.r, 0.45), pow(color.g, 0.45), pow(color.b, 0.45));\n"
			//raw values:
			//"		color = color;\n"
			"	fragColor = vec4(color, 1.0);\n"
			"}\n"
		);
		
		//set TEX to texture unit 0:
		GLuint TEX_sampler2D = glGetUniformLocation(program, "TEX");
		glUseProgram(program);
		glUniform1i(TEX_sampler2D, 0);
		glUseProgram(0);

		GL_ERRORS();
	}

	GLuint program = 0;

	//uniforms:
	//none

	//textures:
	//texture0 -- texture to copy
};

GLuint empty_vao = 0;
Load< ToneMapProgram > tone_map_program(LoadTagEarly, []() -> ToneMapProgram const * {
	glGenVertexArrays(1, &empty_vao);
	return new ToneMapProgram();
});

void Framebuffers::tone_map() {
	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);

	glUseProgram(tone_map_program->program);
	glBindVertexArray(empty_vao);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, hdr_color_tex);

	glDrawArrays(GL_TRIANGLES, 0, 3);

	glBindTexture(GL_TEXTURE_2D, 0);

	glBindVertexArray(0);
	glUseProgram(0);
}

constexpr uint32_t KERNEL_RADIUS = 30;
std::array< float, KERNEL_RADIUS > bloom_kernel = ([](){
	std::array< float, KERNEL_RADIUS > weights;
	//compute a bloom kernel as a somewhat peaked distribution,
	// hand authored to look a'ight:

	// NOTE: you could probably compute this more exactly by:
	//  - simulating the scattering behavior in human eyes to get
	//    a 2D PSF (point spread function).
	//    (this probably also depends on the expected on-screen
	//     size of the image.)
	//  - splitting this PSF into an outer product of two 1D blurs
	//    (doable with an SVD).

	for (uint32_t i = 0; i < weights.size(); ++i) {
		float pos = i / float(weights.size());
		weights[i] = std::pow(20.0f, 1.0f - pos) - 1.0f;
	}

	//normalize:
	float sum = 0.0f;
	for (auto const &w : weights) sum += w;
	sum = 2.0f * sum - weights[0]; //account for the fact that all taps other than center are used 2x
	float inv_sum = 1.0f / sum;
	for (auto &w : weights) w *= inv_sum;

	return weights;
})();

struct BlurXProgram {
	BlurXProgram() {
		program = gl_compile_program(
			//vertex shader -- draws a fullscreen triangle using no attribute streams
			"#version 330\n"
			"void main() {\n"
			"	gl_Position = vec4(4 * (gl_VertexID & 1) - 1,  2 * (gl_VertexID & 2) - 1, 0.0, 1.0);\n"
			"}\n"
		,
			//fragment shader -- blur in X direction with a given kernel
			"#version 330\n"
			"uniform sampler2D TEX;\n"
			"const int KERNEL_RADIUS = " + std::to_string(KERNEL_RADIUS) + ";\n"
			"uniform float KERNEL[KERNEL_RADIUS];\n"
			"out vec4 fragColor;\n"
			"void main() {\n"
			"	ivec2 c = ivec2(gl_FragCoord.xy);\n"
			"	int limit = textureSize(TEX, 0).x-1;\n"
			"	vec3 acc = KERNEL[0] * texelFetch(TEX, c, 0).rgb;\n"
			"	for (int ofs = 1; ofs < KERNEL_RADIUS; ++ofs) {\n"
			"		acc += KERNEL[ofs] * (\n"
			"			  texelFetch(TEX, ivec2(min(c.x+ofs, limit), c.y), 0).rgb\n"
			"			+ texelFetch(TEX, ivec2(max(c.x-ofs, 0), c.y), 0).rgb\n"
			"		);\n"
			"	}\n"
			"	fragColor = vec4(acc,1.0);\n"
			"}\n"
		);

		glUseProgram(program);
		//set KERNEL:
		GLuint KERNEL_float_array = glGetUniformLocation(program, "KERNEL");
		glUniform1fv(KERNEL_float_array, KERNEL_RADIUS, bloom_kernel.data());

		
		//set TEX to texture unit 0:
		GLuint TEX_sampler2D = glGetUniformLocation(program, "TEX");
		glUniform1i(TEX_sampler2D, 0);

		glUseProgram(0);

		GL_ERRORS();
	}

	GLuint program = 0;

	//uniforms:
	//none

	//textures:
	//texture0 -- texture to copy
};

Load< BlurXProgram > blur_x_program(LoadTagEarly);


struct BlurYProgram {
	BlurYProgram() {
		program = gl_compile_program(
			//vertex shader -- draws a fullscreen triangle using no attribute streams
			"#version 330\n"
			"void main() {\n"
			"	gl_Position = vec4(4 * (gl_VertexID & 1) - 1,  2 * (gl_VertexID & 2) - 1, 0.0, 1.0);\n"
			"}\n"
		,
			//fragment shader -- blur in X direction with a given kernel
			"#version 330\n"
			"uniform sampler2D TEX;\n"
			"const int KERNEL_RADIUS = " + std::to_string(KERNEL_RADIUS) + ";\n"
			"uniform float KERNEL[KERNEL_RADIUS];\n"
			"out vec4 fragColor;\n"
			"void main() {\n"
			"	ivec2 c = ivec2(gl_FragCoord.xy);\n"
			"	int limit = textureSize(TEX, 0).y-1;\n"
			"	vec3 acc = KERNEL[0] * texelFetch(TEX, c, 0).rgb;\n"
			"	for (int ofs = 1; ofs < KERNEL_RADIUS; ++ofs) {\n"
			"		acc += KERNEL[ofs] * (\n"
			"			  texelFetch(TEX, ivec2(c.x, min(c.y+ofs, limit)), 0).rgb\n"
			"			+ texelFetch(TEX, ivec2(c.x, max(c.y-ofs, 0)), 0).rgb\n"
			"		);\n"
			"	}\n"
			"	fragColor = vec4(acc,0.1);\n" //<-- alpha here controls strength of effect, because blending used on this pass
			"}\n"
		);

		glUseProgram(program);
		//set KERNEL:
		GLuint KERNEL_float_array = glGetUniformLocation(program, "KERNEL");
		glUniform1fv(KERNEL_float_array, KERNEL_RADIUS, bloom_kernel.data());

		
		//set TEX to texture unit 0:
		GLuint TEX_sampler2D = glGetUniformLocation(program, "TEX");
		glUniform1i(TEX_sampler2D, 0);

		glUseProgram(0);

		GL_ERRORS();
	}

	GLuint program = 0;

	//uniforms:
	//none

	//textures:
	//texture0 -- texture to copy
};

Load< BlurYProgram > blur_y_program(LoadTagEarly);

void Framebuffers::add_bloom() {
	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);

	//blur hdr_color_tex in the X direction, store into blur_x_tex:
	glBindFramebuffer(GL_FRAMEBUFFER, blur_x_fb);

	glUseProgram(blur_x_program->program);
	glBindVertexArray(empty_vao);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, hdr_color_tex);

	glDrawArrays(GL_TRIANGLES, 0, 3);

	glBindTexture(GL_TEXTURE_2D, 0);

	glBindVertexArray(0);
	glUseProgram(0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	//blur blur_x_tex in the Y direction, store back into hdr_color_tex:
	glBindFramebuffer(GL_FRAMEBUFFER, hdr_fb);

	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glUseProgram(blur_y_program->program);
	glBindVertexArray(empty_vao);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, blur_x_tex);

	glDrawArrays(GL_TRIANGLES, 0, 3);

	glBindTexture(GL_TEXTURE_2D, 0);

	glBindVertexArray(0);
	glUseProgram(0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glDisable(GL_BLEND);

	GL_ERRORS();
}
