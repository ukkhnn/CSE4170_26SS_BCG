#pragma once

#define BACKGROUND_COLOR  159.0f / 255.0f, 226.0f / 255.0f, 191.0f / 255.0f
#define POINT_COLOR  0.0f, 0.0f, 0.0f
#define LINE_COLOR_UNSELECTED  0.0f, 0.0f, 1.0f
#define LINE_COLOR_SELECTED  1.0f, 0.0f, 1.0f
#define CENTER_POINT_COLOR 1.0f, 0.0f, 0.0f

#define ROTATION_STEP 100 // ms
#define TRANSLATION_OFFSET 0.05f

#define SCALE_UP_FACTOR			1.02f
#define SCALE_DOWN_FACTOR   (1.0f / SCALE_UP_FACTOR)

#define COS_5_DEGREES 0.9961947f
#define SIN_5_DEGREES 0.08715574f

#define CENTER_SELECTION_SENSITIVITY 0.01f

typedef struct {
	int width, height;
	int initial_anchor_x, initial_anchor_y;
} Window;

typedef struct {
	int leftbuttonpressed;
	int move_mode;
	int rotation_mode;
	int polygon_mode;
} Status;

#define MAX_POSITIONS 256
typedef struct {
	float point[MAX_POSITIONS][2];
	int n_points;
	float center_x, center_y;
} My_Polygon;

// Functions for My_Polygon
void add_point(My_Polygon* pg, Window* wd, int x, int y);
void draw_lines_by_points(My_Polygon* pg, float line_color_r, float line_color_g, float line_color_b);
void update_center_of_gravity(My_Polygon* pg);
void move_points(My_Polygon* pg, float del_x, float del_y);
void rotate_points_around_center_of_grivity(My_Polygon* pg);
void scale_points_around_center_of_grivity(My_Polygon* pg, float scale_factor);
