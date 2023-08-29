#pragma once

#include <GLFW/glfw3.h>

#include <vector>
#include <memory>
#include "vk_objects.h"
#include "vk_engine.h"

class Object;

class VulkanEngine;

class Scene {
private:
	int hoveringObjId = -1;	// mouse hover
	int selectedObjId = -1;	// clicked
	int draggingObjId = -1;
	int lastDragginObjId = -1;

	float lastDraggingX = 0.0f;
	float lastDraggingY = 0.0f;

	std::vector<Object*> pObjects;

	VulkanEngine* pEngine;

public:
	Scene(VulkanEngine* pEngine);

	int getSelectedObjectId() { return selectedObjId; };
	int getHoveringObjectId() { return hoveringObjId; };
	int getDragginObjectId() { return draggingObjId; };

	uint8_t addObject(Object* pObject);
	void removeObject(uint8_t objectId);
	Object* getObjectPointer(uint8_t objectId);
	std::vector<uint8_t> getIds();
	double triangleArea(glm::vec2 a, glm::vec2 b, glm::vec2 c);
	bool pointInsideTriangle(glm::vec2 point, std::vector<glm::vec2> triangle);
	void mouseRayCallback(glm::vec4 mouseRay);
	void mouseButtonCallback(int button, int action, int mods);
	VulkanEngine* getEnginePointer() {
		return pEngine;
	};
};