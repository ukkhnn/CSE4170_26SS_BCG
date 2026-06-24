#pragma warning(disable : 4996)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <GL/glew.h>
#include <GL/freeglut.h>
#include "Shaders/LoadShaders.h"

#define STB_IMAGE_IMPLEMENTATION
#include "Shaders/stb_image.h"

#define TO_RADIAN 0.01745329252f
#define TO_DEGREE 57.295779513f
#define BUFFER_OFFSET(offset) ((GLvoid *) (offset))

#define LIGHT_IN_EC 0
#define LIGHT_IN_WC 1
#define LIGHT_IN_MC 2

#define LOC_POSITION 0
#define LOC_NORMAL 1
#define LOC_TEXCOORD 2

#define FLOOR_SIZE 3000.0f
#define WALL_HEIGHT FLOOR_SIZE/3.0f
#define INITIAL_CAMERA_PRP FLOOR_SIZE/2.0f, FLOOR_SIZE/3.0f, FLOOR_SIZE/2.0f
#define INITIAL_CAMERA_VRP -0.0f, 0.0f, -0.0f
#define LIGHT_SPOT_CUTOFF_ANGLE 30.0f
#define LIGHT_SPOT_EXPONENT 10.0f
#define LIGHT_POS_IN_MC_MOVING_TIGER 0.0f, 0.0f, 15.0f
#define LIGHT_SPOT_DIRECTION_IN_TIGER_MC 0.0f, 0.0f, -1.0f
#define LIGHT_POS_IN_WC -150.0f, 700.0f, -300.0f
#define LIGHT_SPOT_DIRECTION_IN_WC 0.0f, -1.0f, 0.0f
#define LIGHT_POS_IN_EC -50.0f, 50.0f, -800.0f
#define LIGHT_SPOT_DIRECTION_IN_EC 0.0f, -0.3f, -1.0f
#define DIRECT_LIGHT_L_IN_MC 0.9f, 1.0f, 0.2f
#define DIRECT_LIGHT_L_IN_WC 0.9f, 0.9f, 0.9f
#define DIRECT_LIGHT_L_IN_EC 0.9f, 0.1f, 0.9f

#define ATTENUATION_KC 1.0f
#define ATTENUATION_KL 0.0f
#define ATTENUATION_KQ 0.00005f

#define WC_ORBIT_CENTER 0.0f, 700.0f, 0.0f
#define WC_ORBIT_RADIUS 800.0f
#define WC_ORBIT_SPEED 2.0f

#define TIGER_OPACITY_INIT 0.5f
#define TIGER_OPACITY_STEP 0.05f

#define FLOOR_TEX_SCALE 6.0f
#define WALL_TEX_SCALE 4.0f

#define FOG_COLOR 0.55f, 0.58f, 0.62f

GLuint h_ShaderProgram_simple, h_ShaderProgram_PS; // handles to shader programs

// for simple shaders
GLint loc_ModelViewProjectionMatrix_simple, loc_primitive_color_simple;

GLint loc_ModelViewProjectionMatrix_PS, loc_ModelViewMatrix_PS, loc_ModelViewMatrixInvTrans_PS;
GLint loc_Light_Position, loc_Light_La, loc_Light_L;
GLint loc_Light_Spot_direction, loc_Light_Spot_cutoff_angle, loc_Light_Spot_exponent;
GLint loc_Material_Ka, loc_Material_Kd, loc_Material_Ks, loc_Material_Shininess;
GLint loc_use_halfway_vector, loc_apply_spot_light;
GLint loc_apply_attenuation, loc_attenuation_coeff;
GLint loc_use_texture, loc_base_texture, loc_tex_scale, loc_u_alpha;
GLint loc_apply_toon, loc_apply_fog, loc_fog_color;

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp> // translate, rotate, scale, lookAt, perspective, etc.
#include <glm/gtc/matrix_inverse.hpp>   // inverseTranspose, etc.
glm::mat4 ModelViewProjectionMatrix, ModelViewMatrix;
glm::mat3 ModelViewMatrixInvTrans;
glm::mat4 ViewMatrix, ProjectionMatrix;

struct States {
	int cur_frame_tiger{ 0 };
	int cur_frame_spider{ 0 };
	int cur_frame_wolf{ 0 };
	int rotation_angle_tiger{ 0 };
	int animation_mode{ 1 };
	int use_halfway_vector{ 0 };
	int polygon_fill_mode{ 1 };
	int light_geometry_space{ LIGHT_IN_EC };
	int use_spot_light_for_EC_light{ 0 };
	int use_spot_light_for_WC_light{ 0 };
	int use_spot_light_for_circling_tiger_MC{ 0 };
	int apply_attenuation{ 0 };
	int move_wc_light{ 0 };
	int use_object_texture{ 0 };
	int use_floor_texture{ 0 };
	int use_blending{ 0 };
	int apply_toon{ 0 };
	int apply_fog{ 0 };
};
States states;

glm::vec3 cam_PRP;
float cam_yaw_deg;
float cam_pitch_deg;
int   g_left_button_down = 0;
int   g_last_mouse_x = 0, g_last_mouse_y = 0;

float wc_light_orbit_angle = 0.0f;

float tiger_opacity = TIGER_OPACITY_INIT;

GLuint tex_floor = 0, tex_wall = 0;

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
	glVertexAttribPointer(LOC_POSITION, 3, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0));
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}

void draw_axes(void) {
	glBindVertexArray(axes_VAO);
	glUniform3fv(loc_primitive_color_simple, 1, axes_color[0]);
	glDrawArrays(GL_LINES, 0, 2);
	glUniform3fv(loc_primitive_color_simple, 1, axes_color[1]);
	glDrawArrays(GL_LINES, 2, 2);
	glUniform3fv(loc_primitive_color_simple, 1, axes_color[2]);
	glDrawArrays(GL_LINES, 4, 2);
	glBindVertexArray(0);
}

GLuint rectangle_VBO, rectangle_VAO;
GLfloat rectangle_vertices[6][8] = {
	{ 0.0f, 0.0f, 0.0f,   0.0f, 0.0f, 1.0f,   0.0f, 0.0f },
	{ 1.0f, 0.0f, 0.0f,   0.0f, 0.0f, 1.0f,   1.0f, 0.0f },
	{ 1.0f, 1.0f, 0.0f,   0.0f, 0.0f, 1.0f,   1.0f, 1.0f },
	{ 0.0f, 0.0f, 0.0f,   0.0f, 0.0f, 1.0f,   0.0f, 0.0f },
	{ 1.0f, 1.0f, 0.0f,   0.0f, 0.0f, 1.0f,   1.0f, 1.0f },
	{ 0.0f, 1.0f, 0.0f,   0.0f, 0.0f, 1.0f,   0.0f, 1.0f }
};

void prepare_floor(void) {
	glGenBuffers(1, &rectangle_VBO);
	glBindBuffer(GL_ARRAY_BUFFER, rectangle_VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(rectangle_vertices), &rectangle_vertices[0][0], GL_STATIC_DRAW);

	glGenVertexArrays(1, &rectangle_VAO);
	glBindVertexArray(rectangle_VAO);
	glBindBuffer(GL_ARRAY_BUFFER, rectangle_VBO);
	glVertexAttribPointer(LOC_POSITION, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), BUFFER_OFFSET(0));
	glEnableVertexAttribArray(LOC_POSITION);
	glVertexAttribPointer(LOC_NORMAL, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), BUFFER_OFFSET(3 * sizeof(float)));
	glEnableVertexAttribArray(LOC_NORMAL);
	glVertexAttribPointer(LOC_TEXCOORD, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), BUFFER_OFFSET(6 * sizeof(float)));
	glEnableVertexAttribArray(LOC_TEXCOORD);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}

void set_material_floor(void) {
	glUniform3f(loc_Material_Ka, 0.19225f, 0.19225f, 0.19225f);
	glUniform3f(loc_Material_Kd, 0.50754f, 0.50754f, 0.50754f);
	glUniform3f(loc_Material_Ks, 0.508273f, 0.508273f, 0.508273f);
	glUniform1f(loc_Material_Shininess, 128.2f);
}

void draw_floor(void) {
	glFrontFace(GL_CCW);
	glBindVertexArray(rectangle_VAO);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);
}

int read_geometry(GLfloat **object, int bytes_per_primitive, char *filename) {
	int n_triangles;
	FILE *fp;

	fp = fopen(filename, "rb");
	if (fp == NULL) {
		fprintf(stderr, "Cannot open the object file %s ...\n", filename);
		return -1;
	}
	fread(&n_triangles, sizeof(int), 1, fp);
	*object = (float *)malloc(n_triangles * bytes_per_primitive);
	if (*object == NULL) {
		fprintf(stderr, "Cannot allocate memory for the geometry file %s ...\n", filename);
		fclose(fp);
		return -1;
	}
	fread(*object, bytes_per_primitive, n_triangles, fp);
	fclose(fp);
	return n_triangles;
}
// jpg/png -> GPU texture (by stb)
GLuint load_texture(const char *filename) {
	stbi_set_flip_vertically_on_load(true);
	int w, h, n_ch;
	unsigned char *data = stbi_load(filename, &w, &h, &n_ch, 0);
	if (data == NULL) {
		fprintf(stderr, "Cannot load texture image %s ...\n", filename);
		return 0;
	}
	GLenum format = (n_ch == 1) ? GL_RED : (n_ch == 3) ? GL_RGB : GL_RGBA;

	GLuint tex_id;
	glGenTextures(1, &tex_id);
	glBindTexture(GL_TEXTURE_2D, tex_id);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, format, w, h, 0, format, GL_UNSIGNED_BYTE, data);
	glGenerateMipmap(GL_TEXTURE_2D);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);

	stbi_image_free(data);
	fprintf(stdout, " - Loaded texture %-46s (%d x %d, %d ch)\n", filename, w, h, n_ch);
	return tex_id;
}

#define MAX_OBJ_FRAMES 20
typedef struct {
	GLuint VBO, VAO;
	int n_frames;
	int n_triangles[MAX_OBJ_FRAMES];
	int vertex_offset[MAX_OBJ_FRAMES];
	GLuint texture;
} Object;

#define N_TIGER_FRAMES 12
#define N_SPIDER_FRAMES 16
#define N_WOLF_FRAMES 17
Object obj_tiger, obj_cow, obj_ironman, obj_spider, obj_wolf;

void prepare_object_vnt(Object *obj, const char *filename_pattern, int n_frames, const char *texture_filename) {
	int i, n_bytes_per_vertex, n_bytes_per_triangle, n_total_triangles = 0;
	char filename[512];
	GLfloat *vertices[MAX_OBJ_FRAMES];

	n_bytes_per_vertex = 8 * sizeof(float);
	n_bytes_per_triangle = 3 * n_bytes_per_vertex;

	obj->n_frames = n_frames;
	for (i = 0; i < n_frames; i++) {
		if (n_frames == 1) strcpy(filename, filename_pattern);
		else               sprintf(filename, filename_pattern, i);
		obj->n_triangles[i] = read_geometry(&vertices[i], n_bytes_per_triangle, filename);
		n_total_triangles += obj->n_triangles[i];
		if (i == 0) obj->vertex_offset[i] = 0;
		else        obj->vertex_offset[i] = obj->vertex_offset[i - 1] + 3 * obj->n_triangles[i - 1];
	}

	glGenBuffers(1, &obj->VBO);
	glBindBuffer(GL_ARRAY_BUFFER, obj->VBO);
	glBufferData(GL_ARRAY_BUFFER, n_total_triangles * n_bytes_per_triangle, NULL, GL_STATIC_DRAW);
	for (i = 0; i < n_frames; i++)
		glBufferSubData(GL_ARRAY_BUFFER, obj->vertex_offset[i] * n_bytes_per_vertex,
			obj->n_triangles[i] * n_bytes_per_triangle, vertices[i]);
	for (i = 0; i < n_frames; i++)
		free(vertices[i]);

	glGenVertexArrays(1, &obj->VAO);
	glBindVertexArray(obj->VAO);
	glBindBuffer(GL_ARRAY_BUFFER, obj->VBO);
	glVertexAttribPointer(LOC_POSITION, 3, GL_FLOAT, GL_FALSE, n_bytes_per_vertex, BUFFER_OFFSET(0));
	glEnableVertexAttribArray(LOC_POSITION);
	glVertexAttribPointer(LOC_NORMAL, 3, GL_FLOAT, GL_FALSE, n_bytes_per_vertex, BUFFER_OFFSET(3 * sizeof(float)));
	glEnableVertexAttribArray(LOC_NORMAL);
	glVertexAttribPointer(LOC_TEXCOORD, 2, GL_FLOAT, GL_FALSE, n_bytes_per_vertex, BUFFER_OFFSET(6 * sizeof(float)));
	glEnableVertexAttribArray(LOC_TEXCOORD);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	obj->texture = (texture_filename != NULL) ? load_texture(texture_filename) : 0;
}

void draw_object(Object *obj, int frame) {
	glBindVertexArray(obj->VAO);
	glDrawArrays(GL_TRIANGLES, obj->vertex_offset[frame], 3 * obj->n_triangles[frame]);
	glBindVertexArray(0);
}

void set_material_tiger(int ambient, int diffuse, int specular) {
	if (ambient) glUniform3f(loc_Material_Ka, 0.4f, 0.3f, 0.1f);
	else         glUniform3f(loc_Material_Ka, 0.0f, 0.0f, 0.0f);
	if (diffuse) glUniform3f(loc_Material_Kd, 0.95164f, 0.60648f, 0.22648f);
	else         glUniform3f(loc_Material_Kd, 0.0f, 0.0f, 0.0f);
	if (specular) {
		glUniform3f(loc_Material_Ks, 0.628281f, 0.555802f, 0.555802f);
		glUniform1f(loc_Material_Shininess, 21.2f);
	}
	else {
		glUniform3f(loc_Material_Ks, 0.0f, 0.0f, 0.0f);
		glUniform1f(loc_Material_Shininess, 1.0f);
	}
}

void set_material_simple(float r, float g, float b) {
	glUniform3f(loc_Material_Ka, 0.2f * r, 0.2f * g, 0.2f * b);
	glUniform3f(loc_Material_Kd, r, g, b);
	glUniform3f(loc_Material_Ks, 0.3f, 0.3f, 0.3f);
	glUniform1f(loc_Material_Shininess, 20.0f);
}

void set_PS_matrices(const glm::mat4 &MV) {
	ModelViewMatrix = MV;
	ModelViewProjectionMatrix = ProjectionMatrix * MV;
	ModelViewMatrixInvTrans = glm::inverseTranspose(glm::mat3(MV));
	glUniformMatrix4fv(loc_ModelViewProjectionMatrix_PS, 1, GL_FALSE, &ModelViewProjectionMatrix[0][0]);
	glUniformMatrix4fv(loc_ModelViewMatrix_PS, 1, GL_FALSE, &ModelViewMatrix[0][0]);
	glUniformMatrix3fv(loc_ModelViewMatrixInvTrans_PS, 1, GL_FALSE, &ModelViewMatrixInvTrans[0][0]);
}

void set_texture_for_draw(int use_tex, GLuint tex_id, float scale_s, float scale_t, float alpha) {
	glUniform1i(loc_use_texture, use_tex);
	glUniform2f(loc_tex_scale, scale_s, scale_t);
	glUniform1f(loc_u_alpha, alpha);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, tex_id);
}

glm::mat4 placeMV(float wx, float wz, float scale, float native_minY, float yaw_deg) {
	glm::mat4 M = glm::translate(ViewMatrix, glm::vec3(wx, -native_minY * scale, wz));
	if (yaw_deg != 0.0f) M = glm::rotate(M, yaw_deg * TO_RADIAN, glm::vec3(0.0f, 1.0f, 0.0f));
	M = glm::scale(M, glm::vec3(scale, scale, scale));
	return M;
}

void update_ViewMatrix_from_camera(void) {
	float yaw = cam_yaw_deg * TO_RADIAN;
	float pitch = cam_pitch_deg * TO_RADIAN;
	glm::vec3 forward(cosf(pitch) * sinf(yaw), sinf(pitch), -cosf(pitch) * cosf(yaw));
	ViewMatrix = glm::lookAt(cam_PRP, cam_PRP + forward, glm::vec3(0.0f, 1.0f, 0.0f));
}

//display
void display(void) {
	if (states.apply_fog) glClearColor(FOG_COLOR, 1.0f);
	else                  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glm::mat4 tiger_MV = glm::rotate(ViewMatrix, -states.rotation_angle_tiger * TO_RADIAN,
		glm::vec3(0.0f, 1.0f, 0.0f));
	float jump_x = (states.rotation_angle_tiger % 180) / 180.0f;
	float jump_y = 4.0f * 100.0f * jump_x * (1.0f - jump_x);
	tiger_MV = glm::translate(tiger_MV, glm::vec3(500.0f, jump_y, 0.0f));
	tiger_MV = glm::rotate(tiger_MV, -90.0f * TO_RADIAN, glm::vec3(1.0f, 0.0f, 0.0f));

	glm::vec3 wc_light_pos = glm::vec3(WC_ORBIT_CENTER) + WC_ORBIT_RADIUS *
		glm::vec3(cosf(wc_light_orbit_angle * TO_RADIAN), 0.0f, sinf(wc_light_orbit_angle * TO_RADIAN));

	glUseProgram(h_ShaderProgram_PS);
	if (states.light_geometry_space == LIGHT_IN_EC) {
		glm::vec4 Lpos = glm::vec4(LIGHT_POS_IN_EC, 1.0f);
		glUniform4fv(loc_Light_Position, 1, &Lpos[0]);
		glm::vec3 Sdir = glm::normalize(glm::vec3(LIGHT_SPOT_DIRECTION_IN_EC));
		glUniform3fv(loc_Light_Spot_direction, 1, &Sdir[0]);
	}
	else if (states.light_geometry_space == LIGHT_IN_WC) {
		glm::vec4 Lpos = ViewMatrix * glm::vec4(wc_light_pos, 1.0f);
		glUniform4fv(loc_Light_Position, 1, &Lpos[0]);
		glm::vec3 Sdir = glm::mat3(ViewMatrix) * glm::vec3(LIGHT_SPOT_DIRECTION_IN_WC);
		glUniform3fv(loc_Light_Spot_direction, 1, &Sdir[0]);
	}
	else {
		glm::vec4 Lpos = tiger_MV * glm::vec4(LIGHT_POS_IN_MC_MOVING_TIGER, 1.0f);
		glUniform4fv(loc_Light_Position, 1, &Lpos[0]);
		glm::vec3 Sdir = glm::mat3(tiger_MV) * glm::vec3(LIGHT_SPOT_DIRECTION_IN_TIGER_MC);
		glUniform3fv(loc_Light_Spot_direction, 1, &Sdir[0]);
	}
	glUseProgram(0);

	glUseProgram(h_ShaderProgram_simple);
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	ModelViewMatrix = glm::scale(ViewMatrix, glm::vec3(100.0f, 100.0f, 100.0f));
	ModelViewProjectionMatrix = ProjectionMatrix * ModelViewMatrix;
	glUniformMatrix4fv(loc_ModelViewProjectionMatrix_simple, 1, GL_FALSE, &ModelViewProjectionMatrix[0][0]);
	glLineWidth(2.0f);
	draw_axes();
	glLineWidth(1.0f);
	glUseProgram(0);

	glUseProgram(h_ShaderProgram_PS);
	glUniform1i(loc_base_texture, 0);
	if (states.polygon_fill_mode) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	else                          glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	set_material_floor();
	set_texture_for_draw(states.use_floor_texture, tex_floor, FLOOR_TEX_SCALE, FLOOR_TEX_SCALE, 1.0f);
	{
		glm::mat4 MV = glm::translate(ViewMatrix, glm::vec3(-FLOOR_SIZE / 2.0f, 0.0f, FLOOR_SIZE / 2.0f));
		MV = glm::scale(MV, glm::vec3(FLOOR_SIZE, FLOOR_SIZE, FLOOR_SIZE));
		MV = glm::rotate(MV, -90.0f * TO_RADIAN, glm::vec3(1.0f, 0.0f, 0.0f));
		set_PS_matrices(MV);
		draw_floor();
	}
	set_texture_for_draw(states.use_floor_texture, tex_wall, WALL_TEX_SCALE, WALL_TEX_SCALE, 1.0f);
	{
		glm::mat4 MV = glm::translate(ViewMatrix, glm::vec3(-FLOOR_SIZE / 2.0f, 0.0f, -FLOOR_SIZE / 2.0f));
		MV = glm::scale(MV, glm::vec3(FLOOR_SIZE, WALL_HEIGHT, 1.0f));
		set_PS_matrices(MV);
		draw_floor();
	}
	{
		glm::mat4 MV = glm::translate(ViewMatrix, glm::vec3(-FLOOR_SIZE / 2.0f, 0.0f, FLOOR_SIZE / 2.0f));
		MV = glm::scale(MV, glm::vec3(1.0f, WALL_HEIGHT, FLOOR_SIZE));
		MV = glm::rotate(MV, 90.0f * TO_RADIAN, glm::vec3(0.0f, 1.0f, 0.0f));
		set_PS_matrices(MV);
		draw_floor();
	}

	glDisable(GL_CULL_FACE);

	set_material_simple(0.55f, 0.38f, 0.22f);
	set_texture_for_draw(states.use_object_texture, obj_cow.texture, 1.0f, 1.0f, 1.0f);
	{
		const float S = 13.0f, MINY = -0.09f;
		set_PS_matrices(placeMV(-1000.0f, -1000.0f, S, MINY,  30.0f)); draw_object(&obj_cow, 0);
		set_PS_matrices(placeMV( 1000.0f, -1000.0f, S, MINY, -30.0f)); draw_object(&obj_cow, 0);
		set_PS_matrices(placeMV(-1000.0f,  1000.0f, S, MINY, 150.0f)); draw_object(&obj_cow, 0);
	}
	set_material_simple(0.70f, 0.10f, 0.10f);
	set_texture_for_draw(states.use_object_texture, obj_ironman.texture, 1.0f, 1.0f, 1.0f);
	{
		const float S = 85.0f, MINY = -0.02f;
		set_PS_matrices(placeMV( 1000.0f, 1000.0f, S, MINY, 200.0f)); draw_object(&obj_ironman, 0);
		set_PS_matrices(placeMV( 1100.0f,    0.0f, S, MINY,  90.0f)); draw_object(&obj_ironman, 0);
		set_PS_matrices(placeMV(-1100.0f,    0.0f, S, MINY, -90.0f)); draw_object(&obj_ironman, 0);
	}
	set_material_simple(0.30f, 0.30f, 0.32f);
	set_texture_for_draw(states.use_object_texture, obj_spider.texture, 1.0f, 1.0f, 1.0f);
	set_PS_matrices(placeMV(0.0f, 1100.0f, 175.0f, -0.64f, 0.0f));
	draw_object(&obj_spider, states.cur_frame_spider);
	set_material_simple(0.60f, 0.60f, 0.60f);
	set_texture_for_draw(states.use_object_texture, obj_wolf.texture, 1.0f, 1.0f, 1.0f);
	set_PS_matrices(placeMV(0.0f, -1100.0f, 360.0f, 0.0f, 180.0f));
	draw_object(&obj_wolf, states.cur_frame_wolf);

	glEnable(GL_CULL_FACE);

	glFrontFace(GL_CW);
	set_material_tiger(1, 1, 1);
	set_texture_for_draw(states.use_object_texture, obj_tiger.texture, 1.0f, 1.0f,
		states.use_blending ? tiger_opacity : 1.0f);
	if (states.use_blending) {
		glEnable(GL_BLEND);
		glDepthMask(GL_FALSE);
	}
	set_PS_matrices(tiger_MV);
	draw_object(&obj_tiger, states.cur_frame_tiger);
	if (states.use_blending) {
		glDepthMask(GL_TRUE);
		glDisable(GL_BLEND);
	}
	glUseProgram(0);

	glUseProgram(h_ShaderProgram_simple);
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glLineWidth(2.0f);
	if (states.light_geometry_space == LIGHT_IN_EC) {
		ModelViewMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(LIGHT_POS_IN_EC));
		ModelViewMatrix = glm::scale(ModelViewMatrix, glm::vec3(10.0f, 10.0f, 10.0f));
	}
	else if (states.light_geometry_space == LIGHT_IN_WC) {
		ModelViewMatrix = glm::translate(ViewMatrix, wc_light_pos);
		ModelViewMatrix = glm::scale(ModelViewMatrix, glm::vec3(30.0f, 30.0f, 30.0f));
	}
	else {
		ModelViewMatrix = glm::translate(tiger_MV, glm::vec3(LIGHT_POS_IN_MC_MOVING_TIGER));
		ModelViewMatrix = glm::scale(ModelViewMatrix, glm::vec3(10.0f, 10.0f, 10.0f));
	}
	ModelViewProjectionMatrix = ProjectionMatrix * ModelViewMatrix;
	glUniformMatrix4fv(loc_ModelViewProjectionMatrix_simple, 1, GL_FALSE, &ModelViewProjectionMatrix[0][0]);
	draw_axes();
	glLineWidth(1.0f);
	glUseProgram(0);

	glutSwapBuffers();
}

void timer_scene(int value) {
	static unsigned int _timestamp_scene = 0;
	if (states.animation_mode) {
		states.cur_frame_tiger  = _timestamp_scene % N_TIGER_FRAMES;
		states.cur_frame_spider = _timestamp_scene % N_SPIDER_FRAMES;
		states.cur_frame_wolf   = _timestamp_scene % N_WOLF_FRAMES;
		states.rotation_angle_tiger = _timestamp_scene % 360;
		_timestamp_scene++;
	}
	if (states.move_wc_light) {
		wc_light_orbit_angle += WC_ORBIT_SPEED;
		if (wc_light_orbit_angle >= 360.0f) wc_light_orbit_angle -= 360.0f;
	}
	glutPostRedisplay();
	glutTimerFunc(100, timer_scene, 0);
}

void mouse(int button, int state, int x, int y) {
	if (button == GLUT_LEFT_BUTTON) {
		if (state == GLUT_DOWN) { g_left_button_down = 1; g_last_mouse_x = x; g_last_mouse_y = y; }
		else g_left_button_down = 0;
	}
}

void motion(int x, int y) {
	if (!g_left_button_down) return;
	int dx = x - g_last_mouse_x;
	int dy = y - g_last_mouse_y;
	g_last_mouse_x = x; g_last_mouse_y = y;

	const float sensitivity = 0.2f;
	cam_yaw_deg   += dx * sensitivity;
	cam_pitch_deg -= dy * sensitivity;
	if (cam_pitch_deg >  89.0f) cam_pitch_deg =  89.0f;
	if (cam_pitch_deg < -89.0f) cam_pitch_deg = -89.0f;

	update_ViewMatrix_from_camera();
	glutPostRedisplay();
}

void keyboard(unsigned char key, int x, int y) {
	switch (key) {
	case 27:
		glutLeaveMainLoop();
		break;
	case 'a':
		states.animation_mode = 1 - states.animation_mode;
		break;
	case 'p':
		states.polygon_fill_mode = 1 - states.polygon_fill_mode;
		if (states.polygon_fill_mode) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		else                          glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		glutPostRedisplay();
		break;
	case 'v':
		states.use_halfway_vector = 1 - states.use_halfway_vector;
		glUseProgram(h_ShaderProgram_PS);
		glUniform1i(loc_use_halfway_vector, states.use_halfway_vector);
		glUseProgram(0);
		fprintf(stdout, states.use_halfway_vector ?
			"^^^ Using the halfway vector for specular reflection...\n" :
			"^^^ Using the reflection vector for specular reflection...\n");
		glutPostRedisplay();
		break;
	case 'l':
		states.light_geometry_space = (states.light_geometry_space + 1) % 3;
		states.apply_attenuation = 0;
		states.move_wc_light = 0;
		glUseProgram(h_ShaderProgram_PS);
		glUniform1i(loc_apply_attenuation, states.apply_attenuation);
		if (states.light_geometry_space == LIGHT_IN_EC) {
			states.use_spot_light_for_EC_light = 0;
			glUniform3f(loc_Light_L, DIRECT_LIGHT_L_IN_EC);
			glUniform1i(loc_apply_spot_light, states.use_spot_light_for_EC_light);
			fprintf(stdout, "^^^ The light is defined in EC.\n");
		}
		else if (states.light_geometry_space == LIGHT_IN_WC) {
			states.use_spot_light_for_WC_light = 0;
			glUniform3f(loc_Light_L, DIRECT_LIGHT_L_IN_WC);
			glUniform1i(loc_apply_spot_light, states.use_spot_light_for_WC_light);
			fprintf(stdout, "^^^ The light is defined in WC.\n");
		}
		else {
			states.use_spot_light_for_circling_tiger_MC = 0;
			glUniform3f(loc_Light_L, DIRECT_LIGHT_L_IN_MC);
			glUniform1i(loc_apply_spot_light, states.use_spot_light_for_circling_tiger_MC);
			fprintf(stdout, "^^^ The light is defined in MC.\n");
		}
		glUseProgram(0);
		glutPostRedisplay();
		break;
	case 'm':
		if (states.light_geometry_space != LIGHT_IN_MC) break;
		states.use_spot_light_for_circling_tiger_MC = 1 - states.use_spot_light_for_circling_tiger_MC;
		glUseProgram(h_ShaderProgram_PS);
		glUniform1i(loc_apply_spot_light, states.use_spot_light_for_circling_tiger_MC);
		glUseProgram(0);
		fprintf(stdout, states.use_spot_light_for_circling_tiger_MC ?
			"^^^ Spot light ON for the circling-tiger (MC) light...\n" :
			"^^^ Spot light OFF for the circling-tiger (MC) light...\n");
		glutPostRedisplay();
		break;
	case 'w':
		if (states.light_geometry_space != LIGHT_IN_WC) break;
		states.use_spot_light_for_WC_light = 1 - states.use_spot_light_for_WC_light;
		glUseProgram(h_ShaderProgram_PS);
		glUniform1i(loc_apply_spot_light, states.use_spot_light_for_WC_light);
		glUseProgram(0);
		fprintf(stdout, states.use_spot_light_for_WC_light ?
			"^^^ Spot light ON for the WC light...\n" :
			"^^^ Spot light OFF for the WC light...\n");
		glutPostRedisplay();
		break;
	case 'e':
		if (states.light_geometry_space != LIGHT_IN_EC) break;
		states.use_spot_light_for_EC_light = 1 - states.use_spot_light_for_EC_light;
		glUseProgram(h_ShaderProgram_PS);
		glUniform1i(loc_apply_spot_light, states.use_spot_light_for_EC_light);
		glUseProgram(0);
		fprintf(stdout, states.use_spot_light_for_EC_light ?
			"^^^ Spot light ON for the EC light...\n" :
			"^^^ Spot light OFF for the EC light...\n");
		glutPostRedisplay();
		break;
	case 'b':
		if (states.light_geometry_space != LIGHT_IN_MC) break;
		states.apply_attenuation = 1 - states.apply_attenuation;
		glUseProgram(h_ShaderProgram_PS);
		glUniform1i(loc_apply_attenuation, states.apply_attenuation);
		glUseProgram(0);
		fprintf(stdout, states.apply_attenuation ?
			"^^^ Distance attenuation ON for the MC light...\n" :
			"^^^ Distance attenuation OFF...\n");
		glutPostRedisplay();
		break;
	case 'd':
		if (states.light_geometry_space != LIGHT_IN_WC) break;
		states.move_wc_light = 1 - states.move_wc_light;
		fprintf(stdout, states.move_wc_light ?
			"^^^ WC light circular orbit ON...\n" :
			"^^^ WC light circular orbit OFF...\n");
		glutPostRedisplay();
		break;
	case 'f':
		states.use_object_texture = 1 - states.use_object_texture;
		fprintf(stdout, states.use_object_texture ?
			"^^^ Object textures ON...\n" : "^^^ Object textures OFF...\n");
		glutPostRedisplay();
		break;
	case 'g':
		states.use_floor_texture = 1 - states.use_floor_texture;
		fprintf(stdout, states.use_floor_texture ?
			"^^^ Floor/wall textures ON...\n" : "^^^ Floor/wall textures OFF...\n");
		glutPostRedisplay();
		break;
	case 'h':
		states.use_blending = 1 - states.use_blending;
		if (states.use_blending)
			fprintf(stdout, "^^^ Tiger blending ON (opacity %.2f). Use 'i'/'j' to adjust.\n", tiger_opacity);
		else
			fprintf(stdout, "^^^ Tiger blending OFF...\n");
		glutPostRedisplay();
		break;
	case 'i':
		tiger_opacity += TIGER_OPACITY_STEP;
		if (tiger_opacity > 1.0f) tiger_opacity = 1.0f;
		fprintf(stdout, "^^^ Tiger opacity = %.2f\n", tiger_opacity);
		glutPostRedisplay();
		break;
	case 'j':
		tiger_opacity -= TIGER_OPACITY_STEP;
		if (tiger_opacity < 0.0f) tiger_opacity = 0.0f;
		fprintf(stdout, "^^^ Tiger opacity = %.2f\n", tiger_opacity);
		glutPostRedisplay();
		break;
	case 'c':
		states.apply_toon = 1 - states.apply_toon;
		glUseProgram(h_ShaderProgram_PS);
		glUniform1i(loc_apply_toon, states.apply_toon);
		glUseProgram(0);
		fprintf(stdout, states.apply_toon ?
			"^^^ Toon (cel) shading ON...\n" : "^^^ Toon (cel) shading OFF...\n");
		glutPostRedisplay();
		break;
	case 'n':
		states.apply_fog = 1 - states.apply_fog;
		glUseProgram(h_ShaderProgram_PS);
		glUniform1i(loc_apply_fog, states.apply_fog);
		glUseProgram(0);
		fprintf(stdout, states.apply_fog ?
			"^^^ Distance fog ON...\n" : "^^^ Distance fog OFF...\n");
		glutPostRedisplay();
		break;
	default:
		break;
	}
}

#define INITIAL_FOVY_RAD		35.0f * TO_RADIAN
#define MINIMUM_FOVY_RAD		5.0f * TO_RADIAN
#define MAXIMUM_FOVY_RAD		60.0f * TO_RADIAN
#define ZOOM_MULTIPLIER			1.02f
float zoom_factor;
float aspect_ratio;
void wheel(int wheel, int direction, int x, int y) {
	if (direction == -1) {
		if (INITIAL_FOVY_RAD * zoom_factor * ZOOM_MULTIPLIER <= MAXIMUM_FOVY_RAD) {
			zoom_factor *= ZOOM_MULTIPLIER;
			ProjectionMatrix = glm::perspective(zoom_factor * INITIAL_FOVY_RAD, aspect_ratio, 10.0f, 5000.0f);
			glutPostRedisplay();
		}
	}
	else {
		if (INITIAL_FOVY_RAD * zoom_factor / ZOOM_MULTIPLIER >= MINIMUM_FOVY_RAD) {
			zoom_factor /= ZOOM_MULTIPLIER;
			ProjectionMatrix = glm::perspective(zoom_factor * INITIAL_FOVY_RAD, aspect_ratio, 10.0f, 5000.0f);
			glutPostRedisplay();
		}
	}
}

void reshape(int width, int height) {
	glViewport(0, 0, width, height);
	aspect_ratio = (float)width / height;
	ProjectionMatrix = glm::perspective(zoom_factor * INITIAL_FOVY_RAD, aspect_ratio, 10.0f, 5000.0f);
	glutPostRedisplay();
}

void cleanup(void) {
	glDeleteVertexArrays(1, &axes_VAO); glDeleteBuffers(1, &axes_VBO);
	glDeleteVertexArrays(1, &rectangle_VAO); glDeleteBuffers(1, &rectangle_VBO);
	glDeleteVertexArrays(1, &obj_tiger.VAO); glDeleteBuffers(1, &obj_tiger.VBO);
	glDeleteVertexArrays(1, &obj_cow.VAO); glDeleteBuffers(1, &obj_cow.VBO);
	glDeleteVertexArrays(1, &obj_ironman.VAO); glDeleteBuffers(1, &obj_ironman.VBO);
	glDeleteVertexArrays(1, &obj_spider.VAO); glDeleteBuffers(1, &obj_spider.VBO);
	glDeleteVertexArrays(1, &obj_wolf.VAO); glDeleteBuffers(1, &obj_wolf.VBO);
}

void register_callbacks(void) {
	glutDisplayFunc(display);
	glutKeyboardFunc(keyboard);
	glutMouseFunc(mouse);
	glutMotionFunc(motion);
	glutMouseWheelFunc(wheel);
	glutReshapeFunc(reshape);
	glutTimerFunc(100, timer_scene, 0);
	glutCloseFunc(cleanup);
}

void prepare_shader_program(void) {
	ShaderInfo shader_info_simple[3] = {
		{ GL_VERTEX_SHADER, "Shaders/simple.vert" },
		{ GL_FRAGMENT_SHADER, "Shaders/simple.frag" },
		{ GL_NONE, NULL }
	};
	ShaderInfo shader_info_PS[3] = {
		{ GL_VERTEX_SHADER, "Shaders/Simple_Phong.vert" },
		{ GL_FRAGMENT_SHADER, "Shaders/Simple_Phong.frag" },
		{ GL_NONE, NULL }
	};

	h_ShaderProgram_simple = LoadShaders(shader_info_simple);
	loc_primitive_color_simple = glGetUniformLocation(h_ShaderProgram_simple, "u_primitive_color");
	loc_ModelViewProjectionMatrix_simple = glGetUniformLocation(h_ShaderProgram_simple,
		"u_ModelViewProjectionMatrix");

	h_ShaderProgram_PS = LoadShaders(shader_info_PS);
	loc_ModelViewProjectionMatrix_PS = glGetUniformLocation(h_ShaderProgram_PS, "ModelViewProjectionMatrix");
	loc_ModelViewMatrix_PS = glGetUniformLocation(h_ShaderProgram_PS, "ModelViewMatrix");
	loc_ModelViewMatrixInvTrans_PS = glGetUniformLocation(h_ShaderProgram_PS, "ModelViewMatrixInvTrans");

	loc_Light_Position = glGetUniformLocation(h_ShaderProgram_PS, "Light.Position");
	loc_Light_La = glGetUniformLocation(h_ShaderProgram_PS, "Light.La");
	loc_Light_L = glGetUniformLocation(h_ShaderProgram_PS, "Light.L");
	loc_Light_Spot_direction = glGetUniformLocation(h_ShaderProgram_PS, "Light.Spot_direction");
	loc_Light_Spot_cutoff_angle = glGetUniformLocation(h_ShaderProgram_PS, "Light.Spot_cutoff_angle");
	loc_Light_Spot_exponent = glGetUniformLocation(h_ShaderProgram_PS, "Light.Spot_exponent");

	loc_Material_Ka = glGetUniformLocation(h_ShaderProgram_PS, "Material.Ka");
	loc_Material_Kd = glGetUniformLocation(h_ShaderProgram_PS, "Material.Kd");
	loc_Material_Ks = glGetUniformLocation(h_ShaderProgram_PS, "Material.Ks");
	loc_Material_Shininess = glGetUniformLocation(h_ShaderProgram_PS, "Material.Shininess");

	loc_use_halfway_vector = glGetUniformLocation(h_ShaderProgram_PS, "use_halfway_vector");
	loc_apply_spot_light = glGetUniformLocation(h_ShaderProgram_PS, "apply_spot_light");

	loc_apply_attenuation = glGetUniformLocation(h_ShaderProgram_PS, "apply_attenuation");
	loc_attenuation_coeff = glGetUniformLocation(h_ShaderProgram_PS, "attenuation_coeff");
	loc_use_texture = glGetUniformLocation(h_ShaderProgram_PS, "u_use_texture");
	loc_base_texture = glGetUniformLocation(h_ShaderProgram_PS, "u_base_texture");
	loc_tex_scale = glGetUniformLocation(h_ShaderProgram_PS, "u_tex_scale");
	loc_u_alpha = glGetUniformLocation(h_ShaderProgram_PS, "u_alpha");

	loc_apply_toon = glGetUniformLocation(h_ShaderProgram_PS, "apply_toon");
	loc_apply_fog = glGetUniformLocation(h_ShaderProgram_PS, "apply_fog");
	loc_fog_color = glGetUniformLocation(h_ShaderProgram_PS, "fog_color");
}

void initialize_OpenGL(void) {
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_MULTISAMPLE);

	glCullFace(GL_BACK);
	glEnable(GL_CULL_FACE);

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	if (states.polygon_fill_mode) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	else                          glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	glUseProgram(h_ShaderProgram_PS);
	glUniform3f(loc_Light_La, 0.5f, 0.5f, 0.5f);
	glUniform1f(loc_Light_Spot_cutoff_angle, LIGHT_SPOT_CUTOFF_ANGLE);
	glUniform1f(loc_Light_Spot_exponent, LIGHT_SPOT_EXPONENT);
	glUniform1i(loc_apply_attenuation, 0);
	glUniform3f(loc_attenuation_coeff, ATTENUATION_KC, ATTENUATION_KL, ATTENUATION_KQ);
	glUniform1i(loc_use_texture, 0);
	glUniform1i(loc_base_texture, 0);
	glUniform2f(loc_tex_scale, 1.0f, 1.0f);
	glUniform1f(loc_u_alpha, 1.0f);
	glUniform1i(loc_apply_toon, 0);
	glUniform1i(loc_apply_fog, 0);
	glUniform3f(loc_fog_color, FOG_COLOR);
	fprintf(stdout, "^^^ Using the reflection vector for specular reflection...\n");
	glUseProgram(0);

	states.light_geometry_space = LIGHT_IN_EC;
	states.use_spot_light_for_EC_light = 0;
	glUseProgram(h_ShaderProgram_PS);
	glUniform3f(loc_Light_L, DIRECT_LIGHT_L_IN_EC);
	glUniform1i(loc_apply_spot_light, states.use_spot_light_for_EC_light);
	glUseProgram(0);
	fprintf(stdout, "^^^ The light is defined in EC.\n");

	cam_PRP = glm::vec3(INITIAL_CAMERA_PRP);
	glm::vec3 vrp = glm::vec3(INITIAL_CAMERA_VRP);
	glm::vec3 forward0 = glm::normalize(vrp - cam_PRP);
	cam_pitch_deg = asinf(forward0.y) * TO_DEGREE;
	cam_yaw_deg = atan2f(forward0.x, -forward0.z) * TO_DEGREE;
	update_ViewMatrix_from_camera();

	zoom_factor = 1.0f;
}

void prepare_scene(void) {
	prepare_axes();
	prepare_floor();

	prepare_object_vnt(&obj_tiger, "Data/dynamic_objects/tiger/Tiger_%02d_triangles_vnt.geom",
		N_TIGER_FRAMES, "Data/dynamic_objects/tiger/tiger_tex.jpg");
	prepare_object_vnt(&obj_cow, "Data/static_objects/cow_vnt.geom", 1,
		"Data/static_objects/Cow.png");
	prepare_object_vnt(&obj_ironman, "Data/static_objects/ironman_vnt.geom", 1,
		"Data/static_objects/Ironman_Body.png");
	prepare_object_vnt(&obj_spider, "Data/dynamic_objects/spider/spider_vnt_%02d.geom",
		N_SPIDER_FRAMES, "Data/dynamic_objects/spider/Spinnen_Bein_tex.jpg");
	prepare_object_vnt(&obj_wolf, "Data/dynamic_objects/wolf/wolf_%02d_vnt.geom",
		N_WOLF_FRAMES, "Data/dynamic_objects/wolf/Wolf_Body.jpg");

	tex_floor = load_texture("Data/static_objects/grass_tex.jpg");
	tex_wall  = load_texture("Data/static_objects/checker_tex.jpg");
}

void initialize_renderer(void) {
	register_callbacks();
	prepare_shader_program();
	initialize_OpenGL();
	prepare_scene();
}

void initialize_glew(void) {
	GLenum error = glewInit();
	if (error != GLEW_OK) {
		fprintf(stderr, "Error: %s\n", glewGetErrorString(error));
		exit(-1);
	}
	fprintf(stdout, "*********************************************************\n");
	fprintf(stdout, " - GLEW version supported: %s\n", glewGetString(GLEW_VERSION));
	fprintf(stdout, " - OpenGL renderer: %s\n", glGetString(GL_RENDERER));
	fprintf(stdout, " - OpenGL version supported: %s\n", glGetString(GL_VERSION));
	fprintf(stdout, "*********************************************************\n\n");
}

void greetings(char *program_name, char messages[][256], int n_message_lines) {
	fprintf(stdout, "**************************************************************\n\n");
	fprintf(stdout, "  PROGRAM NAME: %s\n\n", program_name);
	fprintf(stdout, "    This program was coded for CSE4170 students\n");
	fprintf(stdout, "      of Dept. of Comp. Sci. & Eng., Sogang University.\n\n");

	for (int i = 0; i < n_message_lines; i++)
		fprintf(stdout, "%s\n", messages[i]);
	fprintf(stdout, "\n**************************************************************\n\n");

	initialize_glew();
}

#define N_MESSAGE_LINES 5
int main(int argc, char *argv[]) {
	char program_name[128] = "Sogang CSE4170/AIE4012 HW5 Tiger Shading/Texture/Transparency";
	char messages[N_MESSAGE_LINES][256] = {
		"    - Keys: a(anim) p(fill) l(light: EC/WC/MC) v(halfway)",
		"    - EC: e(spot) | WC: w(spot) d(orbit) | MC: m(spot) b(attenuation)",
		"    - f(object tex) g(floor tex) h(tiger blend) i/j(opacity +/-)",
		"    - P11: c(toon) n(fog) | ESC to quit",
		"    - Mouse: LEFT-drag = rotate gaze, wheel = zoom"
	};

	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH | GLUT_MULTISAMPLE);
	glutInitWindowSize(1280, 1024);
	glutInitContextVersion(3, 2);
	glutInitContextProfile(GLUT_CORE_PROFILE);
	glutCreateWindow(program_name);

	greetings(program_name, messages, N_MESSAGE_LINES);
	initialize_renderer();

	glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);
	glutMainLoop();
	return 1;
}
