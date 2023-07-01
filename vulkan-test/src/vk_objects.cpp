#include "../include/vk_objects.h"
#include "../include/clamp.h"

#include <memory>
#include <iostream>

// Rect
Rect::Rect(uint8_t id, Scene* scene_ptr, float width, float heigth, float pos_x, float pos_y, glm::vec3 color, uint8_t parent_id, uint16_t vertex_id) {
    Rect::width = width;
    Rect::heigth = heigth;
    Rect::pos_x = pos_x;
    Rect::pos_y = pos_y;
    Rect::color = color;
    Rect::vertex_id = vertex_id;
    Rect::id = id;
    Rect::parent_id = parent_id;
    Rect::scene_ptr = scene_ptr;
}

std::vector<Vertex> Rect::get_vertices() {
    glm::vec3 final_color = color;
    
    if (highlighted) {
        final_color.r += .2f;
        final_color.g += .2f;
        final_color.b += .2f;
    }

    return {
        {{ -Rect::width / 2 + Rect::pos_x, -Rect::heigth / 2 + Rect::pos_y }, final_color},
        {{ Rect::width / 2 + Rect::pos_x, -Rect::heigth / 2 + Rect::pos_y}, final_color},
        {{ Rect::width / 2 + Rect::pos_x, Rect::heigth / 2 + Rect::pos_y}, final_color},
        {{ -Rect::width / 2 + Rect::pos_x, Rect::heigth / 2 + Rect::pos_y}, final_color}
    };
}

std::vector<uint16_t> Rect::get_indices() {
    return {
        0, 1, 2, 2, 3, 0
    };
}

void Rect::on_hover_enter() {
}

void Rect::on_hover_leave() {
}

void Rect::on_select() {
    highlighted = true;
}

void Rect::on_release() {
    highlighted = false;
}

void Rect::on_move(float pos_x, float pos_y) {
    Rect::pos_x = pos_x;
    Rect::pos_y = pos_y;

    Plane* parent = dynamic_cast<Plane*>( scene_ptr->get_object_ptr(parent_id));
    
    if (parent != nullptr)
        parent->move_vertex(vertex_id, pos_x, pos_y);
}

// Plane
Plane::Plane(uint8_t id, Scene* scene_ptr, float width, float heigth, float pos_x, float pos_y, glm::vec3 color) {
    Plane::vertices = {
        {{ -width / 2 + pos_x, -heigth / 2 + pos_y }, color},
        {{ width / 2 + pos_x, -heigth / 2 + pos_y}, color},
        {{ width / 2 + pos_x, heigth / 2 + pos_y}, color},
        {{ -width / 2 + pos_x, heigth / 2 + pos_y}, color}
    };

    Plane::pos_x = pos_x;
    Plane::pos_y = pos_y;
    Plane::id = id;
    Plane::scene_ptr = scene_ptr;
}

std::vector<Vertex> Plane::get_vertices() {
    return vertices;
}

std::vector<uint16_t> Plane::get_indices() {
    return {
        0, 1, 2, 2, 3, 0
    };
}

void Plane::on_hover_enter() {
}

void Plane::on_hover_leave() {
}

void Plane::on_select() {
    for (auto& vertex : vertices) {
        vertex.color.r += .1f;
        vertex.color.g += .1f;
        vertex.color.b += .1f;
    }

    // add marker
    uint8_t first_id = scene_ptr->get_objects()->size();
    for (int i = 0; i < vertices.size(); i++) {
        uint8_t id = first_id + i;
        scene_ptr->add_object(new Rect(id, scene_ptr, .05f, .05f, vertices[i].pos.x, vertices[i].pos.y, {1.0f, 1.0f,1.0f}, Plane::id, i));
        marker_ids.push_back(id);
    }
}

void Plane::on_release() {
    for (auto& vertex : vertices) {
        vertex.color.r -= .1f;
        vertex.color.g -= .1f;
        vertex.color.b -= .1f;
    }
    
    // remove markers
    for (auto marker_id : marker_ids) {
        scene_ptr->remove_object(marker_id);
    }
    marker_ids.clear();
}

void Plane::on_move(float pos_x, float pos_y) {
    float delta_pos_x = pos_x - Plane::pos_x;
    float delta_pos_y = pos_y - Plane::pos_y;
    
    Plane::pos_x = pos_x;
    Plane::pos_y = pos_y;

    for (auto& vertex : vertices) {
        vertex.pos.x += delta_pos_x;
        vertex.pos.y += delta_pos_y;
    }

    if (marker_ids.size() > 0) {
        auto vertices = get_vertices();
           
        // assuming vertex number same as marker number
        for (int i = 0; i < vertices.size(); i++) {
            scene_ptr->get_object_ptr(marker_ids[i])->on_move(vertices[i].pos.x, vertices[i].pos.y);
        }
    }
}

void Plane::move_vertex(unsigned int vertex_id, float pos_x, float pos_y) {
    vertices[vertex_id].pos.x = pos_x;
    vertices[vertex_id].pos.y = pos_y;
}
