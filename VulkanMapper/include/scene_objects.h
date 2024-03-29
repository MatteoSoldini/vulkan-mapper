#pragma once

#include <vector>
#include <memory>
#include "vk_types.h"
#include "scene.h"
#include <string>
#include "media_manager.h"
#include "vm_types.h"
#include "app.h"

class Scene;

class App;


class Object {
private:
	uint8_t id;

public:
	uint8_t getId() { return Object::id; };
	void setId(uint8_t id) {
		Object::id = id;
	}
	virtual void beforeRemove() = 0;

	// renderer
	virtual std::vector<Vertex> getVertices() = 0;
	virtual std::vector<uint16_t> getIndices() = 0;
	virtual std::string getPipelineName() = 0;

	// events
	virtual void hoveringStart() = 0;
	virtual void hoveringStop() = 0;
	virtual void onSelect() = 0;
	virtual void onRelease() = 0;
	virtual void onMove(float deltaX, float deltaY) = 0;

	// config
	virtual bool selectable() = 0;
};

class Plane : public Object {
private:
	App* pApp;
	float pos_x = 0;
	float pos_y = 0;
	std::vector<Vertex> vertices;
	std::vector<uint8_t> markerIds;
	std::vector<uint8_t> lineIds;
	float width;
	float height;
	int mediaId = -1;	// -1 for unset

public:
	Plane(App* pApp, Scene* scene_ptr, float width, float height, float pos_x, float pos_y);
	
	Scene* pScene;

	void beforeRemove();

	std::vector<Vertex> getVertices();
	std::vector<uint16_t> getIndices();
	std::string getPipelineName();

	MediaId_t getMediaId() {
		return Plane::mediaId;
	}

	void setMediaId(int m_id);

	void hoveringStart();
	void hoveringStop();
	void onSelect();
	void onRelease();
	void onMove(float deltaX, float deltaY);
	void moveVertex(unsigned int vertex_id);
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

	void beforeRemove() { return; };

	std::vector<Vertex> getVertices();
	std::vector<uint16_t> getIndices();
	std::string getPipelineName() { return "color"; };

	void hoveringStart();
	void hoveringStop();
	void onSelect();
	void onRelease();
	void onMove(float deltaX, float deltaY);
	bool selectable() { return true; };
	glm::vec2 get_position();
	uint16_t get_vertex_id();
};

class Line : public Object {
private:
	std::vector<Vertex> vertices;

public:
	Line(glm::vec2 firstPoint, glm::vec2 secondPoint);

	// renderer
	std::vector<Vertex> getVertices();
	std::vector<uint16_t> getIndices();
	std::string getPipelineName() { return "line"; };

	void beforeRemove() { return; };

	// events
	void hoveringStart() { return; };
	virtual void hoveringStop() { return; };
	virtual void onSelect() { return; };
	virtual void onRelease() { return; };
	virtual void onMove(float deltaX, float deltaY) { return; };
	void moveVertices(glm::vec2 firstPoint, glm::vec2 secondPoint);

	// config
	bool selectable() { return true; };
};