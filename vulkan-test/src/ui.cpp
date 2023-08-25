#include "../include/ui.h"

#include <imgui.h>
#include <iostream>
#include <string>
#include <nfd.h>

void UI::draw_menu_bar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Exit")) {
                glfwSetWindowShouldClose(window, GL_TRUE);
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

// TO DO: filter file extensions
std::string UI::open_file_dialog() {
    nfdchar_t* outPath = NULL;
    nfdresult_t result = NFD_OpenDialog(NULL, NULL, &outPath);
    std::string path;

    if (result == NFD_OKAY) {
        path.append(outPath);

        free(outPath);
    }
    else if (result == NFD_CANCEL) {
        puts("User pressed cancel.");
    }
    else {
        printf("Error: %s\n", NFD_GetError());
    }

    return path;
}

UI::UI(Scene* scene, GLFWwindow* window) {
    UI::scene = scene;
    UI::window = window;
}

void UI::draw_ui() {
    draw_menu_bar();

    draw_function_window();

    if (show_demo_window) {
        ImGui::ShowDemoWindow();
    }
}

void UI::draw_function_window() {
    int selected_obj_id = scene->get_selected_obj_id();
    
    ImGui::Begin("App bar");
    
    ImGui::BeginGroup();
    
    ImGui::BeginChild("item view", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()*2)); // Leave room for 1 line below us

    ImGui::SeparatorText("Debug");

    ImGui::Text("Selected: %i", selected_obj_id);
    ImGui::Text("Hovering: %i", scene->get_hovering_obj_id());

    ImGui::SeparatorText("Planes");

    bool closable_group = true;

    for (uint8_t object_id: scene->get_ids()) {
        Object* object_ptr = scene->get_object_ptr(object_id);
        
        if (object_ptr == nullptr) {
            continue;
        }

        Plane* plane_ptr = dynamic_cast<Plane*>(object_ptr);
        
        if (plane_ptr != nullptr) {
            std::string name = "Plane " + std::to_string(plane_ptr->get_id());
            if (ImGui::CollapsingHeader(name.c_str())) {
                ImGui::SeparatorText("2D vertices");
                auto plane_vertices = plane_ptr->get_vertices();

                for (auto vertex : plane_vertices) {
                    auto vertex_normal = glm::normalize(vertex.pos);
                    float flat_t = -1 / vertex_normal.z;

                    ImGui::Text("x: %.3f \t y: %.3f", vertex_normal.x*flat_t, vertex_normal.y * flat_t);
                }

                ImGui::SeparatorText("Image");

                if (ImGui::Button(plane_ptr->get_image_path().c_str())) {
                    std::string image_path = open_file_dialog();
                    if (!image_path.empty()) {
                        plane_ptr->set_image_path(image_path);
                    }
                }

                ImGui::SeparatorText("Functions");
                if (ImGui::Button("Remove")) {
                    scene->remove_object(plane_ptr->get_id());
                }

                ImGui::Separator();
                ImGui::Spacing();
            }
        }    
    }

    if (ImGui::Button("Add")) {
        // TO DO: open file dialog 
        scene->add_object(new Plane(scene, .3f, .3f, 0.0f, 0.0f));
    }

    ImGui::EndChild();

    ImGui::SeparatorText("Utility");
    ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state

    ImGui::EndGroup();

    ImGui::End();
}
