#include "../include/scene_objects.h"
#include "../include/clamp.h"

#include <memory>
#include <iostream>

// Rect
Marker::Marker(Scene* scene_ptr, float pos_x, float pos_y, glm::vec3 color, uint8_t parent_id, uint16_t vertex_id) {
    Marker::pos_x = pos_x;
    Marker::pos_y = pos_y;
    Marker::color = color;
    Marker::vertex_id = vertex_id;
    Marker::parent_id = parent_id;
    Marker::scene_ptr = scene_ptr;
}

std::vector<Vertex> Marker::getVertices() {
    glm::vec3 finalColor = color;
    
    if (highlighted) {
        finalColor.r += .2f;
        finalColor.g += .2f;
        finalColor.b += .2f;
    }

    return {
        {{ -Marker::dimension / 2 + Marker::pos_x, -Marker::dimension / 2 + Marker::pos_y, -1.0f }, finalColor, {1.0f, 0.0f}},
        {{ Marker::dimension / 2 + Marker::pos_x, -Marker::dimension / 2 + Marker::pos_y, -1.0f}, finalColor, {0.0f, 0.0f}},
        {{ Marker::dimension / 2 + Marker::pos_x, Marker::dimension / 2 + Marker::pos_y, -1.0f}, finalColor, {0.0f, 1.0f}},
        {{ -Marker::dimension / 2 + Marker::pos_x, Marker::dimension / 2 + Marker::pos_y, -1.0f}, finalColor, {1.0f, 1.0f}}
    };
}

std::vector<uint16_t> Marker::getIndices() {
    return {
        0, 1, 2, 2, 3, 0
    };
}

void Marker::hoveringStart() {
}

void Marker::hoveringStop() {
}

void Marker::onSelect() {
    highlighted = true;
}

void Marker::onRelease() {
    highlighted = false;
}

glm::vec2 Marker::get_position() {
    return { Marker::pos_x, Marker::pos_y };
}

uint16_t Marker::get_vertex_id() {
    return Marker::vertex_id;
}

void Marker::onMove(float deltaX, float deltaY) {
    Marker::pos_x += deltaX;
    Marker::pos_y += deltaY;

    Plane* parent = dynamic_cast<Plane*>(scene_ptr->getObjectPointer(parent_id));
    
    if (parent != nullptr)
        parent->moveVertex(vertex_id);
}

// Plane
Plane::Plane(Scene* scene_ptr, float width, float height, float pos_x, float pos_y) {
    const glm::vec3 defaultColor = { 0.5f, 0.5f, 0.5f };
    
    Plane::width = width;
    Plane::height = height;
    
    Plane::vertices = {
        {{ -width / 2 + pos_x, -height / 2 + pos_y, -1.0f }, defaultColor, {0.0f, 1.0f}},
        {{ width / 2 + pos_x, -height / 2 + pos_y, -1.0f}, defaultColor, {1.0f, 1.0f}},
        {{ width / 2 + pos_x, height / 2 + pos_y, -1.0f}, defaultColor, {1.0f, 0.0f}},
        {{ -width / 2 + pos_x, height / 2 + pos_y, -1.0f}, defaultColor, {0.0f, 0.0f}}
    };

    Plane::pos_x = pos_x;
    Plane::pos_y = pos_y;
    Plane::pScene = scene_ptr;
}

void Plane::beforeRemove() {
    onRelease();
}

std::vector<Vertex> Plane::getVertices() {
    return vertices;
}

std::vector<uint16_t> Plane::getIndices() {
    return {
        0, 1, 2, 2, 3, 0
    };
}

std::string Plane::getPipelineName() {
    if (mediaId != -1) {
        Media* pMedia = pScene->getEngine()->getMediaManager()->getMediaById(mediaId);
        if (pMedia->type == MediaType::IMAGE) { return "texture"; }
        else if (pMedia->type == MediaType::VIDEO) { return "video_frame"; }
        else return "color";
    }
    else return "color";
}

void Plane::setMediaId(uint8_t id) {
    Plane::mediaId = id;
}

void Plane::hoveringStart() {
}

void Plane::hoveringStop() {
}

void Plane::onSelect() {
    for (auto& vertex : vertices) {
        vertex.color.r += .1f;
        vertex.color.g += .1f;
        vertex.color.b += .1f;
    }

    // add marker
    for (int i = 0; i < vertices.size(); i++) {
        // camera at 0,0,0
        glm::vec3 ray = glm::normalize(vertices[i].pos);
        float flat_t = -1.0f / ray.z;
        markerIds.push_back(pScene->addObject(new Marker(pScene, ray.x * flat_t, ray.y * flat_t, { 1.0f, 1.0f,1.0f }, Plane::getId(), i)));
    }

    // add lines
    for (int i = 0; i < 4; i++) {
        glm::vec3 firstPointRay = glm::normalize(vertices[i].pos);
        float firstPointT = -1.0f / firstPointRay.z;

        glm::vec3 secondPointRay = glm::normalize(vertices[(i + 1) % 4].pos);
        float secondPointT = -1.0f / secondPointRay.z;
        lineIds.push_back(pScene->addObject(new Line({ firstPointRay.x * firstPointT , firstPointRay.y * firstPointT }, { secondPointRay.x * secondPointT , secondPointRay.y * secondPointT })));
    }
}

void Plane::onRelease() {
    for (auto& vertex : vertices) {
        vertex.color.r -= .1f;
        vertex.color.g -= .1f;
        vertex.color.b -= .1f;
    }
    
    // remove markers
    for (auto marker_id : markerIds) {
        pScene->removeObject(marker_id);
    }
    markerIds.clear();

    // remove lines
    for (auto lineId : lineIds) {
        pScene->removeObject(lineId);
    }
    lineIds.clear();
}

void Plane::onMove(float deltaX, float deltaY) {
    for (auto markerId : markerIds) {
        Marker* pMarker = dynamic_cast<Marker*>(pScene->getObjectPointer(markerId));

        if (pMarker == nullptr) break;

        pMarker->onMove(deltaX, deltaY);
    }
}

void computeHomographyMatrix(glm::mat3* H, glm::vec3* source, glm::vec3* target) {
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

void Plane::moveVertex(unsigned int vertex_id) {
    const int vertices_count = 4;

    glm::vec3 source[4] = {
        { -width / 2, -height / 2, -1.0f },
        { width / 2, -height / 2, -1.0f },
        { width / 2, height / 2, -1.0f },
        { -width / 2, height / 2, -1.0f },
    };

    glm::vec3 target[4];
    for (auto marker_id : markerIds) {
        Marker *rect_ptr = dynamic_cast<Marker*>(pScene->getObjectPointer(marker_id));
        glm::vec2 position = rect_ptr->get_position();
        uint16_t vertex_id = rect_ptr->get_vertex_id();
        target[vertex_id].x = position.x;
        target[vertex_id].y = position.y;
        target[vertex_id].z = -1.0f;
    }

    glm::mat3 H;

    computeHomographyMatrix(&H, source, target);

    // Apply transformatin to vertices
    for (int i = 0; i < vertices_count; i++) {
        vertices[i].pos = source[i] * H;
    }

    // move line
    for (int i = 0; i < 4; i++) {
        glm::vec3 firstPointRay = glm::normalize(vertices[i].pos);
        float firstPointT = -1.0f / firstPointRay.z;

        glm::vec3 secondPointRay = glm::normalize(vertices[(i + 1) % 4].pos);
        float secondPointT = -1.0f / secondPointRay.z;

        Line* pLine = dynamic_cast<Line*>(pScene->getObjectPointer(lineIds[i]));
        pLine->moveVertices({ firstPointRay.x * firstPointT , firstPointRay.y * firstPointT }, { secondPointRay.x * secondPointT , secondPointRay.y * secondPointT });
    }
}

Line::Line(glm::vec2 firstPoint, glm::vec2 secondPoint) {
    vertices = {
        Vertex{{ firstPoint.x, firstPoint.y, -1.0f }, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f} },
        Vertex{{ secondPoint.x, secondPoint.y, -1.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f} },
    };
}

std::vector<Vertex> Line::getVertices() {
    return vertices;
}

std::vector<uint16_t> Line::getIndices() {
    return { 0, 1 };
}

void Line::moveVertices(glm::vec2 firstPoint, glm::vec2 secondPoint) {
    vertices = {
        Vertex{{ firstPoint.x, firstPoint.y, -1.0f }, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f} },
        Vertex{{ secondPoint.x, secondPoint.y, -1.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f} },
    };
}
