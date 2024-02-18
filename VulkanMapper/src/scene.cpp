#include "../include/scene.h"
#include "../include/scene_objects.h"

#include <iostream>
#include <memory>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

Scene::Scene(App* pApp) {
	Scene::pApp = pApp;
}

uint8_t Scene::addObject(Object* object_ptr) {
	uint8_t new_id = 0;

	for (Object* object : pObjects) {
		uint8_t object_id = object->getId();

		if (object_id >= new_id) {
			new_id = object_id + 1;
		}
	}

	object_ptr->setId(new_id);

	pObjects.push_back(object_ptr);

	return new_id;
}

void Scene::removeObject(uint8_t object_id) {
	for (Object* object_ptr : pObjects) {
		if (object_ptr->getId() == object_id) {
			object_ptr->beforeRemove();

			// remove object from vector
			// position might have changed after beforeRemove()
			for (int i = 0; i < pObjects.size(); i++) {
				if (pObjects[i]->getId() == object_id) {
					pObjects.erase(pObjects.begin() + i);
				}
			}

			delete object_ptr;
			return;
		}
	}
	std::cout << "object with id " << object_id << " not found" << std::endl;
}

Object* Scene::getObjectPointer(uint8_t object_id) {
	for (Object* object : pObjects) {
		if (object->getId() == object_id) return object;
	}
	return nullptr;
}

std::vector<uint8_t> Scene::getIds() {
	std::vector<uint8_t> ids;

	for (Object* object_ptr : pObjects) {
		ids.push_back(object_ptr->getId());
	}

	return ids;
}

double Scene::triangleArea(glm::vec2 a, glm::vec2 b, glm::vec2 c) {
	float A = sqrt((double)(b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y));
	float B = sqrt((double)(b.x - c.x) * (b.x - c.x) + (b.y - c.y) * (b.y - c.y));
	float C = sqrt((double)(a.x - c.x) * (a.x - c.x) + (a.y - c.y) * (a.y - c.y));

	// Heron's formula for area calculation
	// area = sqrt( s * (s-a) * (s-b) * (s-c))

	float s = (A + B + C) / 2;
	float perimeter = A + B + C;
	
	return sqrt(s * (s - A) * (s - B) * (s - C));
}

bool Scene::pointInsideTriangle(glm::vec2 point, std::vector<glm::vec2> triangle) {
	float A = triangleArea(triangle[0], triangle[1], triangle[2]);
	float A1 = triangleArea(point, triangle[1], triangle[2]);
	float A2 = triangleArea(triangle[0], point, triangle[2]);
	float A3 = triangleArea(triangle[0], triangle[1], point);
	
	float sum = A1 + A2 + A3;

	return abs(A - sum) < 1.0f / 8000.0f;
}

// assuming viewport normalized on y axis and camera looking at 0,0
void Scene::mouseRayCallback(glm::vec4 mouseRay) {

	float t = -1 / mouseRay.z;
	float mouseWorldX = mouseRay.x * t;
	float mouseWorldY = mouseRay.y * t;

	//std::cout << x_at_z_zero << " " << y_at_z_zero << std::endl;

	// check if cursor is hovering an object
	int new_hovering_obj_id = -1; // reset

	for (int o = pObjects.size() - 1; o >= 0; o--) {
		std::vector<Vertex> vertices = pObjects[o]->getVertices();
		std::vector<uint16_t> indices = pObjects[o]->getIndices();
		
		if (new_hovering_obj_id >= 0) break;

		// skip if line
		if (pObjects[o]->getPipelineName() != "line") {
			for (int i = 0; i < indices.size(); i += 3) {
				std::vector<glm::vec2> triangle;
				triangle.resize(3);

				for (int j = 0; j < 3; j++) {
					auto vertex_normal = glm::normalize(vertices[indices[i + j]].pos);
					float flat_t = -1 / vertex_normal.z;
					triangle[j] = glm::vec2({ vertex_normal.x * flat_t, vertex_normal.y * flat_t });
				}

				// Cursor is hovering obj
				if (pointInsideTriangle(
					glm::vec2({ mouseWorldX, mouseWorldY }),
					triangle
				)) {
					// invoke object hover enter event
					if (pObjects[o]->getId() != hoveringObjId) {
						pObjects[o]->hoveringStart();
					}

					new_hovering_obj_id = pObjects[o]->getId();
					break;
				}
			}
		}

		
	}

	// invoke object hover leave event
	if (hoveringObjId != new_hovering_obj_id && hoveringObjId >= 0) {
		Object* obj = getObjectPointer(hoveringObjId);
		if (obj != nullptr) obj->hoveringStop();
	}

	hoveringObjId = new_hovering_obj_id;

	//std::cout << new_hovering_obj_id << ", " << selected_obj_id << std::endl;

	// perform dragging

	// update last dragging position on new dragging object before actual dragging
	if (lastDragginObjId != draggingObjId) {
		lastDraggingX = mouseWorldX;
		lastDraggingY = mouseWorldY;
		lastDragginObjId = draggingObjId;
	}

	if (draggingObjId >= 0) {
		getObjectPointer(draggingObjId)->onMove(mouseWorldX - lastDraggingX, mouseWorldY - lastDraggingY);
		lastDraggingX = mouseWorldX;
		lastDraggingY = mouseWorldY;
	}
}

void Scene::mouseButtonCallback(int button, int action, int mods) {
	if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
		if (hoveringObjId >= 0) {
			auto object = getObjectPointer(hoveringObjId);
			if (!object->selectable())
				return;
		}
			
		if (selectedObjId >= 0) {
			Object* obj = getObjectPointer(selectedObjId);
			if (obj != nullptr) obj->onRelease();
		}

		selectedObjId = hoveringObjId;

		if (selectedObjId >= 0) {
			Object* obj = getObjectPointer(selectedObjId);
			if (obj != nullptr) obj->onSelect();
		}
	}

	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
		if (hoveringObjId >= 0) {
			draggingObjId = hoveringObjId;
			// start dragging
			//get_object_ptr(dragging_obj_id)->on_move(world_curson_pos_x, world_curson_pos_y);
		}
	}

	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
		if (draggingObjId >= 0)
			draggingObjId = -1;
	}
}