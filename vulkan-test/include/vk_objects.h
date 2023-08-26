#pragma once

#include <vector>
#include <memory>
#include "vk_types.h"
#include "scene.h"
#include <string>

class Scene;

class Object {
private:
	uint8_t id;

public:
	// scene
	uint8_t get_id() { return Object::id; };
	void set_id(uint8_t id) {
		Object::id = id;
	}
	virtual void on_remove() = 0;

	// renderer
	virtual std::vector<Vertex> get_vertices() = 0;
	virtual std::vector<uint16_t> get_indices() = 0;
	virtual std::string get_pipeline_name() = 0;

	// events
	virtual void on_hover_enter() = 0;
	virtual void on_hover_leave() = 0;
	virtual void on_select() = 0;
	virtual void on_release() = 0;
	virtual void on_move(glm::vec4 ray_world) = 0;

	// config
	virtual bool selectable() = 0;
};

class Plane : public Object {
private:
	float pos_x = 0;
	float pos_y = 0;
	std::vector<Vertex> vertices;
	std::vector<uint8_t> marker_ids;
	float width;
	float height;
	std::string image_path = "";

public:
	Plane(Scene* scene_ptr, float width, float height, float pos_x, float pos_y);
	
	Scene* scene_ptr;

	void on_remove();

	std::vector<Vertex> get_vertices();
	std::vector<uint16_t> get_indices();
	std::string get_pipeline_name();

	std::string get_image_path() {
		return Plane::image_path;
	}

	void set_image_path(std::string image_path);

	void on_hover_enter();
	void on_hover_leave();
	void on_select();
	void on_release();
	void on_move(glm::vec4 ray_world);
	void move_vertex(unsigned int vertex_id, glm::vec4 ray_world);
	bool selectable() { return true; };
};

class Marker : public Object {
private:
	const float dimension = 0.05f;

	float pos_x;
	float pos_y;
	bool highlighted = false;
	glm::vec3 color;
	uint8_t parent_id;
	uint16_t vertex_id;

public:
	Marker(Scene* scene_ptr, float pos_x, float pos_y, glm::vec3 color, uint8_t parent_id, uint16_t vertex_id);
	
	Scene* scene_ptr;

	void on_remove() {
		return;
	};

	std::vector<Vertex> get_vertices();
	std::vector<uint16_t> get_indices();
	std::string get_pipeline_name() { return "color"; };

	void on_hover_enter();
	void on_hover_leave();
	void on_select();
	void on_release();
	void on_move(glm::vec4 ray_world);
	bool selectable() { return true; };
	glm::vec2 get_position();
	uint16_t get_vertex_id();
};