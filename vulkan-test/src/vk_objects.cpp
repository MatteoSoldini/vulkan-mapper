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
        {{ -Rect::width / 2 + Rect::pos_x, -Rect::heigth / 2 + Rect::pos_y, -1.0f }, final_color, {1.0f, 0.0f}},
        {{ Rect::width / 2 + Rect::pos_x, -Rect::heigth / 2 + Rect::pos_y, -1.0f}, final_color, {0.0f, 0.0f}},
        {{ Rect::width / 2 + Rect::pos_x, Rect::heigth / 2 + Rect::pos_y, -1.0f}, final_color, {0.0f, 1.0f}},
        {{ -Rect::width / 2 + Rect::pos_x, Rect::heigth / 2 + Rect::pos_y, -1.0f}, final_color, {1.0f, 1.0f}}
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

glm::vec2 Rect::get_position() {
    return { Rect::pos_x, Rect::pos_y };
}

uint16_t Rect::get_vertex_id() {
    return Rect::vertex_id;
}

void Rect::on_move(glm::vec4 ray_world) {
    float t = -1 / ray_world.z;

    Rect::pos_x = ray_world.x * t;
    Rect::pos_y = ray_world.y * t;

    Plane* parent = dynamic_cast<Plane*>(scene_ptr->get_object_ptr(parent_id));
    
    if (parent != nullptr)
        parent->move_vertex(vertex_id, ray_world);
}

// Plane
Plane::Plane(uint8_t id, Scene* scene_ptr, float width, float height, float pos_x, float pos_y, glm::vec3 color) {
    Plane::width = width;
    Plane::height = height;
    
    Plane::vertices = {
        {{ -width / 2 + pos_x, -height / 2 + pos_y, -1.0f }, color, {0.0f, 1.0f}},
        {{ width / 2 + pos_x, -height / 2 + pos_y, -1.0f}, color, {1.0f, 1.0f}},
        {{ width / 2 + pos_x, height / 2 + pos_y, -1.0f}, color, {1.0f, 0.0f}},
        {{ -width / 2 + pos_x, height / 2 + pos_y, -1.0f}, color, {0.0f, 0.0f}}
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
        // camera at 0,0,0
        glm::vec3 ray = glm::normalize(vertices[i].pos);
        float flat_t = -1.0f / ray.z;

        scene_ptr->add_object(new Rect(id, scene_ptr, .05f, .05f, ray.x*flat_t, ray.y * flat_t, {1.0f, 1.0f,1.0f}, Plane::id, i));
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

void Plane::on_move(glm::vec4 ray_world) {
    float t = -1 / ray_world.z;

    float delta_pos_x = ray_world.x * t - Plane::pos_x;
    float delta_pos_y = ray_world.y * t - Plane::pos_y;
    
    Plane::pos_x = ray_world.x * t;
    Plane::pos_y = ray_world.y * t;

    for (auto& vertex : vertices) {
        vertex.pos.x += delta_pos_x;
        vertex.pos.y += delta_pos_y;
    }

    if (marker_ids.size() > 0) {
        auto vertices = get_vertices();
           
        // assuming vertex number same as marker number
        for (int i = 0; i < vertices.size(); i++) {
            //scene_ptr->get_object_ptr(marker_ids[i])->on_move(vertices[i].pos.x, vertices[i].pos.y);
        }
    }
}

void compute_homography_matrix(glm::mat3* H, glm::vec3* source, glm::vec3* target) {
    // Construct equations system
    const unsigned int vertex_count = 4;
    const int m = 8, n = 9;

    float a[m][n];

    for (int i = 0; i < vertex_count; i++) {
        a[2 * i][0] = 0.0f;
        a[2 * i][1] = 0.0f;
        a[2 * i][2] = 0.0f;
        a[2 * i][3] = source[i].x;
        a[2 * i][4] = source[i].y;
        a[2 * i][5] = -1.0f;
        a[2 * i][6] = -source[i].x * -target[i].y;
        a[2 * i][7] = -source[i].y * -target[i].y;
        a[2 * i][8] = target[i].y;
        
        a[2 * i + 1][0] = source[i].x;
        a[2 * i + 1][1] = source[i].y;
        a[2 * i + 1][2] = -1.0f;
        a[2 * i + 1][3] = 0.0f;
        a[2 * i + 1][4] = 0.0f;
        a[2 * i + 1][5] = 0.0f;
        a[2 * i + 1][6] = -source[i].x * -target[i].x;
        a[2 * i + 1][7] = -source[i].y * -target[i].x;
        a[2 * i + 1][8] = target[i].x;
    }

    // Solving using Gaussian Elimination

    // 1. Forward elimination

    // Wikipedia version
    // h: = 1 /* Initialization of the pivot row */
    // k : = 1 /* Initialization of the pivot column */

    // while h ≤ m and k ≤ n
    //    /* Find the k-th pivot: */
    //     i_max : = argmax(i = h ... m, abs(A[i, k]))
    //     if A[i_max, k] = 0
    //         /* No pivot in this column, pass to next column */
    //         k : = k + 1
    //     else
    //         swap rows(h, i_max)
    //         /* Do for all rows below pivot: */
    //         for i = h + 1 ... m:
    //             f: = A[i, k] / A[h, k]
    //             /* Fill with zeros the lower part of pivot column: */
    //             A[i, k] : = 0
    //             /* Do for all remaining elements in current row: */
    //             for j = k + 1 ... n :
    //                 A[i, j] : = A[i, j] - A[h, j] * f
    //                 /* Increase pivot row and column */
    //         h : = h + 1
    //         k : = k + 1
    
    int h = 0, k = 0;

    while (h < m && k < n) {
        // Find the k-th pivot
        int i_max = 0;
        float a_max = 0.0f;

        for (int i = h; i < m; i++) {
            if (fabs(a[i][k]) > a_max) {
                a_max = fabs(a[i][k]);
                i_max = i;
            }
        }

        if (a[i_max][k] == 0.0f) {
            // No pivot in this column, pass to next column
            k++;
        }
        else {
            // Swap rows h, i_max
            for (int i = 0; i < n; i++) {
                float temp = a[h][i];
                a[h][i] = a[i_max][i];
                a[i_max][i] = temp;
            }
            
            float a_hk = a[h][k];
            // Divide each entry in row i by A[h,k]
            for (int i = 0; i < n; i++) {
                a[h][i] /= a_hk;
            }
            
            // Now A[h,k] will have the value 1.
            for (int i = h + 1; i < m; i++) {
                //subtract A[i,k] * row h from row i
                float a_ik = a[i][k];
                for (int j = 0; j < n; j++) {
                    a[i][j] -= a_ik * a[h][j];
                }
                // Now A[i,k] will be 0, since A[i,k] - A[i,k] * A[i,k] = A[i,k] - 1 * A[i,k] = 0.
            }
            
            h++;
            k++;
        }
    }
    
    // Print matrix
    /*
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j++) {
            std::cout << a[i][j] << "\t";
        }
        std::cout << std::endl;
    }
    */

    // 2. Back substitution
    // 1st attempt
    /*
    for (int i = m - 1; i >= 0; i--) {
        float sum = 0.0f;

        //std::cout << "i=" << i << std::endl;

        // sum previous
        for (int j = i; j < m - 1; j++) {
            //std::cout << "j=" << j;
            sum += a[i][i - j] * H[j];
        }

        //if (a[i][i] == 0.0f) a[i][i] = 1.0f;

        H[i] = (a[i][8] - sum) / a[i][i];
        
        //std::cout << std::endl;
    }
    */

    // Github version
    for (int i = m - 2; i >= 0; i--) {
        for (int j = i + 1; j < n - 1; j++) {
            a[i][m] -= a[i][j] * a[j][m];
        }
    }

    // Copy solutions
    *H = {
        { a[0][8], a[1][8], a[2][8] },
        { a[3][8], a[4][8], a[5][8] },
        { a[6][8], a[7][8], 1.0f    }
    };
}

void Plane::move_vertex(unsigned int vertex_id, glm::vec4 ray_world) {
    const int vertices_count = 4;

    glm::vec3 source[4] = {
        { -width / 2, -height / 2, -1.0f },
        { width / 2, -height / 2, -1.0f },
        { width / 2, height / 2, -1.0f },
        { -width / 2, height / 2, -1.0f },
    };

    glm::vec3 target[4];
    for (auto marker_id : marker_ids) {
        Rect *rect_ptr = dynamic_cast<Rect*>(scene_ptr->get_object_ptr(marker_id));
        glm::vec2 position = rect_ptr->get_position();
        uint16_t vertex_id = rect_ptr->get_vertex_id();
        target[vertex_id].x = position.x;
        target[vertex_id].y = position.y;
        target[vertex_id].z = -1.0f;
    }

    glm::mat3 H;

    compute_homography_matrix(&H, source, target);

    // Apply transformatin to vertices
    for (int i = 0; i < vertices_count; i++) {
        vertices[i].pos = source[i] * H;
    }
}
