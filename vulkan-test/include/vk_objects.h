#pragma once

#include <vector>
#include <memory>
#include "vk_types.h"
#include "scene.h"

class Scene;

class Object {
public:
	// scene
	virtual uint8_t get_id() = 0;

	// renderer
	virtual std::vector<Vertex> get_vertices() = 0;
	virtual std::vector<uint16_t> get_indices() = 0;

	// events
	virtual void on_hover_enter() = 0;
	virtual void on_hover_leave() = 0;
	virtual void on_select() = 0;
	virtual void on_release() = 0;
	virtual void on_move(float pos_x, float pos_y) = 0;

	// config
	virtual bool selectable() = 0;
};

class Plane : public Object, public std::enable_shared_from_this<Plane> {
private:
	uint8_t id;
	float pos_x = 0;
	float pos_y = 0;
	std::vector<Vertex> vertices;
	std::vector<uint8_t> marker_ids;

public:
	Plane(uint8_t id, Scene* scene_ptr, float width, float heigth, float pos_x, float pos_y, glm::vec3 color);
	
	Scene* scene_ptr;

	uint8_t get_id() {
		return id;
	};

	std::vector<Vertex> get_vertices();
	std::vector<uint16_t> get_indices();
	void on_hover_enter();
	void on_hover_leave();
	void on_select();
	void on_release();
	void on_move(float pos_x, float pos_y);
	void move_vertex(unsigned int vertex_id, float pos_x, float pos_y);
	bool selectable() { return true; };
};

class Rect : public Object {
private:
	uint8_t id;
	float width;
	float heigth;
	float pos_x;
	float pos_y;
	bool highlighted = false;
	glm::vec3 color;
	uint8_t parent_id;
	uint16_t vertex_id;

public:
	Rect(uint8_t id, Scene* scene_ptr, float width, float heigth, float pos_x, float pos_y, glm::vec3 color, uint8_t parent_id, uint16_t vertex_id);
	
	Scene* scene_ptr;

	uint8_t get_id() {
		return id;
	};

	std::vector<Vertex> get_vertices();
	std::vector<uint16_t> get_indices();

	void on_hover_enter();
	void on_hover_leave();
	void on_select();
	void on_release();
	void on_move(float pos_x, float pos_y);
	bool selectable() { return true; };
};