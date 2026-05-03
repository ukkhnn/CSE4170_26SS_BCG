#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <GL/glew.h>
#include <GL/freeglut.h>

#include "Shaders/LoadShaders.h"
GLuint h_ShaderProgram; // handle to shader program
GLint loc_ModelViewProjectionMatrix, loc_primitive_color; // indices of uniform variables

#include <glm/gtc/matrix_transform.hpp> //translate, rotate, scale, ortho, etc.
glm::mat4 ModelViewProjectionMatrix;
glm::mat4 ViewMatrix, ProjectionMatrix, ViewProjectionMatrix;

#define TO_RADIAN 0.01745329252f  
#define TO_DEGREE 57.295779513f
#define BUFFER_OFFSET(offset) ((GLvoid *) (offset))

#define LOC_VERTEX 0

int win_width = 0, win_height = 0;

// HW2: 애니메이션 타이머
// timestamp  : 항상 증가 (L2/L3 orbit 공용)
// clock_L1   : '1'키로 on/off  (요구 3 - Level 1)
// clock_L4   : '4'키로 on/off  (요구 4 - Level 4, 추가된 레벨)
// clock_L5   : '5'키로 on/off  (Level 5, 추가 구현)
int  timestamp = 0;
int  clock_L1 = 0;
int  clock_L4 = 0;
int  clock_L5 = 0;
int  anim_L1_on = 1;
int  anim_L4_on = 1;
int  anim_L5_on = 1;

// HW2: 마우스 드래그로 Level0 위치 이동
float L0_x = 0.0f, L0_y = 0.0f;
int   is_dragging = 0;


// ================================================================
//  axes
// ================================================================
GLfloat axes[4][2];
GLfloat axes_color[3] = { 0.0f, 0.0f, 0.0f };
GLuint VBO_axes, VAO_axes;

void prepare_axes(void) { // Draw axes in their MC.
	axes[0][0] = -win_width / 2.5f; axes[0][1] = 0.0f;
	axes[1][0] = win_width / 2.5f; axes[1][1] = 0.0f;
	axes[2][0] = 0.0f; axes[2][1] = -win_height / 2.5f;
	axes[3][0] = 0.0f; axes[3][1] = win_height / 2.5f;

	glGenBuffers(1, &VBO_axes);
	glBindBuffer(GL_ARRAY_BUFFER, VBO_axes);
	glBufferData(GL_ARRAY_BUFFER, sizeof(axes), axes, GL_STATIC_DRAW);

	glGenVertexArrays(1, &VAO_axes);
	glBindVertexArray(VAO_axes);
	glBindBuffer(GL_ARRAY_BUFFER, VBO_axes);
	glVertexAttribPointer(LOC_VERTEX, 2, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0));
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}

void update_axes(void) {
	axes[0][0] = -win_width / 2.25f; axes[1][0] = win_width / 2.25f;
	axes[2][1] = -win_height / 2.25f;
	axes[3][1] = win_height / 2.25f;

	glBindBuffer(GL_ARRAY_BUFFER, VBO_axes);
	glBufferData(GL_ARRAY_BUFFER, sizeof(axes), axes, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void draw_axes(void) {
	glUniform3fv(loc_primitive_color, 1, axes_color);
	glBindVertexArray(VAO_axes);
	glDrawArrays(GL_LINES, 0, 4);
	glBindVertexArray(0);
}


// ================================================================
//  airplane  (Level 0)
// ================================================================
#define AIRPLANE_BIG_WING 0
#define AIRPLANE_SMALL_WING 1
#define AIRPLANE_BODY 2
#define AIRPLANE_BACK 3
#define AIRPLANE_SIDEWINDER1 4
#define AIRPLANE_SIDEWINDER2 5
#define AIRPLANE_CENTER 6
GLfloat big_wing[6][2] = { { 0.0, 0.0 },{ -20.0, 15.0 },{ -20.0, 20.0 },{ 0.0, 23.0 },{ 20.0, 20.0 },{ 20.0, 15.0 } };
GLfloat small_wing[6][2] = { { 0.0, -18.0 },{ -11.0, -12.0 },{ -12.0, -7.0 },{ 0.0, -10.0 },{ 12.0, -7.0 },{ 11.0, -12.0 } };
GLfloat body[5][2] = { { 0.0, -25.0 },{ -6.0, 0.0 },{ -6.0, 22.0 },{ 6.0, 22.0 },{ 6.0, 0.0 } };
GLfloat back[5][2] = { { 0.0, 25.0 },{ -7.0, 24.0 },{ -7.0, 21.0 },{ 7.0, 21.0 },{ 7.0, 24.0 } };
GLfloat sidewinder1[5][2] = { { -20.0, 10.0 },{ -18.0, 3.0 },{ -16.0, 10.0 },{ -18.0, 20.0 },{ -20.0, 20.0 } };
GLfloat sidewinder2[5][2] = { { 20.0, 10.0 },{ 18.0, 3.0 },{ 16.0, 10.0 },{ 18.0, 20.0 },{ 20.0, 20.0 } };
GLfloat center[1][2] = { { 0.0, 0.0 } };
GLfloat airplane_color[7][3] = {
	{ 150 / 255.0f, 129 / 255.0f, 183 / 255.0f },
	{ 245 / 255.0f, 211 / 255.0f,   0 / 255.0f },
	{ 111 / 255.0f,  85 / 255.0f, 157 / 255.0f },
	{ 150 / 255.0f, 129 / 255.0f, 183 / 255.0f },
	{ 245 / 255.0f, 211 / 255.0f,   0 / 255.0f },
	{ 245 / 255.0f, 211 / 255.0f,   0 / 255.0f },
	{ 255 / 255.0f,   0 / 255.0f,   0 / 255.0f }
};
GLuint VBO_airplane, VAO_airplane;

void prepare_airplane() {
	GLsizeiptr buffer_size = sizeof(big_wing) + sizeof(small_wing) + sizeof(body) + sizeof(back)
		+ sizeof(sidewinder1) + sizeof(sidewinder2) + sizeof(center);

	glGenBuffers(1, &VBO_airplane);
	glBindBuffer(GL_ARRAY_BUFFER, VBO_airplane);
	glBufferData(GL_ARRAY_BUFFER, buffer_size, NULL, GL_STATIC_DRAW);

	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(big_wing), big_wing);
	glBufferSubData(GL_ARRAY_BUFFER, sizeof(big_wing), sizeof(small_wing), small_wing);
	glBufferSubData(GL_ARRAY_BUFFER, sizeof(big_wing) + sizeof(small_wing), sizeof(body), body);
	glBufferSubData(GL_ARRAY_BUFFER, sizeof(big_wing) + sizeof(small_wing) + sizeof(body), sizeof(back), back);
	glBufferSubData(GL_ARRAY_BUFFER, sizeof(big_wing) + sizeof(small_wing) + sizeof(body) + sizeof(back),
		sizeof(sidewinder1), sidewinder1);
	glBufferSubData(GL_ARRAY_BUFFER, sizeof(big_wing) + sizeof(small_wing) + sizeof(body) + sizeof(back)
		+ sizeof(sidewinder1), sizeof(sidewinder2), sidewinder2);
	glBufferSubData(GL_ARRAY_BUFFER, sizeof(big_wing) + sizeof(small_wing) + sizeof(body) + sizeof(back)
		+ sizeof(sidewinder1) + sizeof(sidewinder2), sizeof(center), center);

	glGenVertexArrays(1, &VAO_airplane);
	glBindVertexArray(VAO_airplane);
	glBindBuffer(GL_ARRAY_BUFFER, VBO_airplane);
	glVertexAttribPointer(LOC_VERTEX, 2, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0));
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}

void draw_airplane() {
	glBindVertexArray(VAO_airplane);
	glUniform3fv(loc_primitive_color, 1, airplane_color[AIRPLANE_BIG_WING]);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 6);
	glUniform3fv(loc_primitive_color, 1, airplane_color[AIRPLANE_SMALL_WING]);
	glDrawArrays(GL_TRIANGLE_FAN, 6, 6);
	glUniform3fv(loc_primitive_color, 1, airplane_color[AIRPLANE_BODY]);
	glDrawArrays(GL_TRIANGLE_FAN, 12, 5);
	glUniform3fv(loc_primitive_color, 1, airplane_color[AIRPLANE_BACK]);
	glDrawArrays(GL_TRIANGLE_FAN, 17, 5);
	glUniform3fv(loc_primitive_color, 1, airplane_color[AIRPLANE_SIDEWINDER1]);
	glDrawArrays(GL_TRIANGLE_FAN, 22, 5);
	glUniform3fv(loc_primitive_color, 1, airplane_color[AIRPLANE_SIDEWINDER2]);
	glDrawArrays(GL_TRIANGLE_FAN, 27, 5);
	glUniform3fv(loc_primitive_color, 1, airplane_color[AIRPLANE_CENTER]);
	glPointSize(5.0);
	glDrawArrays(GL_POINTS, 32, 1);
	glPointSize(1.0);
	glBindVertexArray(0);
}


// ================================================================
//  hat  (Level 1)
// ================================================================
#define HAT_LEAF 0
#define HAT_BODY 1
#define HAT_STRIP 2
#define HAT_BOTTOM 3

GLfloat hat_leaf[4][2] = { { 3.0, 20.0 },{ 3.0, 28.0 },{ 9.0, 32.0 },{ 9.0, 24.0 } };
GLfloat hat_body[4][2] = { { -19.5, 2.0 },{ 19.5, 2.0 },{ 15.0, 20.0 },{ -15.0, 20.0 } };
GLfloat hat_strip[4][2] = { { -20.0, 0.0 },{ 20.0, 0.0 },{ 19.5, 2.0 },{ -19.5, 2.0 } };
GLfloat hat_bottom[4][2] = { { 25.0, 0.0 },{ -25.0, 0.0 },{ -25.0, -4.0 },{ 25.0, -4.0 } };

GLfloat hat_color[4][3] = {
	{ 167 / 255.0f, 255 / 255.0f,  55 / 255.0f },
	{ 255 / 255.0f, 144 / 255.0f,  32 / 255.0f },
	{ 255 / 255.0f,  40 / 255.0f,  33 / 255.0f },
	{ 255 / 255.0f, 144 / 255.0f,  32 / 255.0f }
};

GLuint VBO_hat, VAO_hat;

void prepare_hat() {
	GLsizeiptr buffer_size = sizeof(hat_leaf) + sizeof(hat_body) + sizeof(hat_strip) + sizeof(hat_bottom);

	glGenBuffers(1, &VBO_hat);
	glBindBuffer(GL_ARRAY_BUFFER, VBO_hat);
	glBufferData(GL_ARRAY_BUFFER, buffer_size, NULL, GL_STATIC_DRAW);

	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(hat_leaf), hat_leaf);
	glBufferSubData(GL_ARRAY_BUFFER, sizeof(hat_leaf), sizeof(hat_body), hat_body);
	glBufferSubData(GL_ARRAY_BUFFER, sizeof(hat_leaf) + sizeof(hat_body), sizeof(hat_strip), hat_strip);
	glBufferSubData(GL_ARRAY_BUFFER, sizeof(hat_leaf) + sizeof(hat_body) + sizeof(hat_strip), sizeof(hat_bottom), hat_bottom);

	glGenVertexArrays(1, &VAO_hat);
	glBindVertexArray(VAO_hat);
	glBindBuffer(GL_ARRAY_BUFFER, VBO_hat);
	glVertexAttribPointer(LOC_VERTEX, 2, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0));
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}

void draw_hat() {
	glBindVertexArray(VAO_hat);
	glUniform3fv(loc_primitive_color, 1, hat_color[HAT_LEAF]);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	glUniform3fv(loc_primitive_color, 1, hat_color[HAT_BODY]);
	glDrawArrays(GL_TRIANGLE_FAN, 4, 4);
	glUniform3fv(loc_primitive_color, 1, hat_color[HAT_STRIP]);
	glDrawArrays(GL_TRIANGLE_FAN, 8, 4);
	glUniform3fv(loc_primitive_color, 1, hat_color[HAT_BOTTOM]);
	glDrawArrays(GL_TRIANGLE_FAN, 12, 4);
	glBindVertexArray(0);
}


// ================================================================
//  cocktail  (Level 3)
// ================================================================
#define COCKTAIL_NECK 0
#define COCKTAIL_LIQUID 1
#define COCKTAIL_REMAIN 2
#define COCKTAIL_STRAW 3
#define COCKTAIL_DECO 4

GLfloat neck[6][2] = { { -6.0, -12.0 },{ -6.0, -11.0 },{ -1.0, 0.0 },{ 1.0, 0.0 },{ 6.0, -11.0 },{ 6.0, -12.0 } };
GLfloat liquid[6][2] = { { -1.0, 0.0 },{ -9.0, 4.0 },{ -12.0, 7.0 },{ 12.0, 7.0 },{ 9.0, 4.0 },{ 1.0, 0.0 } };
GLfloat remain[4][2] = { { -12.0, 7.0 },{ -12.0, 10.0 },{ 12.0, 10.0 },{ 12.0, 7.0 } };
GLfloat straw[4][2] = { { 7.0, 7.0 },{ 12.0, 12.0 },{ 14.0, 12.0 },{ 9.0, 7.0 } };
GLfloat deco[8][2] = { { 12.0, 12.0 },{ 10.0, 14.0 },{ 10.0, 16.0 },{ 12.0, 18.0 },{ 14.0, 18.0 },{ 16.0, 16.0 },{ 16.0, 14.0 },{ 14.0, 12.0 } };

GLfloat cocktail_color[5][3] = {
	{ 235 / 255.0f, 225 / 255.0f, 196 / 255.0f },
	{   0 / 255.0f,  63 / 255.0f, 122 / 255.0f },
	{ 235 / 255.0f, 225 / 255.0f, 196 / 255.0f },
	{ 191 / 255.0f, 255 / 255.0f,   0 / 255.0f },
	{ 218 / 255.0f, 165 / 255.0f,  32 / 255.0f }
};

GLuint VBO_cocktail, VAO_cocktail;

void prepare_cocktail() {
	GLsizeiptr buffer_size = sizeof(neck) + sizeof(liquid) + sizeof(remain) + sizeof(straw) + sizeof(deco);

	glGenBuffers(1, &VBO_cocktail);
	glBindBuffer(GL_ARRAY_BUFFER, VBO_cocktail);
	glBufferData(GL_ARRAY_BUFFER, buffer_size, NULL, GL_STATIC_DRAW);

	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(neck), neck);
	glBufferSubData(GL_ARRAY_BUFFER, sizeof(neck), sizeof(liquid), liquid);
	glBufferSubData(GL_ARRAY_BUFFER, sizeof(neck) + sizeof(liquid), sizeof(remain), remain);
	glBufferSubData(GL_ARRAY_BUFFER, sizeof(neck) + sizeof(liquid) + sizeof(remain), sizeof(straw), straw);
	glBufferSubData(GL_ARRAY_BUFFER, sizeof(neck) + sizeof(liquid) + sizeof(remain) + sizeof(straw), sizeof(deco), deco);

	glGenVertexArrays(1, &VAO_cocktail);
	glBindVertexArray(VAO_cocktail);
	glBindBuffer(GL_ARRAY_BUFFER, VBO_cocktail);
	glVertexAttribPointer(LOC_VERTEX, 2, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0));
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}

void draw_cocktail() {
	glBindVertexArray(VAO_cocktail);
	glUniform3fv(loc_primitive_color, 1, cocktail_color[COCKTAIL_NECK]);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 6);
	glUniform3fv(loc_primitive_color, 1, cocktail_color[COCKTAIL_LIQUID]);
	glDrawArrays(GL_TRIANGLE_FAN, 6, 6);
	glUniform3fv(loc_primitive_color, 1, cocktail_color[COCKTAIL_REMAIN]);
	glDrawArrays(GL_TRIANGLE_FAN, 12, 4);
	glUniform3fv(loc_primitive_color, 1, cocktail_color[COCKTAIL_STRAW]);
	glDrawArrays(GL_TRIANGLE_FAN, 16, 4);
	glUniform3fv(loc_primitive_color, 1, cocktail_color[COCKTAIL_DECO]);
	glDrawArrays(GL_TRIANGLE_FAN, 20, 8);
	glBindVertexArray(0);
}


// ================================================================
//  cake  (Level 4)
// ================================================================
#define CAKE_FIRE 0
#define CAKE_CANDLE 1
#define CAKE_BODY 2
#define CAKE_BOTTOM 3
#define CAKE_DECORATE 4

GLfloat cake_fire[4][2] = { { -0.5, 14.0 },{ -0.5, 13.0 },{ 0.5, 13.0 },{ 0.5, 14.0 } };
GLfloat cake_candle[4][2] = { { -1.0, 8.0 } ,{ -1.0, 13.0 },{ 1.0, 13.0 },{ 1.0, 8.0 } };
GLfloat cake_body[4][2] = { { 8.0, 5.0 },{ -8.0, 5.0 } ,{ -8.0, 8.0 },{ 8.0, 8.0 } };
GLfloat cake_bottom[4][2] = { { -10.0, 1.0 },{ -10.0, 5.0 },{ 10.0, 5.0 },{ 10.0, 1.0 } };
GLfloat cake_decorate[4][2] = { { -10.0, 0.0 },{ -10.0, 1.0 },{ 10.0, 1.0 },{ 10.0, 0.0 } };

GLfloat cake_color[5][3] = {
	{ 255 / 255.0f,   0 / 255.0f,   0 / 255.0f },
	{ 255 / 255.0f, 204 / 255.0f,   0 / 255.0f },
	{ 255 / 255.0f, 102 / 255.0f, 255 / 255.0f },
	{ 255 / 255.0f, 102 / 255.0f, 255 / 255.0f },
	{ 102 / 255.0f,  51 / 255.0f,   0 / 255.0f }
};

GLuint VBO_cake, VAO_cake;

void prepare_cake() {
	int size = sizeof(cake_fire);
	GLsizeiptr buffer_size = sizeof(cake_fire) * 5;

	glGenBuffers(1, &VBO_cake);
	glBindBuffer(GL_ARRAY_BUFFER, VBO_cake);
	glBufferData(GL_ARRAY_BUFFER, buffer_size, NULL, GL_STATIC_DRAW);

	glBufferSubData(GL_ARRAY_BUFFER, 0, size, cake_fire);
	glBufferSubData(GL_ARRAY_BUFFER, size, size, cake_candle);
	glBufferSubData(GL_ARRAY_BUFFER, size * 2, size, cake_body);
	glBufferSubData(GL_ARRAY_BUFFER, size * 3, size, cake_bottom);
	glBufferSubData(GL_ARRAY_BUFFER, size * 4, size, cake_decorate);

	glGenVertexArrays(1, &VAO_cake);
	glBindVertexArray(VAO_cake);
	glBindBuffer(GL_ARRAY_BUFFER, VBO_cake);
	glVertexAttribPointer(LOC_VERTEX, 2, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0));
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}

void draw_cake() {
	glBindVertexArray(VAO_cake);
	glUniform3fv(loc_primitive_color, 1, cake_color[CAKE_FIRE]);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	glUniform3fv(loc_primitive_color, 1, cake_color[CAKE_CANDLE]);
	glDrawArrays(GL_TRIANGLE_FAN, 4, 4);
	glUniform3fv(loc_primitive_color, 1, cake_color[CAKE_BODY]);
	glDrawArrays(GL_TRIANGLE_FAN, 8, 4);
	glUniform3fv(loc_primitive_color, 1, cake_color[CAKE_BOTTOM]);
	glDrawArrays(GL_TRIANGLE_FAN, 12, 4);
	glUniform3fv(loc_primitive_color, 1, cake_color[CAKE_DECORATE]);
	glDrawArrays(GL_TRIANGLE_FAN, 16, 4);
	glBindVertexArray(0);
}


// ================================================================
//  star  (Level 2) - 자체 설계 물체 #1
//
//  5각별을 TRIANGLE_FAN 하나로 표현.
//  설계 방법: 외부 꼭짓점 5개(반지름 14)와 오목한 내부 점 5개(반지름 6)를
//  72도 간격으로 교대 배치. 중심점에서 fan을 펼쳐 삼각형을 채움.
//  각도는 맨 위 꼭짓점부터 시계 방향으로 72도씩 증가.
//  좌표 계산: x = r * cos(θ), y = r * sin(θ)  (θ는 라디안)
//
//  정점 순서: 중심 → (외-0 내-0 외-1 내-1 외-2 내-2 외-3 내-3 외-4 내-4) → 마감
//  총 12 정점
// ================================================================
#define STAR_BODY 0

GLfloat star_body[12][2] = {
	{  0.0f,   0.0f  },  // 중심
	{  0.0f,  14.0f  },  // 외-0  90도  (r=14)
	{ -3.53f,  4.85f },  // 내-0 126도  (r=6)
	{-13.31f,  4.33f },  // 외-1 162도  (r=14)
	{ -5.71f, -1.85f },  // 내-1 198도  (r=6)
	{ -8.23f,-11.33f },  // 외-2 234도  (r=14)
	{  0.0f,  -6.0f  },  // 내-2 270도  (r=6)
	{  8.23f,-11.33f },  // 외-3 306도  (r=14)
	{  5.71f, -1.85f },  // 내-3 342도  (r=6)
	{ 13.31f,  4.33f },  // 외-4  18도  (r=14)
	{  3.53f,  4.85f },  // 내-4  54도  (r=6)
	{  0.0f,  14.0f  },  // 마감 (= 외-0, fan 닫기)
};

GLfloat star_color[1][3] = {
	{ 255 / 255.0f, 215 / 255.0f, 0 / 255.0f }  // 금색
};

GLuint VBO_star, VAO_star;

void prepare_star() {
	glGenBuffers(1, &VBO_star);
	glBindBuffer(GL_ARRAY_BUFFER, VBO_star);
	glBufferData(GL_ARRAY_BUFFER, sizeof(star_body), star_body, GL_STATIC_DRAW);

	glGenVertexArrays(1, &VAO_star);
	glBindVertexArray(VAO_star);
	glBindBuffer(GL_ARRAY_BUFFER, VBO_star);
	glVertexAttribPointer(LOC_VERTEX, 2, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0));
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}

void draw_star() {
	glBindVertexArray(VAO_star);
	glUniform3fv(loc_primitive_color, 1, star_color[STAR_BODY]);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 12);
	glBindVertexArray(0);
}


// ================================================================
//  fish  (Level 5) - 자체 설계 물체 #2
//
//  파트 5개: 몸통, 꼬리, 등지느러미, 배지느러미, 눈
//  VBO 정점 순서:
//    [0..9]   fish_body   TRIANGLE_FAN (10 정점)
//    [10..12] fish_tail   TRIANGLES    ( 3 정점)
//    [13..15] fish_dfin   TRIANGLES    ( 3 정점, 등지느러미)
//    [16..18] fish_vfin   TRIANGLES    ( 3 정점, 배지느러미)
//    [19]     fish_eye    POINTS       ( 1 정점)
//
//  +x 방향이 코. display()에서 +90도 로컬 회전으로 궤도 접선 방향을 향하게 함.
// ================================================================
#define FISH_BODY  0
#define FISH_TAIL  1
#define FISH_DFIN  2  // 등지느러미
#define FISH_VFIN  3  // 배지느러미
#define FISH_EYE   4

GLfloat fish_body[10][2] = {
	{  0.0f,  0.0f },  // 중심 허브
	{ 15.0f,  0.0f },  // 코
	{ 12.0f,  5.0f },
	{  5.0f,  8.0f },
	{ -5.0f,  7.0f },
	{-15.0f,  0.0f },  // 꼬리 연결점
	{ -5.0f, -7.0f },
	{  5.0f, -8.0f },
	{ 12.0f, -5.0f },
	{ 15.0f,  0.0f },  // 마감 (코와 같음)
};
GLfloat fish_tail[3][2] = { {-15.0f, 0.0f}, {-25.0f, 9.0f}, {-25.0f, -9.0f} };
GLfloat fish_dfin[3][2] = { {  0.0f, 8.0f}, {  4.0f, 15.0f}, { -6.0f,  8.0f} };  // 등지느러미
GLfloat fish_vfin[3][2] = { {  0.0f,-8.0f}, {  3.0f,-14.0f}, { -4.0f, -8.0f} };  // 배지느러미
GLfloat fish_eye[1][2] = { {  9.0f, 2.5f} };

GLfloat fish_color[5][3] = {
	{ 255 / 255.0f, 127 / 255.0f,  80 / 255.0f },  // 몸통 - 코랄
	{ 200 / 255.0f,  80 / 255.0f,  40 / 255.0f },  // 꼬리
	{ 255 / 255.0f, 190 / 255.0f, 110 / 255.0f },  // 등지느러미
	{ 255 / 255.0f, 160 / 255.0f,  80 / 255.0f },  // 배지느러미 (약간 진하게)
	{   0 / 255.0f,   0 / 255.0f,   0 / 255.0f },  // 눈
};

GLuint VBO_fish, VAO_fish;

void prepare_fish() {
	GLsizeiptr buffer_size = sizeof(fish_body) + sizeof(fish_tail)
		+ sizeof(fish_dfin) + sizeof(fish_vfin) + sizeof(fish_eye);

	glGenBuffers(1, &VBO_fish);
	glBindBuffer(GL_ARRAY_BUFFER, VBO_fish);
	glBufferData(GL_ARRAY_BUFFER, buffer_size, NULL, GL_STATIC_DRAW);

	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(fish_body), fish_body);
	glBufferSubData(GL_ARRAY_BUFFER, sizeof(fish_body), sizeof(fish_tail), fish_tail);
	glBufferSubData(GL_ARRAY_BUFFER, sizeof(fish_body) + sizeof(fish_tail), sizeof(fish_dfin), fish_dfin);
	glBufferSubData(GL_ARRAY_BUFFER, sizeof(fish_body) + sizeof(fish_tail) + sizeof(fish_dfin), sizeof(fish_vfin), fish_vfin);
	glBufferSubData(GL_ARRAY_BUFFER, sizeof(fish_body) + sizeof(fish_tail) + sizeof(fish_dfin) + sizeof(fish_vfin), sizeof(fish_eye), fish_eye);

	glGenVertexArrays(1, &VAO_fish);
	glBindVertexArray(VAO_fish);
	glBindBuffer(GL_ARRAY_BUFFER, VBO_fish);
	glVertexAttribPointer(LOC_VERTEX, 2, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0));
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}

void draw_fish() {
	glBindVertexArray(VAO_fish);
	glUniform3fv(loc_primitive_color, 1, fish_color[FISH_BODY]);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 10);
	glUniform3fv(loc_primitive_color, 1, fish_color[FISH_TAIL]);
	glDrawArrays(GL_TRIANGLES, 10, 3);
	glUniform3fv(loc_primitive_color, 1, fish_color[FISH_DFIN]);
	glDrawArrays(GL_TRIANGLES, 13, 3);
	glUniform3fv(loc_primitive_color, 1, fish_color[FISH_VFIN]);
	glDrawArrays(GL_TRIANGLES, 16, 3);
	glUniform3fv(loc_primitive_color, 1, fish_color[FISH_EYE]);
	glPointSize(5.0f);
	glDrawArrays(GL_POINTS, 19, 1);
	glPointSize(1.0f);
	glBindVertexArray(0);
}


// ================================================================
//  display
//
//  계층 구조:
//    Level 0: airplane  - 마우스로 이동
//    Level 1: hat x3    - L0 중심 원 운동, '1'키 on/off
//    Level 2: star x1   - L1 중심 원 운동 + 크기 펄싱 (자체물체)
//    Level 3: cocktail x2 - L2 중심 사각형 궤도
//    Level 4: cake x1   - L3 중심 리사주 8자 궤도
//    Level 5: fish x1   - L4 중심 원 운동, '5'키 on/off (자체물체)
// ================================================================
void display(void) {
	glm::mat4 ModelMatrix;
	glm::mat4 L0_M, L1_M, L2_M, L3_M, L4_M, L5_M;
	float angle, ox, oy, t, T, s, scale;

	glClear(GL_COLOR_BUFFER_BIT);

	// axes
	ModelMatrix = glm::mat4(1.0f);
	ModelViewProjectionMatrix = ViewProjectionMatrix * ModelMatrix;
	glUniformMatrix4fv(loc_ModelViewProjectionMatrix, 1, GL_FALSE, &ModelViewProjectionMatrix[0][0]);
	draw_axes();

	// ----- Level 0: airplane -----
	// 마우스 클릭/드래그 위치로 이동
	L0_M = glm::translate(glm::mat4(1.0f), glm::vec3(L0_x, L0_y, 0.0f));

	ModelMatrix = glm::scale(L0_M, glm::vec3(1.5f, 1.5f, 1.0f));
	ModelViewProjectionMatrix = ViewProjectionMatrix * ModelMatrix;
	glUniformMatrix4fv(loc_ModelViewProjectionMatrix, 1, GL_FALSE, &ModelViewProjectionMatrix[0][0]);
	draw_airplane();

	// ----- Level 1: hat x3 -----
	// L0 중심으로 120도 간격, 원 운동. '1'키로 on/off
	for (int i = 0; i < 3; i++) {
		angle = clock_L1 * 0.015f + i * (2.0f * 3.14159f / 3.0f);

		L1_M = L0_M;
		L1_M = glm::rotate(L1_M, angle, glm::vec3(0.0f, 0.0f, 1.0f));
		L1_M = glm::translate(L1_M, glm::vec3(120.0f, 0.0f, 0.0f));

		ModelMatrix = glm::scale(L1_M, glm::vec3(0.7f, 0.7f, 1.0f));
		ModelViewProjectionMatrix = ViewProjectionMatrix * ModelMatrix;
		glUniformMatrix4fv(loc_ModelViewProjectionMatrix, 1, GL_FALSE, &ModelViewProjectionMatrix[0][0]);
		draw_hat();

		// ----- Level 2: star -----
		// L1 중심 원 운동 + sin으로 크기 펄싱
		angle = timestamp * 0.025f + i * (2.0f * 3.14159f / 3.0f);
		scale = 1.0f + 0.55f * sinf(timestamp * 0.06f);

		L2_M = L1_M;
		L2_M = glm::rotate(L2_M, angle, glm::vec3(0.0f, 0.0f, 1.0f));
		L2_M = glm::translate(L2_M, glm::vec3(65.0f, 0.0f, 0.0f));

		ModelMatrix = glm::scale(L2_M, glm::vec3(scale, scale, 1.0f));
		ModelViewProjectionMatrix = ViewProjectionMatrix * ModelMatrix;
		glUniformMatrix4fv(loc_ModelViewProjectionMatrix, 1, GL_FALSE, &ModelViewProjectionMatrix[0][0]);
		draw_star();

		// ----- Level 3: cocktail x2 -----
		// L2 중심 사각형 궤도. j=0,1 두 개가 정사각형 위에서 반 바퀴 어긋나게 돌음
		for (int j = 0; j < 2; j++) {
			t = timestamp * 0.008f + j * 2.0f;  // j*2.0 → 반 바퀴 오프셋
			T = fmodf(t, 4.0f);
			if (T < 0.0f) T += 4.0f;
			s = 40.0f;  // 정사각형 반변 길이

			if (T < 1.0f) { ox = -s + T * 2 * s;     oy = -s; }
			else if (T < 2.0f) { ox = s;                   oy = -s + (T - 1) * 2 * s; }
			else if (T < 3.0f) { ox = s - (T - 2) * 2 * s; oy = s; }
			else { ox = -s;                  oy = s - (T - 3) * 2 * s; }

			L3_M = glm::translate(L2_M, glm::vec3(ox, oy, 0.0f));

			ModelMatrix = glm::scale(L3_M, glm::vec3(0.45f, 0.45f, 1.0f));
			ModelViewProjectionMatrix = ViewProjectionMatrix * ModelMatrix;
			glUniformMatrix4fv(loc_ModelViewProjectionMatrix, 1, GL_FALSE, &ModelViewProjectionMatrix[0][0]);
			draw_cocktail();

			// ----- Level 4: cake -----
			// L3 중심 리사주 8자 궤도: x=rx*cos(t), y=ry*sin(2t)
			// clock_L4를 사용해서 '4'키로 궤도를 멈출 수 있음 (요구 4)
			// off 시에도 L3(상위 레벨)의 움직임에는 여전히 종속됨
			t = clock_L4 * 0.018f + j * 3.14159f;
			ox = 22.0f * cosf(t);
			oy = 13.0f * sinf(2.0f * t);

			L4_M = glm::translate(L3_M, glm::vec3(ox, oy, 0.0f));

			ModelMatrix = glm::scale(L4_M, glm::vec3(0.35f, 0.35f, 1.0f));
			ModelViewProjectionMatrix = ViewProjectionMatrix * ModelMatrix;
			glUniformMatrix4fv(loc_ModelViewProjectionMatrix, 1, GL_FALSE, &ModelViewProjectionMatrix[0][0]);
			draw_cake();

			// ----- Level 5: fish -----
			// L4 중심 원 운동. '5'키로 on/off
			// 물고기 코가 진행 방향을 향하도록 +90도 로컬 회전
			angle = clock_L5 * 0.04f + j * 3.14159f;

			L5_M = L4_M;
			L5_M = glm::rotate(L5_M, angle, glm::vec3(0.0f, 0.0f, 1.0f));
			L5_M = glm::translate(L5_M, glm::vec3(16.0f, 0.0f, 0.0f));

			ModelMatrix = glm::rotate(L5_M, 3.14159f * 0.5f, glm::vec3(0.0f, 0.0f, 1.0f));
			ModelMatrix = glm::scale(ModelMatrix, glm::vec3(0.4f, 0.4f, 1.0f));
			ModelViewProjectionMatrix = ViewProjectionMatrix * ModelMatrix;
			glUniformMatrix4fv(loc_ModelViewProjectionMatrix, 1, GL_FALSE, &ModelViewProjectionMatrix[0][0]);
			draw_fish();
		}
	}

	glutSwapBuffers();
}


void myTimer(int value) {
	timestamp++;
	if (anim_L1_on) clock_L1++;
	if (anim_L4_on) clock_L4++;
	if (anim_L5_on) clock_L5++;
	glutPostRedisplay();
	glutTimerFunc(16, myTimer, 0);
}

void keyboard(unsigned char key, int x, int y) {
	switch (key) {
	case '1': // Level1 (hat) 애니메이션 on/off  [요구 3]
		anim_L1_on = !anim_L1_on;
		printf("Level 1 (hat) animation: %s\n", anim_L1_on ? "ON" : "OFF");
		break;
	case '4': // Level4 (cake) 애니메이션 on/off  [요구 4 - 추가된 레벨]
		anim_L4_on = !anim_L4_on;
		printf("Level 4 (cake) animation: %s\n", anim_L4_on ? "ON" : "OFF");
		break;
	case '5': // Level5 (fish) 애니메이션 on/off  [추가 구현]
		anim_L5_on = !anim_L5_on;
		printf("Level 5 (fish) animation: %s\n", anim_L5_on ? "ON" : "OFF");
		break;
	case 27: // ESC
		glutLeaveMainLoop();
		break;
	}
}

// GLUT y축은 위아래 반전이므로 OpenGL 좌표로 변환해줘야 함
void myMouse(int button, int state, int x, int y) {
	if (button == GLUT_LEFT_BUTTON) {
		if (state == GLUT_DOWN) {
			is_dragging = 1;
			L0_x = (float)x - win_width * 0.5f;
			L0_y = -(float)y + win_height * 0.5f;
			glutPostRedisplay();
		}
		else {
			is_dragging = 0;
		}
	}
}

void myMotion(int x, int y) {
	if (is_dragging) {
		L0_x = (float)x - win_width * 0.5f;
		L0_y = -(float)y + win_height * 0.5f;
		glutPostRedisplay();
	}
}

void reshape(int width, int height) {
	win_width = width, win_height = height;

	glViewport(0, 0, win_width, win_height);
	ProjectionMatrix = glm::ortho(-win_width / 2.0, win_width / 2.0,
		-win_height / 2.0, win_height / 2.0, -1000.0, 1000.0);
	ViewProjectionMatrix = ProjectionMatrix * ViewMatrix;

	update_axes();

	glutPostRedisplay();
}

void cleanup(void) {
	glDeleteVertexArrays(1, &VAO_axes);
	glDeleteBuffers(1, &VBO_axes);

	glDeleteVertexArrays(1, &VAO_airplane);
	glDeleteBuffers(1, &VBO_airplane);

	glDeleteVertexArrays(1, &VAO_hat);
	glDeleteBuffers(1, &VBO_hat);

	glDeleteVertexArrays(1, &VAO_cocktail);
	glDeleteBuffers(1, &VBO_cocktail);

	glDeleteVertexArrays(1, &VAO_cake);
	glDeleteBuffers(1, &VBO_cake);

	glDeleteVertexArrays(1, &VAO_star);
	glDeleteBuffers(1, &VBO_star);

	glDeleteVertexArrays(1, &VAO_fish);
	glDeleteBuffers(1, &VBO_fish);
}

void register_callbacks(void) {
	glutDisplayFunc(display);
	glutKeyboardFunc(keyboard);
	glutReshapeFunc(reshape);
	glutMouseFunc(myMouse);
	glutMotionFunc(myMotion);
	glutTimerFunc(16, myTimer, 0);
	glutCloseFunc(cleanup);
}

void prepare_shader_program(void) {
	ShaderInfo shader_info[3] = {
		{ GL_VERTEX_SHADER, "Shaders/simple.vert" },
		{ GL_FRAGMENT_SHADER, "Shaders/simple.frag" },
		{ GL_NONE, NULL }
	};

	h_ShaderProgram = LoadShaders(shader_info);
	glUseProgram(h_ShaderProgram);

	loc_ModelViewProjectionMatrix = glGetUniformLocation(h_ShaderProgram, "u_ModelViewProjectionMatrix");
	loc_primitive_color = glGetUniformLocation(h_ShaderProgram, "u_primitive_color");
}

void initialize_OpenGL(void) {
	glEnable(GL_MULTISAMPLE);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	glClearColor(0.08f, 0.12f, 0.22f, 1.0f);  // 어두운 남색 배경
	ViewMatrix = glm::mat4(1.0f);
}

void prepare_scene(void) {
	prepare_axes();
	prepare_airplane();
	prepare_hat();
	prepare_cocktail();
	prepare_cake();
	prepare_star();
	prepare_fish();
}

void initialize_renderer(void) {
	register_callbacks();
	prepare_shader_program();
	initialize_OpenGL();
	prepare_scene();
}

void initialize_glew(void) {
	GLenum error;

	glewExperimental = GL_TRUE;

	error = glewInit();
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

void greetings(char* program_name, char messages[][256], int n_message_lines) {
	fprintf(stdout, "**************************************************************\n\n");
	fprintf(stdout, "  PROGRAM NAME: %s\n\n", program_name);
	fprintf(stdout, "    This program was coded for CSE4170 students\n");
	fprintf(stdout, "      of Dept. of Comp. Sci. & Eng., Sogang University.\n\n");

	for (int i = 0; i < n_message_lines; i++)
		fprintf(stdout, "%s\n", messages[i]);
	fprintf(stdout, "\n**************************************************************\n\n");

	initialize_glew();
}

#define N_MESSAGE_LINES 3
void main(int argc, char* argv[]) {
	char program_name[64] = "Sogang CSE4170 HW2 - 2D Hierarchical Modeling";
	char messages[N_MESSAGE_LINES][256] = {
		"    - Keys: '1' Level1(hat) ON/OFF  |  '4' Level4(cake) ON/OFF  |  '5' Level5(fish) ON/OFF  |  ESC",
		"    - Mouse: left-click / drag to move Level0 (airplane)",
		"    - Hierarchy: airplane(L0) > hat(L1) > star(L2) > cocktail(L3) > cake(L4,req4) > fish(L5)"
	};

	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_MULTISAMPLE);  // DOUBLE 추가 (애니메이션)
	glutInitWindowSize(1200, 800);
	glutInitContextVersion(3, 3);
	glutInitContextProfile(GLUT_CORE_PROFILE);
	glutCreateWindow(program_name);

	greetings(program_name, messages, N_MESSAGE_LINES);
	initialize_renderer();

	glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);
	glutMainLoop();
}