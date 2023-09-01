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

std::string UI::open_file_dialog() {
    nfdchar_t* outPath = NULL;
    nfdchar_t* filter_list = (nfdchar_t*)"png, jpg";
    nfdresult_t result = NFD_OpenDialog(filter_list, NULL, &outPath);
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
    UI::pScene = scene;
    UI::window = window;
}

void UI::draw_ui() {
    draw_menu_bar();

    // set fullscreen window
    static ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);

    ImGui::Begin("App", nullptr, flags);

    ImGui::BeginTable("table1", 2);
    // set first first column width
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 250.0f);

    ImGui::TableNextRow();

    // first column
    ImGui::TableSetColumnIndex(0);
    UI::menu();

    // second column
    ImGui::TableSetColumnIndex(1);
    
    // viewport
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();

    // render viewport
    VkDescriptorSet viewportSet = pScene->getEnginePointer()->renderViewport(viewportPanelSize.x, viewportPanelSize.y, io.MousePos.x - pos.x, io.MousePos.y - pos.y);

    ImGui::Image(viewportSet, ImVec2(viewportPanelSize.x, viewportPanelSize.y));
    ImGui::Text("Mouse pos: (%g, %g)", io.MousePos.x - pos.x, io.MousePos.y - pos.y);

    ImGui::EndTable();

    ImGui::End();

    if (show_demo_window) {
        ImGui::ShowDemoWindow();
    }
}

void UI::menu() {
    ImGui::BeginGroup();

    ImGui::BeginChild("item view", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 2)); // Leave room for 1 line below us

    ImGui::SeparatorText("Debug");

    ImGui::Text("Selected: %i", pScene->getSelectedObjectId());
    ImGui::Text("Hovering: %i", pScene->getHoveringObjectId());
    ImGui::Text("Dragging: %i", pScene->getDragginObjectId());

    
    ImGuiIO& io = ImGui::GetIO();
    ImGui::Text("Mouse pos: (%g, %g)", io.MousePos.x, io.MousePos.y);

    ImGui::SeparatorText("Planes");

    bool closable_group = true;

    for (uint8_t object_id : pScene->getIds()) {
        Object* object_ptr = pScene->getObjectPointer(object_id);

        if (object_ptr == nullptr) {
            continue;
        }

        Plane* plane_ptr = dynamic_cast<Plane*>(object_ptr);

        if (plane_ptr != nullptr) {
            std::string name = "Plane " + std::to_string(plane_ptr->getId());
            if (ImGui::CollapsingHeader(name.c_str())) {
                ImGui::SeparatorText("2D vertices");
                auto plane_vertices = plane_ptr->getVertices();

                for (auto vertex : plane_vertices) {
                    auto vertex_normal = glm::normalize(vertex.pos);
                    float flat_t = -1 / vertex_normal.z;

                    ImGui::Text("x: %.3f \t y: %.3f", vertex_normal.x * flat_t, vertex_normal.y * flat_t);
                }

                ImGui::SeparatorText("Image");

                std::string button_label = plane_ptr->get_image_path();
                if (button_label.size() == 0) {
                    button_label = "Load";
                }

                if (ImGui::Button(button_label.c_str())) {
                    std::string image_path = open_file_dialog();
                    if (!image_path.empty()) {
                        plane_ptr->set_image_path(image_path);
                    }
                }

                ImGui::SeparatorText("Functions");
                if (ImGui::Button("Remove")) {
                    pScene->removeObject(plane_ptr->getId());
                }

                ImGui::Separator();
                ImGui::Spacing();
            }
        }
    }

    if (ImGui::Button("Add")) {
        // TO DO: open file dialog 
        pScene->addObject(new Plane(pScene, .3f, .3f, 0.0f, 0.0f));
    }

    ImGui::EndChild();

    ImGui::SeparatorText("Utility");
    ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state

    ImGui::EndGroup();
}
