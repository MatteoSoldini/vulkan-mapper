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

    ImGui::BeginChild("media_manager");
    if (ImGui::Button("Add", ImVec2{ 100, 100 })) {
        std::string filePath = openFileDialog();
        if (!filePath.empty()) {

            pMediaManager->loadFile(filePath);
        }
    }

    for (auto media : medias) {
        bool selected = false;

        if (pSelectedPlane != nullptr) {
            if (pSelectedPlane->getMediaId() == media.id) {
                selected = true;
            }
        }

        ImGui::SameLine();
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::BeginChild((std::string("media_") + std::to_string(media.id)).c_str(), ImVec2(100, 100), true);
        
        std::string fullText = media.filePath.substr(media.filePath.find_last_of("\\") + 1);
        std::string buttonText = "";
        uint32_t letterCount = 0;

        for (auto letter : fullText) {
            buttonText.push_back(letter);
            letterCount = ++letterCount % 10;
            if (letterCount == 0) buttonText += '\n';
        }

        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5f, 0.5f));
        if (ImGui::Selectable(
            buttonText.c_str(),
            selected,
            0,
            ImVec2{ ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y }
        )) {
            if (pSelectedPlane != nullptr) {
                pSelectedPlane->setMediaId(media.id);
            }
        }
        ImGui::PopStyleVar();
        
        ImGui::EndChild();
        ImGui::PopStyleVar();
    }
    ImGui::EndChild();
}

void UI::drawPropertiesManager() {
    ImGui::SeparatorText("Properties manager");

    Scene* pScene = pEngine->getScene();
    int selectedObjId = pScene->getSelectedObjectId();
    Plane* pSelectedPlane = nullptr;
    if (selectedObjId != -1) {
        pSelectedPlane = dynamic_cast<Plane*>(pScene->getObjectPointer(selectedObjId));
    }

    if (pSelectedPlane == nullptr) {
        ImGui::Text("Select a media");
    }
    else {
        MediaId_t selectedMediaId = pSelectedPlane->getMediaId();
        Media* pMedia = pEngine->getMediaManager()->getMediaById(selectedMediaId);

        if (pMedia != nullptr) {
            ImGui::Text("File name: %s", pMedia->filePath.c_str());
            
            switch (pMedia->type) {
            case MediaType::VIDEO:
                ImGui::Text("Type: Video");
                drawVideoProperties(pMedia->pState);
                break;
            case MediaType::IMAGE:
                ImGui::Text("Type: Image");
                break;
            default:
                ImGui::Text("Type: Unknown");
            }
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
    nfdchar_t* filter_list = (nfdchar_t*)"png,jpg,mp4";
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

void UI::drawVideoProperties(Video* pVideo) {
    if (pVideo == nullptr) return;

    int currentFrame = (int)pVideo->currentFrame;

    if (pVideo != nullptr) {
        ImGui::SliderInt("frame",
            &currentFrame,
            0,
            (int)pVideo->framesCount
        );
    }

    if (ImGui::Button("first frame")) {
        pVideo->firstFrame();
    }
    if (pVideo->playing) {
        if (ImGui::Button("pause")) {
            pVideo->pause();
        }
    }
    else {
        if (ImGui::Button("play")) {
            pVideo->play();
        }
    }
}

UI::UI(VulkanState* pEngine) {
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

    ImGui::BeginChild("upper", ImVec2(0, -(ImGui::GetFrameHeightWithSpacing()/2 + 100)));
    {
        ImGui::BeginTable("upper_table", 3);
        // set first first column width
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 300.0f);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 300.0f);

        ImGui::TableNextRow();

        // first column - planes menu
        ImGui::TableSetColumnIndex(0);
        planesMenu();

        // second column - viewport
        ImGui::TableSetColumnIndex(1);
        viewport();

        // third column - media manager
        ImGui::TableSetColumnIndex(2);
        drawPropertiesManager();

        ImGui::EndTable();
    }
    ImGui::EndChild();

    drawMediaManager();

    ImGui::End();

    if (showImGuiDemoWindow) {
        ImGui::ShowDemoWindow(&showImGuiDemoWindow);
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
    ImGui::Checkbox("Demo Window", &showImGuiDemoWindow);      // Edit bools storing our window open/close state

    ImGui::EndGroup();
}
