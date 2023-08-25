#include "../include/scene.h"
#include "../include/vk_objects.h"

#include <iostream>
#include <memory>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>


Scene::Scene(VulkanEngine* engine) {
	Scene::engine = engine;
}

int Scene::get_selected_obj_id() {
	return selected_obj_id;
}

int Scene::get_hovering_obj_id() {
	return hovering_obj_id;
}

uint8_t Scene::add_object(Object* object_ptr) {
	uint8_t new_id = 0;

	for (Object* object : objects) {
		uint8_t object_id = object->get_id();

		if (object_id >= new_id) {
			new_id = object_id + 1;
		}
	}

	object_ptr->set_id(new_id);

	objects.push_back(object_ptr);

	return new_id;
}

void Scene::remove_object(uint8_t object_id) {
	for (Object* object_ptr : objects) {
		if (object_ptr->get_id() == object_id) {
			object_ptr->on_remove();

			// remove object from vector
			// position might have changed after on_remove()
			for (int i = 0; i < objects.size(); i++) {
				if (objects[i]->get_id() == object_id) {
					objects.erase(objects.begin() + i);
				}
			}

			delete object_ptr;
			return;
		}
	}
	std::cout << "object with id " << object_id << " not found" << std::endl;
}

Object* Scene::get_object_ptr(uint8_t object_id) {
	for (Object* object : objects) {
		if (object->get_id() == object_id) return object;
	}
	return nullptr;
}

std::vector<uint8_t> Scene::get_ids() {
	std::vector<uint8_t> ids;

	for (Object* object_ptr : objects) {
		ids.push_back(object_ptr->get_id());
	}

	return ids;
}

double Scene::triangle_area(glm::vec2 a, glm::vec2 b, glm::vec2 c) {
	float A = sqrt((double)(b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y));
	float B = sqrt((double)(b.x - c.x) * (b.x - c.x) + (b.y - c.y) * (b.y - c.y));
	float C = sqrt((double)(a.x - c.x) * (a.x - c.x) + (a.y - c.y) * (a.y - c.y));

	// Heron's formula for area calculation
	// area = sqrt( s * (s-a) * (s-b) * (s-c))

	float s = (A + B + C) / 2;
	float perimeter = A + B + C;
	
	return sqrt(s * (s - A) * (s - B) * (s - C));
}

bool Scene::point_inside_triangle(glm::vec2 point, std::vector<glm::vec2> triangle) {
	float A = triangle_area(triangle[0], triangle[1], triangle[2]);
	float A1 = triangle_area(point, triangle[1], triangle[2]);
	float A2 = triangle_area(triangle[0], point, triangle[2]);
	float A3 = triangle_area(triangle[0], triangle[1], point);
	
	float sum = A1 + A2 + A3;

	return abs(A - sum) < 1.0f / (window_width * 8.0f);
}

// assuming viewport normalized on y axis and camera looking at 0,0
void Scene::cursor_position_callback(int xpos, int ypos) {
	// Transform NDC to camera-space coordinate
	// inverse view transformation not needed since camera is fixed

	// make cursor coordinates from -1 to +1
	float ndc_x = ((float)xpos / window_width) * 2.f - 1.f;
	float ndc_y = - ((float)ypos / window_height) * 2.f + 1.f;

	// inverse of projection matrix
	glm::vec4 clip_coords = { ndc_x, ndc_y, -1.0f, 1.0f };
	glm::mat4 proj = glm::perspective(glm::radians(60.0f), window_width / (float)window_height, 0.1f, 10.0f);
	glm::mat4 proj_inv = glm::inverse(proj);
	glm::vec4 camera_coords = proj_inv * clip_coords;
	glm::mat4 view_inv = glm::inverse(glm::lookAt(glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f)));
	glm::vec4 ray_world = glm::normalize(camera_coords * view_inv);

	float t = -1 / ray_world.z;
	float x_at_z_zero = ray_world.x * t;
	float y_at_z_zero = ray_world.y * t;

	//std::cout << x_at_z_zero << " " << y_at_z_zero << std::endl;

	// check if cursor is hovering an object
	int new_hovering_obj_id = -1; // reset

	for (int o = objects.size() - 1; o >= 0; o--) {
		std::vector<Vertex> vertices = objects[o]->get_vertices();
		std::vector<uint16_t> indices = objects[o]->get_indices();
		
		if (new_hovering_obj_id >= 0) break;

		for (int i = 0; i < indices.size(); i += 3) {
			std::vector<glm::vec2> triangle;
			triangle.resize(3);

			for (int j = 0; j < 3; j++) {
				auto vertex_normal = glm::normalize(vertices[indices[i + j]].pos);
				float flat_t = -1 / vertex_normal.z;
				triangle[j] = glm::vec2({ vertex_normal.x * flat_t, vertex_normal.y * flat_t });
			}

			// Cursor is hovering obj
			if (point_inside_triangle(
				glm::vec2({ x_at_z_zero, y_at_z_zero }),
				triangle
			)) {
				// invoke object hover enter event
				if (objects[o]->get_id() != hovering_obj_id) {
					objects[o]->on_hover_enter();
				}

				new_hovering_obj_id = objects[o]->get_id();
				break;
			}
		}
	}

	// invoke object hover leave event
	if (hovering_obj_id != new_hovering_obj_id && hovering_obj_id >= 0) {
		Object* obj = get_object_ptr(hovering_obj_id);
		if (obj != nullptr) obj->on_hover_leave();
	}

	hovering_obj_id = new_hovering_obj_id;

	//std::cout << new_hovering_obj_id << ", " << selected_obj_id << std::endl;

	// perform dragging
	if (dragging_obj_id >= 0)
		get_object_ptr(dragging_obj_id)->on_move(ray_world);
}

void Scene::mouse_button_callback(int button, int action, int mods) {
	if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
		if (hovering_obj_id >= 0) {
			auto object = get_object_ptr(hovering_obj_id);
			if (!object->selectable())
				return;
		}
			
		if (selected_obj_id >= 0) {
			Object* obj = get_object_ptr(selected_obj_id);
			if (obj != nullptr) obj->on_release();
		}

		selected_obj_id = hovering_obj_id;

		if (selected_obj_id >= 0) {
			Object* obj = get_object_ptr(selected_obj_id);
			if (obj != nullptr) obj->on_select();
		}
	}

	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
		if (hovering_obj_id >= 0) {
			dragging_obj_id = hovering_obj_id;
			// start dragging
			//get_object_ptr(dragging_obj_id)->on_move(world_curson_pos_x, world_curson_pos_y);
		}
	}

	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
		if (dragging_obj_id >= 0)
			dragging_obj_id = -1;
	}
}


void Scene::window_resize_callback(unsigned int width, unsigned int height) {
	Scene::window_width = width;
	Scene::window_height = height;
}