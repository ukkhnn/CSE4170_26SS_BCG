﻿// DrawScene.cpp - CSE4170 HW3
// student: 20211551
//
// Data setup: copy 4.0.2 Data/ folder into project root
//   Data/dynamic_objects/tiger/Tiger_00~11_triangles_vnt.geom
//   Data/dynamic_objects/wolf/wolf_00~16_vnt.geom
//   Data/dynamic_objects/spider/spider_vnt_00~15.geom
//   Data/static_objects/cow_vnt.geom, ironman_vnt.geom, godzilla_vnt.geom
//
// Keys:
//   u/i/o/p    - fixed observation cameras
//   a          - free-moving camera
//   t          - tiger eye camera
//   g          - tiger follow camera
//   c          - CCTV split screen toggle
//   SPACE      - animation on/off
//   f          - grid/axes toggle
//   ESC        - quit
//
// Camera controls (u/i/o/p mode):
//   CTRL + left drag up/down   - pitch (rotate around u-axis)
//   CTRL + left drag left/right - yaw (rotate around v-axis)
//   CTRL + scroll               - zoom in/out
//
// Camera controls (a mode):
//   arrow up/down   - move forward/backward (-n / +n direction)
//   arrow left/right - strafe left/right (-u / +u direction)
//   PageUp/Down     - move up/down (+v / -v direction)
//   left drag       - pitch/yaw
//   right drag      - roll
//   CTRL + scroll   - zoom in/out

#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <GL/glew.h>
#include <GL/freeglut.h>
#include "LoadScene.h"

// shader setup
#include "Shaders/LoadShaders.h"
#include "ShadingInfo.h"

extern SCENE scene;

// simple shader
GLuint h_ShaderProgram_simple;
GLuint h_ShaderProgram_background, h_ShaderProgram_equiToCube;
GLint loc_ModelViewProjectionMatrix, loc_primitive_color;

// PBR shader
GLuint h_ShaderProgram_TXPBR;
#define NUMBER_OF_LIGHT_SUPPORTED 1
GLint loc_global_ambient_color;
GLint loc_lightCount;
loc_light_Parameters loc_light[NUMBER_OF_LIGHT_SUPPORTED];
loc_Material_Parameters loc_material;
GLint loc_ModelViewProjectionMatrix_TXPBR, loc_ModelViewMatrix_TXPBR, loc_ModelViewMatrixInvTrans_TXPBR;
GLint loc_cameraPos;

#define TEXTURE_INDEX_DIFFUSE	(0)
#define TEXTURE_INDEX_NORMAL	(1)
#define TEXTURE_INDEX_SPECULAR	(2)
#define TEXTURE_INDEX_EMISSIVE	(3)
#define TEXTURE_INDEX_SKYMAP	(4)

// skybox shader
GLuint h_ShaderProgram_skybox;
GLint loc_cubemap_skybox;
GLint loc_ModelViewProjectionMatrix_SKY;

#include <glm/gtc/matrix_transform.hpp>
glm::mat4 ViewProjectionMatrix, ViewMatrix, ProjectionMatrix;
glm::mat4 ModelViewProjectionMatrix;
glm::mat4 ModelViewMatrix;
glm::mat3 ModelViewMatrixInvTrans;

#define TO_RADIAN 0.01745329252f
#define TO_DEGREE 57.295779513f

// read .geom file: [int n_tri][n_tri * bytes_per_prim]
int read_geometry_vnt(GLfloat** object, int bytes_per_primitive, const char* filename) {
	int n_triangles;
	FILE* fp = fopen(filename, "rb");
	if (!fp) { fprintf(stderr, "Cannot open %s\n", filename); return -1; }
	fread(&n_triangles, sizeof(int), 1, fp);
	*object = (float*)malloc(n_triangles * bytes_per_primitive);
	if (!*object) { fclose(fp); return -1; }
	fread(*object, bytes_per_primitive, n_triangles, fp);
	fclose(fp);
	return n_triangles;
}

// ==================== camera ====================
typedef enum {
	CAMERA_U, CAMERA_I, CAMERA_O, CAMERA_P,  //
	CAMERA_A,                                  // free-moving
	CAMERA_T,                                  // tiger eye
	CAMERA_G,                                  // tiger follow
	NUM_CAMERAS
} CAMERA_INDEX;

typedef struct _Camera {
	float pos[3];
	float uaxis[3], vaxis[3], naxis[3];
	float fovy, aspect_ratio, near_c, far_c;
	int move, rotation_axis;
} Camera;

Camera camera_info[NUM_CAMERAS];
Camera current_camera;
Camera camera_u_initial; // CCTV
int current_camera_index = CAMERA_U;
bool cctv_mode = false;
bool cctv_tracking = false;

using glm::mat4;

void set_ViewMatrix_from_camera_frame(void) {
	ViewMatrix = glm::mat4(
		current_camera.uaxis[0], current_camera.vaxis[0], current_camera.naxis[0], 0.0f,
		current_camera.uaxis[1], current_camera.vaxis[1], current_camera.naxis[1], 0.0f,
		current_camera.uaxis[2], current_camera.vaxis[2], current_camera.naxis[2], 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f);
	ViewMatrix = glm::translate(ViewMatrix, glm::vec3(-current_camera.pos[0], -current_camera.pos[1], -current_camera.pos[2]));
}

void set_current_camera(int camera_num) {
	memcpy(&current_camera, &camera_info[camera_num], sizeof(Camera));
	set_ViewMatrix_from_camera_frame();
	ProjectionMatrix = glm::perspective(current_camera.fovy, current_camera.aspect_ratio, current_camera.near_c, current_camera.far_c);
	ViewProjectionMatrix = ProjectionMatrix * ViewMatrix;
}

// eye, at (world up = Z)
void compute_camera_frame(Camera* cam, glm::vec3 eye, glm::vec3 at) {
	glm::vec3 n = glm::normalize(eye - at);
	glm::vec3 up(0.0f, 0.0f, 1.0f);
	// if n is nearly parallel to world-up, use Y as fallback
	if (fabsf(glm::dot(n, up)) > 0.99f) up = glm::vec3(0.0f, 1.0f, 0.0f);
	glm::vec3 u = glm::normalize(glm::cross(up, n));
	glm::vec3 v = glm::cross(n, u);
	cam->pos[0] = eye.x; cam->pos[1] = eye.y; cam->pos[2] = eye.z;
	cam->uaxis[0] = u.x; cam->uaxis[1] = u.y; cam->uaxis[2] = u.z;
	cam->vaxis[0] = v.x; cam->vaxis[1] = v.y; cam->vaxis[2] = v.z;
	cam->naxis[0] = n.x; cam->naxis[1] = n.y; cam->naxis[2] = n.z;
}

// pitch: u
void camera_pitch(Camera* cam, float angle) {
	glm::vec3 u(cam->uaxis[0], cam->uaxis[1], cam->uaxis[2]);
	glm::mat3 R = glm::mat3(glm::rotate(glm::mat4(1.0f), angle, u));
	glm::vec3 v = R * glm::vec3(cam->vaxis[0], cam->vaxis[1], cam->vaxis[2]);
	glm::vec3 n = R * glm::vec3(cam->naxis[0], cam->naxis[1], cam->naxis[2]);
	cam->vaxis[0] = v.x; cam->vaxis[1] = v.y; cam->vaxis[2] = v.z;
	cam->naxis[0] = n.x; cam->naxis[1] = n.y; cam->naxis[2] = n.z;
}

// yaw: v
void camera_yaw(Camera* cam, float angle) {
	glm::vec3 v(cam->vaxis[0], cam->vaxis[1], cam->vaxis[2]);
	glm::mat3 R = glm::mat3(glm::rotate(glm::mat4(1.0f), angle, v));
	glm::vec3 u = R * glm::vec3(cam->uaxis[0], cam->uaxis[1], cam->uaxis[2]);
	glm::vec3 n = R * glm::vec3(cam->naxis[0], cam->naxis[1], cam->naxis[2]);
	cam->uaxis[0] = u.x; cam->uaxis[1] = u.y; cam->uaxis[2] = u.z;
	cam->naxis[0] = n.x; cam->naxis[1] = n.y; cam->naxis[2] = n.z;
}

// roll: n
void camera_roll(Camera* cam, float angle) {
	glm::vec3 n(cam->naxis[0], cam->naxis[1], cam->naxis[2]);
	glm::mat3 R = glm::mat3(glm::rotate(glm::mat4(1.0f), angle, n));
	glm::vec3 u = R * glm::vec3(cam->uaxis[0], cam->uaxis[1], cam->uaxis[2]);
	glm::vec3 v = R * glm::vec3(cam->vaxis[0], cam->vaxis[1], cam->vaxis[2]);
	cam->uaxis[0] = u.x; cam->uaxis[1] = u.y; cam->uaxis[2] = u.z;
	cam->vaxis[0] = v.x; cam->vaxis[1] = v.y; cam->vaxis[2] = v.z;
}

void initialize_camera(void) {
	float default_fovy = TO_RADIAN * scene.camera.fovy;
	float default_aspect = scene.camera.aspect;

	// Camera U: east side, looking at bistro
	compute_camera_frame(&camera_info[CAMERA_U], glm::vec3(3200.0f, 300.0f, 250.0f), glm::vec3(0.0f, 300.0f, 100.0f));
	camera_info[CAMERA_U].fovy = default_fovy; camera_info[CAMERA_U].aspect_ratio = default_aspect;
	camera_info[CAMERA_U].near_c = 0.1f; camera_info[CAMERA_U].far_c = 50000.0f;

	// Camera I: elevated bird's eye view
	compute_camera_frame(&camera_info[CAMERA_I], glm::vec3(2500.0f, 2500.0f, 2000.0f), glm::vec3(500.0f, -500.0f, 0.0f));
	camera_info[CAMERA_I].fovy = default_fovy; camera_info[CAMERA_I].aspect_ratio = default_aspect;
	camera_info[CAMERA_I].near_c = 0.1f; camera_info[CAMERA_I].far_c = 50000.0f;

	// Camera O: south alley looking north
	compute_camera_frame(&camera_info[CAMERA_O], glm::vec3(-300.0f, -2500.0f, 200.0f), glm::vec3(500.0f, 0.0f, 100.0f));
	camera_info[CAMERA_O].fovy = default_fovy; camera_info[CAMERA_O].aspect_ratio = default_aspect;
	camera_info[CAMERA_O].near_c = 0.1f; camera_info[CAMERA_O].far_c = 50000.0f;

	// Camera P: rooftop view looking down
	compute_camera_frame(&camera_info[CAMERA_P], glm::vec3(1200.0f, -300.0f, 1200.0f), glm::vec3(1000.0f, -1000.0f, 0.0f));
	camera_info[CAMERA_P].fovy = default_fovy; camera_info[CAMERA_P].aspect_ratio = default_aspect;
	camera_info[CAMERA_P].near_c = 0.1f; camera_info[CAMERA_P].far_c = 50000.0f;

	// Camera A: initial free-cam position
	compute_camera_frame(&camera_info[CAMERA_A], glm::vec3(1500.0f, -800.0f, 400.0f), glm::vec3(500.0f, 0.0f, 0.0f));
	camera_info[CAMERA_A].fovy = default_fovy; camera_info[CAMERA_A].aspect_ratio = default_aspect;
	camera_info[CAMERA_A].near_c = 0.1f; camera_info[CAMERA_A].far_c = 50000.0f;

	// Camera T, G: dynamic cameras, initialized by update functions
	camera_info[CAMERA_T].fovy = default_fovy; camera_info[CAMERA_T].aspect_ratio = default_aspect;
	camera_info[CAMERA_T].near_c = 0.1f; camera_info[CAMERA_T].far_c = 50000.0f;
	camera_info[CAMERA_G].fovy = default_fovy; camera_info[CAMERA_G].aspect_ratio = default_aspect;
	camera_info[CAMERA_G].near_c = 0.1f; camera_info[CAMERA_G].far_c = 50000.0f;

// save Camera_U initial state for CCTV tracking reset
	memcpy(&camera_u_initial, &camera_info[CAMERA_U], sizeof(Camera));

	set_current_camera(CAMERA_U);
}

// load shader programs
void prepare_shader_program(void) {
	char string[256];

	ShaderInfo shader_info[3] = {
		{ GL_VERTEX_SHADER, "Shaders/simple.vert" },
		{ GL_FRAGMENT_SHADER, "Shaders/simple.frag" },
		{ GL_NONE, NULL }
	};
	h_ShaderProgram_simple = LoadShaders(shader_info);
	glUseProgram(h_ShaderProgram_simple);
	loc_ModelViewProjectionMatrix = glGetUniformLocation(h_ShaderProgram_simple, "u_ModelViewProjectionMatrix");
	loc_primitive_color = glGetUniformLocation(h_ShaderProgram_simple, "u_primitive_color");

	ShaderInfo shader_info_TXPBR[3] = {
		{ GL_VERTEX_SHADER, "Shaders/Background/PBR_Tx.vert" },
		{ GL_FRAGMENT_SHADER, "Shaders/Background/PBR_Tx.frag" },
		{ GL_NONE, NULL }
	};
	h_ShaderProgram_TXPBR = LoadShaders(shader_info_TXPBR);
	glUseProgram(h_ShaderProgram_TXPBR);

	loc_ModelViewProjectionMatrix_TXPBR = glGetUniformLocation(h_ShaderProgram_TXPBR, "u_ModelViewProjectionMatrix");
	loc_ModelViewMatrix_TXPBR = glGetUniformLocation(h_ShaderProgram_TXPBR, "u_ModelViewMatrix");
	loc_ModelViewMatrixInvTrans_TXPBR = glGetUniformLocation(h_ShaderProgram_TXPBR, "u_ModelViewMatrixInvTrans");
	loc_lightCount = glGetUniformLocation(h_ShaderProgram_TXPBR, "u_light_count");

	for (int i = 0; i < NUMBER_OF_LIGHT_SUPPORTED; i++) {
		sprintf(string, "u_light[%d].position", i);
		loc_light[i].position = glGetUniformLocation(h_ShaderProgram_TXPBR, string);
		sprintf(string, "u_light[%d].color", i);
		loc_light[i].color = glGetUniformLocation(h_ShaderProgram_TXPBR, string);
	}
	loc_cameraPos = glGetUniformLocation(h_ShaderProgram_TXPBR, "u_camPos");
	loc_material.diffuseTex = glGetUniformLocation(h_ShaderProgram_TXPBR, "u_albedoMap");
	loc_material.normalTex = glGetUniformLocation(h_ShaderProgram_TXPBR, "u_normalMap");
	loc_material.specularTex = glGetUniformLocation(h_ShaderProgram_TXPBR, "u_metallicRoughnessMap");
	loc_material.emissiveTex = glGetUniformLocation(h_ShaderProgram_TXPBR, "u_emissiveMap");

	ShaderInfo shader_info_skybox[3] = {
		{ GL_VERTEX_SHADER, "Shaders/Background/skybox.vert" },
		{ GL_FRAGMENT_SHADER, "Shaders/Background/skybox.frag" },
		{ GL_NONE, NULL }
	};
	h_ShaderProgram_skybox = LoadShaders(shader_info_skybox);
	loc_cubemap_skybox = glGetUniformLocation(h_ShaderProgram_skybox, "u_skymap");
	loc_ModelViewProjectionMatrix_SKY = glGetUniformLocation(h_ShaderProgram_skybox, "u_ModelViewProjectionMatrix");
}

// ==================== geometry: axes, grid, bistro, skybox ( ) ====================
#define BUFFER_OFFSET(offset) ((GLvoid *) (offset))
#define INDEX_VERTEX_POSITION	0
#define INDEX_NORMAL			1
#define INDEX_TEX_COORD			2

bool b_draw_grid = false;

// axes
GLuint axes_VBO, axes_VAO;
GLfloat axes_vertices[6][3] = {
	{ 0.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f },
	{ 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }
};
GLfloat axes_color[3][3] = { { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } };

void prepare_axes(void) {
	glGenBuffers(1, &axes_VBO);
	glBindBuffer(GL_ARRAY_BUFFER, axes_VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(axes_vertices), &axes_vertices[0][0], GL_STATIC_DRAW);
	glGenVertexArrays(1, &axes_VAO);
	glBindVertexArray(axes_VAO);
	glBindBuffer(GL_ARRAY_BUFFER, axes_VBO);
	glVertexAttribPointer(INDEX_VERTEX_POSITION, 3, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0));
	glEnableVertexAttribArray(INDEX_VERTEX_POSITION);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}

void draw_axes(void) {
	if (!b_draw_grid) return;
	glUseProgram(h_ShaderProgram_simple);
	ModelViewMatrix = glm::scale(ViewMatrix, glm::vec3(8000.0f, 8000.0f, 8000.0f));
	ModelViewProjectionMatrix = ProjectionMatrix * ModelViewMatrix;
	glUniformMatrix4fv(loc_ModelViewProjectionMatrix, 1, GL_FALSE, &ModelViewProjectionMatrix[0][0]);
	glLineWidth(2.0f);
	glBindVertexArray(axes_VAO);
	glUniform3fv(loc_primitive_color, 1, axes_color[0]);
	glDrawArrays(GL_LINES, 0, 2);
	glUniform3fv(loc_primitive_color, 1, axes_color[1]);
	glDrawArrays(GL_LINES, 2, 2);
	glUniform3fv(loc_primitive_color, 1, axes_color[2]);
	glDrawArrays(GL_LINES, 4, 2);
	glBindVertexArray(0);
	glLineWidth(1.0f);
	glUseProgram(0);
}

// grid
#define GRID_LENGTH			(100)
#define NUM_GRID_VETICES	((2 * GRID_LENGTH + 1) * 4)
GLuint grid_VBO, grid_VAO;
GLfloat grid_vertices[NUM_GRID_VETICES][3];
GLfloat grid_color[3] = { 0.5f, 0.5f, 0.5f };

void prepare_grid(void) {
	int vertex_idx = 0;
	for (int x_idx = -GRID_LENGTH; x_idx <= GRID_LENGTH; x_idx++) {
		grid_vertices[vertex_idx][0] = (float)x_idx; grid_vertices[vertex_idx][1] = (float)-GRID_LENGTH; grid_vertices[vertex_idx][2] = 0.0f; vertex_idx++;
		grid_vertices[vertex_idx][0] = (float)x_idx; grid_vertices[vertex_idx][1] = (float)GRID_LENGTH;  grid_vertices[vertex_idx][2] = 0.0f; vertex_idx++;
	}
	for (int y_idx = -GRID_LENGTH; y_idx <= GRID_LENGTH; y_idx++) {
		grid_vertices[vertex_idx][0] = (float)-GRID_LENGTH; grid_vertices[vertex_idx][1] = (float)y_idx; grid_vertices[vertex_idx][2] = 0.0f; vertex_idx++;
		grid_vertices[vertex_idx][0] = (float)GRID_LENGTH;  grid_vertices[vertex_idx][1] = (float)y_idx; grid_vertices[vertex_idx][2] = 0.0f; vertex_idx++;
	}
	glGenBuffers(1, &grid_VBO);
	glBindBuffer(GL_ARRAY_BUFFER, grid_VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(grid_vertices), &grid_vertices[0][0], GL_STATIC_DRAW);
	glGenVertexArrays(1, &grid_VAO);
	glBindVertexArray(grid_VAO);
	glBindBuffer(GL_ARRAY_BUFFER, grid_VBO);
	glVertexAttribPointer(INDEX_VERTEX_POSITION, 3, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0));
	glEnableVertexAttribArray(INDEX_VERTEX_POSITION);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}

void draw_grid(void) {
	if (!b_draw_grid) return;
	glUseProgram(h_ShaderProgram_simple);
	ModelViewMatrix = glm::scale(ViewMatrix, glm::vec3(100.0f, 100.0f, 100.0f));
	ModelViewProjectionMatrix = ProjectionMatrix * ModelViewMatrix;
	glUniformMatrix4fv(loc_ModelViewProjectionMatrix, 1, GL_FALSE, &ModelViewProjectionMatrix[0][0]);
	glLineWidth(1.0f);
	glBindVertexArray(grid_VAO);
	glUniform3fv(loc_primitive_color, 1, grid_color);
	glDrawArrays(GL_LINES, 0, NUM_GRID_VETICES);
	glBindVertexArray(0);
	glLineWidth(1.0f);
	glUseProgram(0);
}

// bistro exterior
GLuint* bistro_exterior_VBO;
GLuint* bistro_exterior_VAO;
int* bistro_exterior_n_triangles;
int* bistro_exterior_vertex_offset;
GLfloat** bistro_exterior_vertices;
GLuint* bistro_exterior_texture_names;
int flag_fog;
bool* flag_texture_mapping;

void initialize_lights(void) {
	glUseProgram(h_ShaderProgram_TXPBR);
	glUniform1f(loc_lightCount, (float)scene.n_lights);
	for (int i = 0; i < scene.n_lights; i++) {
		glUniform4f(loc_light[i].position, scene.light_list[i].pos[0], scene.light_list[i].pos[1], scene.light_list[i].pos[2], 0.0f);
		glUniform3f(loc_light[i].color, scene.light_list[i].color[0], scene.light_list[i].color[1], scene.light_list[i].color[2]);
	}
	glUseProgram(0);
}

bool readTexImage2D_from_file(char* filename) {
	FREE_IMAGE_FORMAT tx_file_format;
	int tx_bits_per_pixel;
	FIBITMAP* tx_pixmap;
	int width, height;
	GLvoid* data;

	tx_file_format = FreeImage_GetFileType(filename, 0);
	tx_pixmap = FreeImage_Load(tx_file_format, filename);
	if (tx_pixmap == NULL) return false;
	tx_bits_per_pixel = FreeImage_GetBPP(tx_pixmap);

	GLenum format, internalFormat;
	if (tx_bits_per_pixel == 32) { format = GL_BGRA; internalFormat = GL_RGBA; }
	else if (tx_bits_per_pixel == 24) { format = GL_BGR; internalFormat = GL_RGB; }
	else { tx_pixmap = FreeImage_ConvertTo32Bits(tx_pixmap); format = GL_BGRA; internalFormat = GL_RGBA; }

	width = FreeImage_GetWidth(tx_pixmap);
	height = FreeImage_GetHeight(tx_pixmap);
	data = FreeImage_GetBits(tx_pixmap);
	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, GL_UNSIGNED_BYTE, data);
	FreeImage_Unload(tx_pixmap);
	return true;
}

void prepare_bistro_exterior(void) {
	int n_bytes_per_vertex = 8 * sizeof(float);
	int n_bytes_per_triangle = 3 * n_bytes_per_vertex;

	bistro_exterior_VBO = (GLuint*)malloc(sizeof(GLuint) * scene.n_materials);
	bistro_exterior_VAO = (GLuint*)malloc(sizeof(GLuint) * scene.n_materials);
	bistro_exterior_n_triangles = (int*)malloc(sizeof(int) * scene.n_materials);
	bistro_exterior_vertex_offset = (int*)malloc(sizeof(int) * scene.n_materials);
	flag_texture_mapping = (bool*)malloc(sizeof(bool) * scene.n_textures);
	bistro_exterior_vertices = (GLfloat**)malloc(sizeof(GLfloat*) * scene.n_materials);

	for (int materialIdx = 0; materialIdx < scene.n_materials; materialIdx++) {
		MATERIAL* pMaterial = &(scene.material_list[materialIdx]);
		GEOMETRY_TRIANGULAR_MESH* tm = &(pMaterial->geometry.tm);
		bistro_exterior_vertices[materialIdx] = (GLfloat*)malloc(sizeof(GLfloat) * 8 * tm->n_triangle * 3);
		int vertexIdx = 0;
		for (int triIdx = 0; triIdx < tm->n_triangle; triIdx++) {
			TRIANGLE tri = tm->triangle_list[triIdx];
			for (int triVertex = 0; triVertex < 3; triVertex++) {
				bistro_exterior_vertices[materialIdx][vertexIdx++] = tri.position[triVertex].x;
				bistro_exterior_vertices[materialIdx][vertexIdx++] = tri.position[triVertex].y;
				bistro_exterior_vertices[materialIdx][vertexIdx++] = tri.position[triVertex].z;
				bistro_exterior_vertices[materialIdx][vertexIdx++] = tri.normal_vetcor[triVertex].x;
				bistro_exterior_vertices[materialIdx][vertexIdx++] = tri.normal_vetcor[triVertex].y;
				bistro_exterior_vertices[materialIdx][vertexIdx++] = tri.normal_vetcor[triVertex].z;
				bistro_exterior_vertices[materialIdx][vertexIdx++] = tri.texture_list[triVertex][0].u;
				bistro_exterior_vertices[materialIdx][vertexIdx++] = tri.texture_list[triVertex][0].v;
			}
		}
		bistro_exterior_n_triangles[materialIdx] = tm->n_triangle;
		if (materialIdx == 0) bistro_exterior_vertex_offset[materialIdx] = 0;
		else bistro_exterior_vertex_offset[materialIdx] = bistro_exterior_vertex_offset[materialIdx - 1] + 3 * bistro_exterior_n_triangles[materialIdx - 1];

		glGenBuffers(1, &bistro_exterior_VBO[materialIdx]);
		glBindBuffer(GL_ARRAY_BUFFER, bistro_exterior_VBO[materialIdx]);
		glBufferData(GL_ARRAY_BUFFER, bistro_exterior_n_triangles[materialIdx] * 3 * n_bytes_per_vertex, bistro_exterior_vertices[materialIdx], GL_STATIC_DRAW);
		free(bistro_exterior_vertices[materialIdx]);

		glGenVertexArrays(1, &bistro_exterior_VAO[materialIdx]);
		glBindVertexArray(bistro_exterior_VAO[materialIdx]);
		glBindBuffer(GL_ARRAY_BUFFER, bistro_exterior_VBO[materialIdx]);
		glVertexAttribPointer(INDEX_VERTEX_POSITION, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), BUFFER_OFFSET(0));
		glEnableVertexAttribArray(INDEX_VERTEX_POSITION);
		glVertexAttribPointer(INDEX_NORMAL, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), BUFFER_OFFSET(3 * sizeof(float)));
		glEnableVertexAttribArray(INDEX_NORMAL);
		glVertexAttribPointer(INDEX_TEX_COORD, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), BUFFER_OFFSET(6 * sizeof(float)));
		glEnableVertexAttribArray(INDEX_TEX_COORD);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);

		if ((materialIdx > 0) && (materialIdx % 100 == 0))
			fprintf(stdout, " * Loaded %d bistro exterior materials into graphics memory.\n", materialIdx / 100 * 100);
	}
	fprintf(stdout, " * Loaded %d bistro exterior materials into graphics memory.\n", scene.n_materials);

	bistro_exterior_texture_names = (GLuint*)malloc(sizeof(GLuint) * scene.n_textures);
	glGenTextures(scene.n_textures, bistro_exterior_texture_names);
	for (int texId = 0; texId < scene.n_textures; texId++) {
		glBindTexture(GL_TEXTURE_2D, bistro_exterior_texture_names[texId]);
		bool bReturn = readTexImage2D_from_file(scene.texture_file_name[texId]);
		if (bReturn) {
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
			flag_texture_mapping[texId] = true;
		}
		else { flag_texture_mapping[texId] = false; }
		glBindTexture(GL_TEXTURE_2D, 0);
	}
	fprintf(stdout, " * Loaded bistro exterior textures into graphics memory.\n");
	free(bistro_exterior_vertices);
}

void bindTexture(GLuint tex, int glTextureId, int texId) {
	if (INVALID_TEX_ID != (unsigned int)texId) {
		glActiveTexture(GL_TEXTURE0 + glTextureId);
		glBindTexture(GL_TEXTURE_2D, bistro_exterior_texture_names[texId]);
		glUniform1i(tex, glTextureId);
	}
}

void draw_bistro_exterior(void) {
	glUseProgram(h_ShaderProgram_TXPBR);
	ModelViewMatrix = ViewMatrix;
	ModelViewProjectionMatrix = ProjectionMatrix * ModelViewMatrix;
	ModelViewMatrixInvTrans = glm::transpose(glm::inverse(glm::mat3(ModelViewMatrix)));
	glUniformMatrix4fv(loc_ModelViewProjectionMatrix_TXPBR, 1, GL_FALSE, &ModelViewProjectionMatrix[0][0]);
	glUniformMatrix4fv(loc_ModelViewMatrix_TXPBR, 1, GL_FALSE, &ModelViewMatrix[0][0]);
	glUniformMatrix3fv(loc_ModelViewMatrixInvTrans_TXPBR, 1, GL_FALSE, &ModelViewMatrixInvTrans[0][0]);
	glUniform4fv(loc_cameraPos, 1, current_camera.pos);

	for (int materialIdx = 0; materialIdx < scene.n_materials; materialIdx++) {
		bindTexture(loc_material.diffuseTex, TEXTURE_INDEX_DIFFUSE, scene.material_list[materialIdx].diffuseTexId);
		bindTexture(loc_material.normalTex, TEXTURE_INDEX_NORMAL, scene.material_list[materialIdx].normalMapTexId);
		bindTexture(loc_material.specularTex, TEXTURE_INDEX_SPECULAR, scene.material_list[materialIdx].specularTexId);
		bindTexture(loc_material.emissiveTex, TEXTURE_INDEX_EMISSIVE, scene.material_list[materialIdx].emissiveTexId);
		glEnable(GL_TEXTURE_2D);
		glBindVertexArray(bistro_exterior_VAO[materialIdx]);
		glDrawArrays(GL_TRIANGLES, 0, 3 * bistro_exterior_n_triangles[materialIdx]);
		glBindVertexArray(0);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
	glUseProgram(0);
}

// skybox
GLuint skybox_VBO, skybox_VAO;
GLuint skybox_texture_name;
GLfloat cube_vertices[72][3] = {
	-1.0f,  1.0f, -1.0f,    1.0f,  1.0f, -1.0f,    1.0f,  1.0f,  1.0f,
	 1.0f,  1.0f,  1.0f,   -1.0f,  1.0f,  1.0f,   -1.0f,  1.0f, -1.0f,
	-1.0f, -1.0f, -1.0f,   -1.0f, -1.0f,  1.0f,    1.0f, -1.0f, -1.0f,
	 1.0f, -1.0f, -1.0f,   -1.0f, -1.0f,  1.0f,    1.0f, -1.0f,  1.0f,
	-1.0f, -1.0f,  1.0f,   -1.0f,  1.0f,  1.0f,    1.0f,  1.0f,  1.0f,
	 1.0f,  1.0f,  1.0f,    1.0f, -1.0f,  1.0f,   -1.0f, -1.0f,  1.0f,
	-1.0f,  1.0f, -1.0f,   -1.0f, -1.0f, -1.0f,    1.0f, -1.0f, -1.0f,
	 1.0f, -1.0f, -1.0f,    1.0f,  1.0f, -1.0f,   -1.0f,  1.0f, -1.0f,
	-1.0f, -1.0f,  1.0f,   -1.0f, -1.0f, -1.0f,   -1.0f,  1.0f, -1.0f,
	-1.0f,  1.0f, -1.0f,   -1.0f,  1.0f,  1.0f,   -1.0f, -1.0f,  1.0f,
	 1.0f, -1.0f, -1.0f,    1.0f, -1.0f,  1.0f,    1.0f,  1.0f,  1.0f,
	 1.0f,  1.0f,  1.0f,    1.0f,  1.0f, -1.0f,    1.0f, -1.0f, -1.0f,
	 0.0f, 0.0f,-1.0f,  0.0f, 0.0f,-1.0f,  0.0f, 0.0f,-1.0f,
	 0.0f, 0.0f,-1.0f,  0.0f, 0.0f,-1.0f,  0.0f, 0.0f,-1.0f,
	-1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f,
	-1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f,
	 1.0f, 0.0f, 0.0f,  1.0f, 0.0f, 0.0f,  1.0f, 0.0f, 0.0f,
	 1.0f, 0.0f, 0.0f,  1.0f, 0.0f, 0.0f,  1.0f, 0.0f, 0.0f,
	 0.0f, 0.0f, 1.0f,  0.0f, 0.0f, 1.0f,  0.0f, 0.0f, 1.0f,
	 0.0f, 0.0f, 1.0f,  0.0f, 0.0f, 1.0f,  0.0f, 0.0f, 1.0f,
	 0.0f, 1.0f, 0.0f,  0.0f, 1.0f, 0.0f,  0.0f, 1.0f, 0.0f,
	 0.0f, 1.0f, 0.0f,  0.0f, 1.0f, 0.0f,  0.0f, 1.0f, 0.0f,
	 0.0f,-1.0f, 0.0f,  0.0f,-1.0f, 0.0f,  0.0f,-1.0f, 0.0f,
	 0.0f,-1.0f, 0.0f,  0.0f,-1.0f, 0.0f,  0.0f,-1.0f, 0.0f
};

void readTexImage2DForCubeMap(const char* filename, GLenum texture_target) {
	FREE_IMAGE_FORMAT tx_file_format;
	FIBITMAP* tx_pixmap;
	int width, height;
	GLvoid* data;
	tx_file_format = FreeImage_GetFileType(filename, 0);
	tx_pixmap = FreeImage_Load(tx_file_format, filename);
	width = FreeImage_GetWidth(tx_pixmap);
	height = FreeImage_GetHeight(tx_pixmap);
	FreeImage_FlipVertical(tx_pixmap);
	data = FreeImage_GetBits(tx_pixmap);
	glTexImage2D(texture_target, 0, GL_RGB, width, height, 0, GL_BGR, GL_UNSIGNED_BYTE, data);
	FreeImage_Unload(tx_pixmap);
}

void prepare_skybox(void) {
	glGenVertexArrays(1, &skybox_VAO);
	glGenBuffers(1, &skybox_VBO);
	glBindVertexArray(skybox_VAO);
	glBindBuffer(GL_ARRAY_BUFFER, skybox_VBO);
	glBufferData(GL_ARRAY_BUFFER, 36 * 3 * sizeof(GLfloat), &cube_vertices[0][0], GL_STATIC_DRAW);
	glEnableVertexAttribArray(INDEX_VERTEX_POSITION);
	glVertexAttribPointer(INDEX_VERTEX_POSITION, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), BUFFER_OFFSET(0));
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	glGenTextures(1, &skybox_texture_name);
	glBindTexture(GL_TEXTURE_CUBE_MAP, skybox_texture_name);
	readTexImage2DForCubeMap("Scene/Cubemap/px.png", GL_TEXTURE_CUBE_MAP_POSITIVE_X);
	readTexImage2DForCubeMap("Scene/Cubemap/nx.png", GL_TEXTURE_CUBE_MAP_NEGATIVE_X);
	readTexImage2DForCubeMap("Scene/Cubemap/py.png", GL_TEXTURE_CUBE_MAP_POSITIVE_Y);
	readTexImage2DForCubeMap("Scene/Cubemap/ny.png", GL_TEXTURE_CUBE_MAP_NEGATIVE_Y);
	readTexImage2DForCubeMap("Scene/Cubemap/pz.png", GL_TEXTURE_CUBE_MAP_POSITIVE_Z);
	readTexImage2DForCubeMap("Scene/Cubemap/nz.png", GL_TEXTURE_CUBE_MAP_NEGATIVE_Z);
	fprintf(stdout, " * Loaded cube map textures into graphics memory.\n\n");
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
}

void draw_skybox(void) {
	glUseProgram(h_ShaderProgram_skybox);
	glUniform1i(loc_cubemap_skybox, TEXTURE_INDEX_SKYMAP);
	ModelViewMatrix = ViewMatrix * glm::mat4(1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
	ModelViewMatrix = glm::scale(ModelViewMatrix, glm::vec3(20000.0f, 20000.0f, 20000.0f));
	ModelViewProjectionMatrix = ProjectionMatrix * ModelViewMatrix;
	glUniformMatrix4fv(loc_ModelViewProjectionMatrix_SKY, 1, GL_FALSE, &ModelViewProjectionMatrix[0][0]);
	glBindVertexArray(skybox_VAO);
	glActiveTexture(GL_TEXTURE0 + TEXTURE_INDEX_SKYMAP);
	glBindTexture(GL_TEXTURE_CUBE_MAP, skybox_texture_name);
	glFrontFace(GL_CW);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
	glDisable(GL_CULL_FACE);
	glUseProgram(0);
}

// ==================== MY OBJECTS (wireframe) ====================

// VNT stride = 8 floats (pos3 + norm3 + tex2), only position attr needed for wireframe
#define VNT_BYTES_PER_VERTEX (8 * (int)sizeof(float))
#define VNT_BYTES_PER_TRI   (3 * VNT_BYTES_PER_VERTEX)

// bind position attrib only (index 0) with stride 8 floats
void setup_vao_vnt(GLuint vao, GLuint vbo) {
	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glVertexAttribPointer(INDEX_VERTEX_POSITION, 3, GL_FLOAT, GL_FALSE, VNT_BYTES_PER_VERTEX, BUFFER_OFFSET(0));
	glEnableVertexAttribArray(INDEX_VERTEX_POSITION);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}

// tiger: 12 animation frames, dynamic object
#define N_TIGER_FRAMES 12
GLuint my_tiger_VBO, my_tiger_VAO;
int my_tiger_n_triangles[N_TIGER_FRAMES];
int my_tiger_vertex_offset[N_TIGER_FRAMES];
int cur_frame_tiger = 0;

void prepare_my_tiger(void) {
	int total_tri = 0;
	GLfloat* verts[N_TIGER_FRAMES];
	char filename[256];
	for (int i = 0; i < N_TIGER_FRAMES; i++) {
		sprintf(filename, "Data/dynamic_objects/tiger/Tiger_%d%d_triangles_vnt.geom", i / 10, i % 10);
		my_tiger_n_triangles[i] = read_geometry_vnt(&verts[i], VNT_BYTES_PER_TRI, filename);
		if (i == 0) my_tiger_vertex_offset[i] = 0;
		else my_tiger_vertex_offset[i] = my_tiger_vertex_offset[i - 1] + 3 * my_tiger_n_triangles[i - 1];
		total_tri += my_tiger_n_triangles[i];
	}
	glGenBuffers(1, &my_tiger_VBO);
	glBindBuffer(GL_ARRAY_BUFFER, my_tiger_VBO);
	glBufferData(GL_ARRAY_BUFFER, total_tri * VNT_BYTES_PER_TRI, NULL, GL_STATIC_DRAW);
	for (int i = 0; i < N_TIGER_FRAMES; i++) {
		glBufferSubData(GL_ARRAY_BUFFER, my_tiger_vertex_offset[i] * VNT_BYTES_PER_VERTEX,
			my_tiger_n_triangles[i] * VNT_BYTES_PER_TRI, verts[i]);
		free(verts[i]);
	}
	glGenVertexArrays(1, &my_tiger_VAO);
	setup_vao_vnt(my_tiger_VAO, my_tiger_VBO);
	fprintf(stdout, " * Loaded tiger wireframe.\n");
}

// wolf: 17 animation frames, dynamic object
#define N_WOLF_FRAMES 17
GLuint my_wolf_VBO, my_wolf_VAO;
int my_wolf_n_triangles[N_WOLF_FRAMES];
int my_wolf_vertex_offset[N_WOLF_FRAMES];
int cur_frame_wolf = 0;

void prepare_my_wolf(void) {
	int total_tri = 0;
	GLfloat* verts[N_WOLF_FRAMES];
	char filename[256];
	for (int i = 0; i < N_WOLF_FRAMES; i++) {
		sprintf(filename, "Data/dynamic_objects/wolf/wolf_%02d_vnt.geom", i);
		my_wolf_n_triangles[i] = read_geometry_vnt(&verts[i], VNT_BYTES_PER_TRI, filename);
		if (i == 0) my_wolf_vertex_offset[i] = 0;
		else my_wolf_vertex_offset[i] = my_wolf_vertex_offset[i - 1] + 3 * my_wolf_n_triangles[i - 1];
		total_tri += my_wolf_n_triangles[i];
	}
	glGenBuffers(1, &my_wolf_VBO);
	glBindBuffer(GL_ARRAY_BUFFER, my_wolf_VBO);
	glBufferData(GL_ARRAY_BUFFER, total_tri * VNT_BYTES_PER_TRI, NULL, GL_STATIC_DRAW);
	for (int i = 0; i < N_WOLF_FRAMES; i++) {
		glBufferSubData(GL_ARRAY_BUFFER, my_wolf_vertex_offset[i] * VNT_BYTES_PER_VERTEX,
			my_wolf_n_triangles[i] * VNT_BYTES_PER_TRI, verts[i]);
		free(verts[i]);
	}
	glGenVertexArrays(1, &my_wolf_VAO);
	setup_vao_vnt(my_wolf_VAO, my_wolf_VBO);
	fprintf(stdout, " * Loaded wolf wireframe.\n");
}

// spider: 16 animation frames, dynamic object
#define N_SPIDER_FRAMES 16
GLuint my_spider_VBO, my_spider_VAO;
int my_spider_n_triangles[N_SPIDER_FRAMES];
int my_spider_vertex_offset[N_SPIDER_FRAMES];
int cur_frame_spider = 0;

void prepare_my_spider(void) {
	int total_tri = 0;
	GLfloat* verts[N_SPIDER_FRAMES];
	char filename[256];
	for (int i = 0; i < N_SPIDER_FRAMES; i++) {
		sprintf(filename, "Data/dynamic_objects/spider/spider_vnt_%d%d.geom", i / 10, i % 10);
		my_spider_n_triangles[i] = read_geometry_vnt(&verts[i], VNT_BYTES_PER_TRI, filename);
		if (i == 0) my_spider_vertex_offset[i] = 0;
		else my_spider_vertex_offset[i] = my_spider_vertex_offset[i - 1] + 3 * my_spider_n_triangles[i - 1];
		total_tri += my_spider_n_triangles[i];
	}
	glGenBuffers(1, &my_spider_VBO);
	glBindBuffer(GL_ARRAY_BUFFER, my_spider_VBO);
	glBufferData(GL_ARRAY_BUFFER, total_tri * VNT_BYTES_PER_TRI, NULL, GL_STATIC_DRAW);
	for (int i = 0; i < N_SPIDER_FRAMES; i++) {
		glBufferSubData(GL_ARRAY_BUFFER, my_spider_vertex_offset[i] * VNT_BYTES_PER_VERTEX,
			my_spider_n_triangles[i] * VNT_BYTES_PER_TRI, verts[i]);
		free(verts[i]);
	}
	glGenVertexArrays(1, &my_spider_VAO);
	setup_vao_vnt(my_spider_VAO, my_spider_VBO);
	fprintf(stdout, " * Loaded spider wireframe.\n");
}

// cow: static object
GLuint my_cow_VBO, my_cow_VAO;
int my_cow_n_triangles;

void prepare_my_cow(void) {
	GLfloat* verts;
	my_cow_n_triangles = read_geometry_vnt(&verts, VNT_BYTES_PER_TRI, "Data/static_objects/cow_vnt.geom");
	glGenBuffers(1, &my_cow_VBO);
	glBindBuffer(GL_ARRAY_BUFFER, my_cow_VBO);
	glBufferData(GL_ARRAY_BUFFER, my_cow_n_triangles * VNT_BYTES_PER_TRI, verts, GL_STATIC_DRAW);
	free(verts);
	glGenVertexArrays(1, &my_cow_VAO);
	setup_vao_vnt(my_cow_VAO, my_cow_VBO);
	fprintf(stdout, " * Loaded cow wireframe.\n");
}

// ironman: static object
GLuint my_ironman_VBO, my_ironman_VAO;
int my_ironman_n_triangles;

void prepare_my_ironman(void) {
	GLfloat* verts;
	my_ironman_n_triangles = read_geometry_vnt(&verts, VNT_BYTES_PER_TRI, "Data/static_objects/ironman_vnt.geom");
	glGenBuffers(1, &my_ironman_VBO);
	glBindBuffer(GL_ARRAY_BUFFER, my_ironman_VBO);
	glBufferData(GL_ARRAY_BUFFER, my_ironman_n_triangles * VNT_BYTES_PER_TRI, verts, GL_STATIC_DRAW);
	free(verts);
	glGenVertexArrays(1, &my_ironman_VAO);
	setup_vao_vnt(my_ironman_VAO, my_ironman_VBO);
	fprintf(stdout, " * Loaded ironman wireframe.\n");
}

// godzilla: static object
GLuint my_godzilla_VBO, my_godzilla_VAO;
int my_godzilla_n_triangles;

void prepare_my_godzilla(void) {
	GLfloat* verts;
	my_godzilla_n_triangles = read_geometry_vnt(&verts, VNT_BYTES_PER_TRI, "Data/static_objects/godzilla_vnt.geom");
	glGenBuffers(1, &my_godzilla_VBO);
	glBindBuffer(GL_ARRAY_BUFFER, my_godzilla_VBO);
	glBufferData(GL_ARRAY_BUFFER, my_godzilla_n_triangles * VNT_BYTES_PER_TRI, verts, GL_STATIC_DRAW);
	free(verts);
	glGenVertexArrays(1, &my_godzilla_VAO);
	setup_vao_vnt(my_godzilla_VAO, my_godzilla_VBO);
	fprintf(stdout, " * Loaded godzilla wireframe.\n");
}

// ==================== tiger path (Catmull-Rom spline) ====================
// 16 control points looping around the bistro scene, z varies for elevation
#define N_TIGER_CP 16
glm::vec3 tiger_cp[N_TIGER_CP] = {
	glm::vec3(  300.0f,  -200.0f,   0.0f),  // 0  : start
	glm::vec3( 1000.0f,  -100.0f,  30.0f),  // 1  : gentle rise
	glm::vec3( 1800.0f,   200.0f,   0.0f),  // 2  : curve right
	glm::vec3( 2400.0f,  -400.0f,  50.0f),  // 3  : bistro plaza
	glm::vec3( 3200.0f, -1200.0f,   0.0f),  // 4  : south-east
	glm::vec3( 3500.0f,  -600.0f,  80.0f),  // 5  : up the hill
	glm::vec3( 3300.0f,   100.0f,  40.0f),  // 6  : U-turn U
	glm::vec3( 2800.0f,   700.0f,   0.0f),  // 7  : heading north
	glm::vec3( 2000.0f,   900.0f,  60.0f),  // 8  : through trees
	glm::vec3( 1400.0f,   500.0f,   0.0f),  // 9  : back to center
	glm::vec3(  600.0f,   800.0f,  20.0f),  // 10 : west side
	glm::vec3( -200.0f,   400.0f,  70.0f),  // 11 : elevated point
	glm::vec3( -500.0f,  -300.0f,  30.0f),  // 12 : south-west
	glm::vec3( -300.0f, -1000.0f,   0.0f),  // 13 : south
	glm::vec3(  200.0f,  -800.0f,  40.0f),  // 14 : returning
	glm::vec3(  300.0f,  -200.0f,   0.0f),  // 15 : same as 0, closes the loop
};

// Catmull-Rom interpolation: t in [0,1], needs 4 surrounding control points
glm::vec3 catmull_rom(glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, glm::vec3 p3, float t) {
	float t2 = t * t, t3 = t2 * t;
	return 0.5f * ((2.0f * p1) +
		(-p0 + p2) * t +
		(2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
		(-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
}

float tiger_speed = 3.0f;     // spline
int tiger_seg = 0;            // index
float tiger_seg_t = 0.0f;     // [0,1]
glm::vec3 tiger_position(300.0f, -200.0f, 0.0f);
float tiger_heading = 0.0f;
bool animation_running = true;
int timestamp_scene = 0;

// wolf ( + )
glm::vec3 wolf_center(2500.0f, -500.0f, 0.0f);
float wolf_radius = 600.0f;
float wolf_angle = 0.0f;
glm::vec3 wolf_position;
float wolf_heading = 0.0f;

// spider (figure-8)
glm::vec3 spider_center(-500.0f, -1000.0f, 0.0f);
float spider_t = 0.0f;
glm::vec3 spider_position;
float spider_heading = 0.0f;

void update_tiger_path(void) {
	// 4 surrounding points with modular index for seamless loop
	int n = N_TIGER_CP - 1; // 15 segments (last point == first, so valid segments = N-1)
	int i0 = (tiger_seg - 1 + n) % n;
	int i1 = tiger_seg;
	int i2 = (tiger_seg + 1) % n;
	int i3 = (tiger_seg + 2) % n;

	// arc length estimate for uniform speed
	float est_len = glm::length(tiger_cp[i2] - tiger_cp[i1]);
	if (est_len < 1.0f) est_len = 1.0f;
	tiger_seg_t += tiger_speed / est_len;

	if (tiger_seg_t >= 1.0f) {
		tiger_seg_t -= 1.0f;
		tiger_seg = (tiger_seg + 1) % n;
		// recalculate surrounding indices
		i0 = (tiger_seg - 1 + n) % n;
		i1 = tiger_seg;
		i2 = (tiger_seg + 1) % n;
		i3 = (tiger_seg + 2) % n;
	}

	tiger_position = catmull_rom(tiger_cp[i0], tiger_cp[i1], tiger_cp[i2], tiger_cp[i3], tiger_seg_t);

	// forward direction = spline tangent (finite difference)
	float dt = 0.01f;
	glm::vec3 next = catmull_rom(tiger_cp[i0], tiger_cp[i1], tiger_cp[i2], tiger_cp[i3],
		fminf(tiger_seg_t + dt, 1.0f));
	glm::vec3 dir = next - tiger_position;
	if (glm::length(dir) > 0.001f) {
		dir = glm::normalize(dir);
		tiger_heading = atan2f(dir.x, -dir.y);
	}
}

void update_wolf_path(void) {
	wolf_angle += 0.02f;
	wolf_position = wolf_center + glm::vec3(cosf(wolf_angle) * wolf_radius, sinf(wolf_angle) * wolf_radius, 50.0f * sinf(wolf_angle * 2.0f));
	wolf_heading = wolf_angle + 1.5708f; // tangent
}

void update_spider_path(void) {
	spider_t += 0.015f;
	spider_position = spider_center + glm::vec3(500.0f * sinf(spider_t), 400.0f * sinf(2.0f * spider_t), 0.0f);
	glm::vec3 vel(500.0f * cosf(spider_t), 800.0f * cosf(2.0f * spider_t), 0.0f);
	spider_heading = atan2f(vel.x, -vel.y);
}

// tiger model matrix - scene is Z-up, tiger model is also Z-up so no axis flip needed
glm::mat4 get_tiger_model_matrix(void) {
	glm::mat4 M(1.0f);
	M = glm::translate(M, tiger_position);
	M = glm::rotate(M, tiger_heading, glm::vec3(0.0f, 0.0f, 1.0f));
	M = glm::scale(M, glm::vec3(2.0f, 2.0f, 2.0f));
	return M;
}

// Camera_T: placed at tiger eye position, follows tiger transform
void update_tiger_eye_camera(void) {
	glm::mat4 M = get_tiger_model_matrix();
	// eye position: model-local (0, -88, 62)
	glm::vec4 eye_w = M * glm::vec4(0.0f, -88.0f, 62.0f, 1.0f);
	// tiger faces -Y in model space
	glm::vec4 fwd_w = M * glm::vec4(0.0f, -1.0f, 0.0f, 0.0f);
	glm::vec3 eye(eye_w);
	glm::vec3 at = eye + glm::vec3(fwd_w) * 500.0f;

	Camera* cam = &camera_info[CAMERA_T];
	compute_camera_frame(cam, eye, at);

	// head bobbing: pitch oscillation while walking
	float bob = sinf(timestamp_scene * 0.08f) * 3.0f * TO_RADIAN;
	camera_pitch(cam, bob);
}

// Camera_G: stays behind and above tiger, always looking at it
void update_tiger_follow_camera(void) {
	glm::mat4 M = get_tiger_model_matrix();
	glm::vec4 tpos_w = M * glm::vec4(0.0f, 0.0f, 30.0f, 1.0f);
	glm::vec4 fwd_w = M * glm::vec4(0.0f, -1.0f, 0.0f, 0.0f);
	glm::vec3 tpos(tpos_w);
	glm::vec3 fwd = glm::normalize(glm::vec3(fwd_w));
	// behind tiger + elevated
	glm::vec3 eye = tpos - fwd * 600.0f + glm::vec3(0.0f, 0.0f, 250.0f);
	compute_camera_frame(&camera_info[CAMERA_G], eye, tpos);
}

// CCTV tracking: Camera_U rotates toward tiger when tiger is within 65-degree FOV
void update_tracking_camera(void) {
	Camera* cam = &camera_info[CAMERA_U];
	glm::vec3 cam_pos(camera_u_initial.pos[0], camera_u_initial.pos[1], camera_u_initial.pos[2]);
	glm::vec3 init_n(camera_u_initial.naxis[0], camera_u_initial.naxis[1], camera_u_initial.naxis[2]);
	glm::vec3 to_tiger = tiger_position - cam_pos;
	float dist = glm::length(to_tiger);
	if (dist < 1.0f) return;

	glm::vec3 dir = to_tiger / dist;
	float cos_a = glm::dot(-init_n, dir);

	if (cos_a > cosf(65.0f * TO_RADIAN)) {
		// tiger in view: aim camera at it
		compute_camera_frame(cam, cam_pos, tiger_position);
		cctv_tracking = true;
	}
	else if (cctv_tracking) {
		// tiger left view: restore original orientation
		memcpy(cam->uaxis, camera_u_initial.uaxis, 3 * sizeof(float));
		memcpy(cam->vaxis, camera_u_initial.vaxis, 3 * sizeof(float));
		memcpy(cam->naxis, camera_u_initial.naxis, 3 * sizeof(float));
		cctv_tracking = false;
	}
}

// all my wireframe objects drawn here (assignment requirement: called from display())
void draw_my_objects_20211551(void) {
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glUseProgram(h_ShaderProgram_simple);

	glm::mat4 MM;

	// tiger - orange, Catmull-Rom spline path
	MM = get_tiger_model_matrix();
	ModelViewProjectionMatrix = ProjectionMatrix * ViewMatrix * MM;
	glUniformMatrix4fv(loc_ModelViewProjectionMatrix, 1, GL_FALSE, &ModelViewProjectionMatrix[0][0]);
	glUniform3f(loc_primitive_color, 1.0f, 0.5f, 0.0f);
	glBindVertexArray(my_tiger_VAO);
	glDrawArrays(GL_TRIANGLES, my_tiger_vertex_offset[cur_frame_tiger], 3 * my_tiger_n_triangles[cur_frame_tiger]);
	glBindVertexArray(0);

	// wolf - gray, circular orbit with Z oscillation
	MM = glm::translate(glm::mat4(1.0f), wolf_position);
	MM = glm::rotate(MM, wolf_heading, glm::vec3(0.0f, 0.0f, 1.0f));
	MM = glm::rotate(MM, 90.0f * TO_RADIAN, glm::vec3(1.0f, 0.0f, 0.0f)); // Y-up -> Z-up
	MM = glm::scale(MM, glm::vec3(200.0f, 200.0f, 200.0f));
	ModelViewProjectionMatrix = ProjectionMatrix * ViewMatrix * MM;
	glUniformMatrix4fv(loc_ModelViewProjectionMatrix, 1, GL_FALSE, &ModelViewProjectionMatrix[0][0]);
	glUniform3f(loc_primitive_color, 0.6f, 0.6f, 0.7f);
	glBindVertexArray(my_wolf_VAO);
	glDrawArrays(GL_TRIANGLES, my_wolf_vertex_offset[cur_frame_wolf], 3 * my_wolf_n_triangles[cur_frame_wolf]);
	glBindVertexArray(0);

	// spider - green, figure-8 Lissajous path
	MM = glm::translate(glm::mat4(1.0f), spider_position);
	MM = glm::rotate(MM, spider_heading, glm::vec3(0.0f, 0.0f, 1.0f));
	MM = glm::rotate(MM, 90.0f * TO_RADIAN, glm::vec3(1.0f, 0.0f, 0.0f));
	MM = glm::scale(MM, glm::vec3(100.0f, 100.0f, 100.0f));
	ModelViewProjectionMatrix = ProjectionMatrix * ViewMatrix * MM;
	glUniformMatrix4fv(loc_ModelViewProjectionMatrix, 1, GL_FALSE, &ModelViewProjectionMatrix[0][0]);
	glUniform3f(loc_primitive_color, 0.2f, 0.8f, 0.2f);
	glBindVertexArray(my_spider_VAO);
	glDrawArrays(GL_TRIANGLES, my_spider_vertex_offset[cur_frame_spider], 3 * my_spider_n_triangles[cur_frame_spider]);
	glBindVertexArray(0);

	// cow #1 - red, near bistro entrance
	MM = glm::translate(glm::mat4(1.0f), glm::vec3(800.0f, 300.0f, 0.0f));
	MM = glm::rotate(MM, 90.0f * TO_RADIAN, glm::vec3(1.0f, 0.0f, 0.0f));
	MM = glm::scale(MM, glm::vec3(30.0f, 30.0f, 30.0f));
	ModelViewProjectionMatrix = ProjectionMatrix * ViewMatrix * MM;
	glUniformMatrix4fv(loc_ModelViewProjectionMatrix, 1, GL_FALSE, &ModelViewProjectionMatrix[0][0]);
	glUniform3f(loc_primitive_color, 0.9f, 0.2f, 0.2f);
	glBindVertexArray(my_cow_VAO);
	glDrawArrays(GL_TRIANGLES, 0, 3 * my_cow_n_triangles);
	glBindVertexArray(0);

	// cow #2 - pink, rotated, west side
	MM = glm::translate(glm::mat4(1.0f), glm::vec3(-500.0f, -500.0f, 0.0f));
	MM = glm::rotate(MM, 135.0f * TO_RADIAN, glm::vec3(0.0f, 0.0f, 1.0f));
	MM = glm::rotate(MM, 90.0f * TO_RADIAN, glm::vec3(1.0f, 0.0f, 0.0f));
	MM = glm::scale(MM, glm::vec3(25.0f, 25.0f, 25.0f));
	ModelViewProjectionMatrix = ProjectionMatrix * ViewMatrix * MM;
	glUniformMatrix4fv(loc_ModelViewProjectionMatrix, 1, GL_FALSE, &ModelViewProjectionMatrix[0][0]);
	glUniform3f(loc_primitive_color, 1.0f, 0.5f, 0.7f);
	glBindVertexArray(my_cow_VAO);
	glDrawArrays(GL_TRIANGLES, 0, 3 * my_cow_n_triangles);
	glBindVertexArray(0);

	// ironman #1 - yellow, south-east
	MM = glm::translate(glm::mat4(1.0f), glm::vec3(1200.0f, -800.0f, 0.0f));
	MM = glm::rotate(MM, 90.0f * TO_RADIAN, glm::vec3(1.0f, 0.0f, 0.0f));
	MM = glm::scale(MM, glm::vec3(150.0f, 150.0f, 150.0f));
	ModelViewProjectionMatrix = ProjectionMatrix * ViewMatrix * MM;
	glUniformMatrix4fv(loc_ModelViewProjectionMatrix, 1, GL_FALSE, &ModelViewProjectionMatrix[0][0]);
	glUniform3f(loc_primitive_color, 1.0f, 0.9f, 0.1f);
	glBindVertexArray(my_ironman_VAO);
	glDrawArrays(GL_TRIANGLES, 0, 3 * my_ironman_n_triangles);
	glBindVertexArray(0);

	// ironman #2 - blue, north-west
	MM = glm::translate(glm::mat4(1.0f), glm::vec3(-300.0f, 700.0f, 0.0f));
	MM = glm::rotate(MM, -90.0f * TO_RADIAN, glm::vec3(0.0f, 0.0f, 1.0f));
	MM = glm::rotate(MM, 90.0f * TO_RADIAN, glm::vec3(1.0f, 0.0f, 0.0f));
	MM = glm::scale(MM, glm::vec3(120.0f, 120.0f, 120.0f));
	ModelViewProjectionMatrix = ProjectionMatrix * ViewMatrix * MM;
	glUniformMatrix4fv(loc_ModelViewProjectionMatrix, 1, GL_FALSE, &ModelViewProjectionMatrix[0][0]);
	glUniform3f(loc_primitive_color, 0.2f, 0.4f, 1.0f);
	glBindVertexArray(my_ironman_VAO);
	glDrawArrays(GL_TRIANGLES, 0, 3 * my_ironman_n_triangles);
	glBindVertexArray(0);

	// godzilla - purple, east area
	MM = glm::translate(glm::mat4(1.0f), glm::vec3(2000.0f, 500.0f, 0.0f));
	MM = glm::rotate(MM, 90.0f * TO_RADIAN, glm::vec3(1.0f, 0.0f, 0.0f));
	MM = glm::scale(MM, glm::vec3(5.0f, 5.0f, 5.0f));
	ModelViewProjectionMatrix = ProjectionMatrix * ViewMatrix * MM;
	glUniformMatrix4fv(loc_ModelViewProjectionMatrix, 1, GL_FALSE, &ModelViewProjectionMatrix[0][0]);
	glUniform3f(loc_primitive_color, 0.7f, 0.2f, 0.9f);
	glBindVertexArray(my_godzilla_VAO);
	glDrawArrays(GL_TRIANGLES, 0, 3 * my_godzilla_n_triangles);
	glBindVertexArray(0);

	glUseProgram(0);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

// render everything for one camera view
void render_scene_all(void) {
	draw_grid();
	draw_axes();
	draw_bistro_exterior();
	draw_skybox();
	draw_my_objects_20211551();
}

void set_view_from_camera(Camera* cam) {
	ViewMatrix = glm::mat4(
		cam->uaxis[0], cam->vaxis[0], cam->naxis[0], 0.0f,
		cam->uaxis[1], cam->vaxis[1], cam->naxis[1], 0.0f,
		cam->uaxis[2], cam->vaxis[2], cam->naxis[2], 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f);
	ViewMatrix = glm::translate(ViewMatrix, glm::vec3(-cam->pos[0], -cam->pos[1], -cam->pos[2]));
}

// ==================== display ====================
void display(void) {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if (cctv_mode) {
		// 3x2 grid: top=u,i,o / bottom=p,t,g
		int w = glutGet(GLUT_WINDOW_WIDTH);
		int h = glutGet(GLUT_WINDOW_HEIGHT);
		int vw = w / 3, vh = h / 2;

		int cam_list[6] = { CAMERA_U, CAMERA_I, CAMERA_O, CAMERA_P, CAMERA_T, CAMERA_G };
		for (int idx = 0; idx < 6; idx++) {
			int col = idx % 3;
			int row = idx / 3;
			int vx = col * vw;
			int vy = (1 - row) * vh;

			glViewport(vx, vy, vw, vh);
			glEnable(GL_SCISSOR_TEST);
			glScissor(vx, vy, vw, vh);
			glClear(GL_DEPTH_BUFFER_BIT);
			glDisable(GL_SCISSOR_TEST);

			Camera* cam = &camera_info[cam_list[idx]];
			set_view_from_camera(cam);
			ProjectionMatrix = glm::perspective(cam->fovy, (float)vw / (float)vh, cam->near_c, cam->far_c);
			ViewProjectionMatrix = ProjectionMatrix * ViewMatrix;

			// sync current_camera for bistro shader camPos
			memcpy(&current_camera, cam, sizeof(Camera));
			render_scene_all();
		}
		// restore full viewport
		glViewport(0, 0, w, h);
	}
	else {
		render_scene_all();
	}

	glutSwapBuffers();
}

// ==================== input state ====================
int prev_mouse_x = 0, prev_mouse_y = 0;
int mouse_button_pressed = -1;
int ctrl_pressed = 0;

// ==================== keyboard ====================
void keyboard(unsigned char key, int x, int y) {
	switch (key) {
	case 'f':
		b_draw_grid = !b_draw_grid;
		glutPostRedisplay();
		break;
	case 'u':
		current_camera_index = CAMERA_U;
		set_current_camera(CAMERA_U);
		glutPostRedisplay();
		break;
	case 'i':
		current_camera_index = CAMERA_I;
		set_current_camera(CAMERA_I);
		glutPostRedisplay();
		break;
	case 'o':
		current_camera_index = CAMERA_O;
		set_current_camera(CAMERA_O);
		glutPostRedisplay();
		break;
	case 'p':
		current_camera_index = CAMERA_P;
		set_current_camera(CAMERA_P);
		glutPostRedisplay();
		break;
	case 'a':
		current_camera_index = CAMERA_A;
		set_current_camera(CAMERA_A);
		glutPostRedisplay();
		break;
	case 't':
		current_camera_index = CAMERA_T;
		set_current_camera(CAMERA_T);
		glutPostRedisplay();
		break;
	case 'g':
		current_camera_index = CAMERA_G;
		set_current_camera(CAMERA_G);
		glutPostRedisplay();
		break;
	case 'c':
		cctv_mode = !cctv_mode;
		if (!cctv_mode) {
			// restore previous camera when leaving CCTV
			set_current_camera(current_camera_index);
		}
		glutPostRedisplay();
		break;
	case ' ': // SPACE
		animation_running = !animation_running;
		break;
	case 27:
		glutLeaveMainLoop();
		break;
	}
}

// arrow keys + PageUp/Down move Camera_A along its own axes
void special_key(int key, int x, int y) {
	if (current_camera_index != CAMERA_A) return;

	Camera* cam = &camera_info[CAMERA_A];
	float speed = 50.0f;
	glm::vec3 u(cam->uaxis[0], cam->uaxis[1], cam->uaxis[2]);
	glm::vec3 v(cam->vaxis[0], cam->vaxis[1], cam->vaxis[2]);
	glm::vec3 n(cam->naxis[0], cam->naxis[1], cam->naxis[2]);

	switch (key) {
	case GLUT_KEY_UP:    // forward (-n direction)
		cam->pos[0] -= n.x * speed; cam->pos[1] -= n.y * speed; cam->pos[2] -= n.z * speed;
		break;
	case GLUT_KEY_DOWN:  // backward (+n direction)
		cam->pos[0] += n.x * speed; cam->pos[1] += n.y * speed; cam->pos[2] += n.z * speed;
		break;
	case GLUT_KEY_LEFT:  // strafe left (-u direction)
		cam->pos[0] -= u.x * speed; cam->pos[1] -= u.y * speed; cam->pos[2] -= u.z * speed;
		break;
	case GLUT_KEY_RIGHT: // strafe right (+u direction)
		cam->pos[0] += u.x * speed; cam->pos[1] += u.y * speed; cam->pos[2] += u.z * speed;
		break;
	case GLUT_KEY_PAGE_UP:   // move up (+v direction)
		cam->pos[0] += v.x * speed; cam->pos[1] += v.y * speed; cam->pos[2] += v.z * speed;
		break;
	case GLUT_KEY_PAGE_DOWN: // move down (-v direction)
		cam->pos[0] -= v.x * speed; cam->pos[1] -= v.y * speed; cam->pos[2] -= v.z * speed;
		break;
	}
	set_current_camera(CAMERA_A);
	glutPostRedisplay();
}

// ==================== mouse ====================
void mouse_func(int button, int state, int x, int y) {
	// scroll wheel zoom (CTRL + scroll only)
	if (button == 3 || button == 4) {
		if (state == GLUT_DOWN) {
			int mod = glutGetModifiers();
			if (mod & GLUT_ACTIVE_CTRL) {
				Camera* cam;
				if (current_camera_index >= CAMERA_U && current_camera_index <= CAMERA_P)
					cam = &camera_info[current_camera_index];
				else if (current_camera_index == CAMERA_A)
					cam = &camera_info[CAMERA_A];
				else return;

				float delta = (button == 3) ? -1.0f : 1.0f; // 3=up=zoom in, 4=down=zoom out
				cam->fovy += delta * TO_RADIAN;
				// clamp fovy range
				if (cam->fovy < 5.0f * TO_RADIAN) cam->fovy = 5.0f * TO_RADIAN;
				if (cam->fovy > 120.0f * TO_RADIAN) cam->fovy = 120.0f * TO_RADIAN;
				set_current_camera(current_camera_index);
				glutPostRedisplay();
			}
		}
		return;
	}

	if (state == GLUT_DOWN) {
		mouse_button_pressed = button;
		ctrl_pressed = (glutGetModifiers() & GLUT_ACTIVE_CTRL) ? 1 : 0;
		prev_mouse_x = x;
		prev_mouse_y = y;
	}
	else {
		mouse_button_pressed = -1;
	}
}

void motion_func(int x, int y) {
	int dx = x - prev_mouse_x;
	int dy = y - prev_mouse_y;
	prev_mouse_x = x;
	prev_mouse_y = y;

	if (mouse_button_pressed < 0) return;

	// obs cameras: CTRL + left drag = pitch/yaw
	if (current_camera_index >= CAMERA_U && current_camera_index <= CAMERA_P) {
		if (ctrl_pressed && mouse_button_pressed == GLUT_LEFT_BUTTON) {
			Camera* cam = &camera_info[current_camera_index];
			if (dy != 0) camera_pitch(cam, dy * 0.003f); // = pitch
			if (dx != 0) camera_yaw(cam, dx * 0.003f);   // = yaw
			set_current_camera(current_camera_index);
			glutPostRedisplay();
		}
	}
	// free cam: left drag = pitch/yaw, right drag = roll
	else if (current_camera_index == CAMERA_A) {
		Camera* cam = &camera_info[CAMERA_A];
		if (mouse_button_pressed == GLUT_LEFT_BUTTON) {
			if (dy != 0) camera_pitch(cam, dy * 0.003f);
			if (dx != 0) camera_yaw(cam, dx * 0.003f);
		}
		else if (mouse_button_pressed == GLUT_RIGHT_BUTTON) {
			if (dx != 0) camera_roll(cam, dx * 0.003f);
		}
		set_current_camera(CAMERA_A);
		glutPostRedisplay();
	}
}

// ==================== timer ====================
void timer_scene(int value) {
	if (animation_running) {
		timestamp_scene++;
		cur_frame_tiger = (timestamp_scene / 3) % N_TIGER_FRAMES;
		cur_frame_wolf = (timestamp_scene / 2) % N_WOLF_FRAMES;
		cur_frame_spider = (timestamp_scene / 2) % N_SPIDER_FRAMES;

		update_tiger_path();
		update_wolf_path();
		update_spider_path();
	}

	// always update dynamic cameras even when animation is paused
	update_tiger_eye_camera();
	update_tiger_follow_camera();

	if (cctv_mode) update_tracking_camera();

	// always update dynamic cameras even when animation is paused
	if (current_camera_index == CAMERA_T || current_camera_index == CAMERA_G) {
		set_current_camera(current_camera_index);
	}

	glutPostRedisplay();
	glutTimerFunc(10, timer_scene, 0);
}

// ==================== reshape ====================
void reshape(int width, int height) {
	glViewport(0, 0, width, height);
	current_camera.aspect_ratio = (float)width / (float)height;
	// sync aspect ratio to all cameras
	for (int i = 0; i < NUM_CAMERAS; i++)
		camera_info[i].aspect_ratio = current_camera.aspect_ratio;
	ProjectionMatrix = glm::perspective(current_camera.fovy, current_camera.aspect_ratio, current_camera.near_c, current_camera.far_c);
	ViewProjectionMatrix = ProjectionMatrix * ViewMatrix;
	glutPostRedisplay();
}

// ==================== cleanup ====================
void cleanup(void) {
	glDeleteVertexArrays(1, &axes_VAO); glDeleteBuffers(1, &axes_VBO);
	glDeleteVertexArrays(1, &grid_VAO); glDeleteBuffers(1, &grid_VBO);
	glDeleteVertexArrays(scene.n_materials, bistro_exterior_VAO);
	glDeleteBuffers(scene.n_materials, bistro_exterior_VBO);
	glDeleteTextures(scene.n_textures, bistro_exterior_texture_names);
	glDeleteVertexArrays(1, &skybox_VAO); glDeleteBuffers(1, &skybox_VBO);

	// my wireframe objects
	glDeleteVertexArrays(1, &my_tiger_VAO); glDeleteBuffers(1, &my_tiger_VBO);
	glDeleteVertexArrays(1, &my_wolf_VAO); glDeleteBuffers(1, &my_wolf_VBO);
	glDeleteVertexArrays(1, &my_spider_VAO); glDeleteBuffers(1, &my_spider_VBO);
	glDeleteVertexArrays(1, &my_cow_VAO); glDeleteBuffers(1, &my_cow_VBO);
	glDeleteVertexArrays(1, &my_ironman_VAO); glDeleteBuffers(1, &my_ironman_VBO);
	glDeleteVertexArrays(1, &my_godzilla_VAO); glDeleteBuffers(1, &my_godzilla_VBO);

	free(bistro_exterior_n_triangles);
	free(bistro_exterior_vertex_offset);
	free(bistro_exterior_VAO);
	free(bistro_exterior_VBO);
	free(bistro_exterior_texture_names);
	free(flag_texture_mapping);
}

// ==================== registration & init ====================
void register_callbacks(void) {
	glutDisplayFunc(display);
	glutKeyboardFunc(keyboard);
	glutSpecialFunc(special_key);
	glutReshapeFunc(reshape);
	glutMouseFunc(mouse_func);
	glutMotionFunc(motion_func);
	glutCloseFunc(cleanup);
	glutTimerFunc(10, timer_scene, 0);
}

void initialize_OpenGL(void) {
	glEnable(GL_DEPTH_TEST);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	ViewMatrix = glm::mat4(1.0f);
	ProjectionMatrix = glm::mat4(1.0f);
	ViewProjectionMatrix = ProjectionMatrix * ViewMatrix;
	initialize_lights();
}

void prepare_scene(void) {
	prepare_axes();
	prepare_grid();
	prepare_bistro_exterior();
	prepare_skybox();
	// my wireframe objects
	prepare_my_tiger();
	prepare_my_wolf();
	prepare_my_spider();
	prepare_my_cow();
	prepare_my_ironman();
	prepare_my_godzilla();
}

void initialize_renderer(void) {
	register_callbacks();
	prepare_shader_program();
	initialize_OpenGL();
	prepare_scene();
	initialize_camera();
}

void initialize_glew(void) {
	GLenum error;
	glewExperimental = GL_TRUE;
	error = glewInit();
	if (error != GLEW_OK) { fprintf(stderr, "Error: %s\n", glewGetErrorString(error)); exit(-1); }
	fprintf(stdout, "********************************************************************************\n");
	fprintf(stdout, " - GLEW version supported: %s\n", glewGetString(GLEW_VERSION));
	fprintf(stdout, " - OpenGL renderer: %s\n", glGetString(GL_RENDERER));
	fprintf(stdout, " - OpenGL version supported: %s\n", glGetString(GL_VERSION));
	fprintf(stdout, "********************************************************************************\n\n");
}

void greetings(char* program_name, char messages[][256], int n_message_lines) {
	fprintf(stdout, "********************************************************************************\n\n");
	fprintf(stdout, "  PROGRAM NAME: %s\n\n", program_name);
	fprintf(stdout, "    This program was coded for CSE4170/AIE4012 students\n");
	fprintf(stdout, "      of Dept. of Comp. Sci. & Eng., Sogang University.\n\n");
	for (int i = 0; i < n_message_lines; i++)
		fprintf(stdout, "%s\n", messages[i]);
	fprintf(stdout, "\n********************************************************************************\n\n");
	initialize_glew();
}

#define N_MESSAGE_LINES 14
void drawScene(int argc, char* argv[]) {
	char program_name[256] = "Sogang CSE4170 HW3 - BistroExterior";
	char messages[N_MESSAGE_LINES][256] = {
		"    - Keys used:",
		"\t\t'u/i/o/p' : world observation cameras",
		"\t\t'a' : free-moving camera",
		"\t\t't' : tiger eye camera",
		"\t\t'g' : tiger follow camera",
		"\t\t'c' : CCTV mode toggle",
		"\t\t'SPACE' : animation start/stop",
		"\t\t'f' : draw grid/axes",
		"\t\t'ESC' : program close",
		"\t\tCTRL+drag : pitch/yaw (obs cam)",
		"\t\tCTRL+scroll : zoom in/out",
		"\t\tarrow keys : translate (free cam)",
		"\t\tPageUp/Down : up/down (free cam)",
		"\t\tR-drag : roll (free cam)",
	};

	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
	glutInitWindowSize(900, 600);
	glutInitWindowPosition(20, 20);
	glutInitContextVersion(3, 3);
	glutInitContextProfile(GLUT_CORE_PROFILE);
	glutCreateWindow(program_name);

	greetings(program_name, messages, N_MESSAGE_LINES);
	initialize_renderer();

	glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);
	glutMainLoop();
}
