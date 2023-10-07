#include "../include/ui.h"

#include <imgui.h>
#include <iostream>
#include <string>
#include <nfd.h>

void UI::drawTopBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Exit")) {
                //glfwSetWindowShouldClose(window, GL_TRUE);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Output")) {
            bool showOutput = pEngine->getShowOutput();
            if (ImGui::MenuItem("Show output window", nullptr, showOutput)) {
                pEngine->setShowOutput(!showOutput);
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

void UI::drawMediaManager() {
    MediaManager* pMediaManager = pEngine->getMediaManager();
    std::vector<Media> medias = pMediaManager->getMedias();

    Scene* pScene = pEngine->getScene();

    int selectedObjId = pScene->getSelectedObjectId();
    Plane* pSelectedPlane = nullptr;
    if (selectedObjId != -1) {
        pSelectedPlane = dynamic_cast<Plane*>(pScene->getObjectPointer(selectedObjId));
    }

    for (auto media : medias) {
        bool selected = false;

        if (pSelectedPlane != nullptr) {
            if (pSelectedPlane->getImageId() == media.id) {
                selected = true;
            }
        }

        if (ImGui::Selectable(media.filePath.substr(media.filePath.find_last_of("\\") + 1).c_str(), selected, 0, ImVec2{ ImGui::GetContentRegionAvail().x , 40 })) {
            if (pSelectedPlane != nullptr) {
                pSelectedPlane->setImageId(media.id);
            }
        }
    }
    
    if (ImGui::Button("Add", ImVec2{ ImGui::GetContentRegionAvail().x , 40 })) {
        std::string image_path = openFileDialog();
        if (!image_path.empty()) {
            pMediaManager->loadImage(image_path);
        }
    }
}

void UI::viewport() {
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();

    // render viewport
    VkDescriptorSet viewportSet = pEngine->renderViewport(viewportPanelSize.x, viewportPanelSize.y, io.MousePos.x - pos.x, io.MousePos.y - pos.y);
    ImGui::Image(viewportSet, ImVec2(viewportPanelSize.x, viewportPanelSize.y));
}

std::string UI::openFileDialog() {
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

UI::UI(VulkanEngine* pEngine) {
    UI::pEngine = pEngine;
}

void UI::drawUi() {
    drawTopBar();

    // set fullscreen window
    static ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);

    ImGui::Begin("App", nullptr, flags);

    ImGui::BeginTable("table1", 3);
    // set first first column width
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 250.0f);
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 250.0f);

    ImGui::TableNextRow();

    // first column - planes menu
    ImGui::TableSetColumnIndex(0);
    planesMenu();

    // second column - viewport
    ImGui::TableSetColumnIndex(1);
    viewport();

    // third column - media manager
    ImGui::TableSetColumnIndex(2);
    drawMediaManager();

    ImGui::EndTable();

    ImGui::End();

    if (show_demo_window) {
        ImGui::ShowDemoWindow();
    }
}

void UI::planesMenu() {
    Scene* pScene = pEngine->getScene();

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
                /*
                ImGui::SeparatorText("Image");

                std::string button_label = plane_ptr->getImageId();
                if (button_label.size() == 0) {
                    button_label = "Load";
                }

                if (ImGui::Button(button_label.c_str())) {
                    std::string image_path = openFileDialog();
                    if (!image_path.empty()) {
                        plane_ptr->setImageId(image_path);
                    }
                }
                */

                ImGui::SeparatorText("Functions");
                if (ImGui::Button("Remove")) {
                    pScene->removeObject(plane_ptr->getId());
                }

                ImGui::Separator();
                ImGui::Spacing();
            }
        }
    }

    if (ImGui::Button("Add", ImVec2{ ImGui::GetContentRegionAvail().x , 20 })) {
        // TO DO: open file dialog 
        pScene->addObject(new Plane(pScene, .3f, .3f, 0.0f, 0.0f));
    }

    ImGui::EndChild();

    ImGui::SeparatorText("Utility");
    ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state

    ImGui::EndGroup();
}
