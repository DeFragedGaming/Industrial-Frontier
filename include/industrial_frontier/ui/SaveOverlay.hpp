#pragma once

#include "industrial_frontier/save/SaveManager.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include <string>
#include <vector>
#include <utility>

namespace industrial_frontier
{
    enum class SaveUiAction
    {
        None,
        OpenSave,
        OpenLoad,
        Continue,
        SaveNamed,
        LoadSlot,
        DeleteSlot,
        Close
    };

    struct SaveUiResult
    {
        SaveUiAction action = SaveUiAction::None;
        std::string slotId;
        std::string name;
        int backupIndex = 0;

        SaveUiResult(SaveUiAction type = SaveUiAction::None,
                     std::string id = {}, std::string slotName = {}, int backup = 0)
            : action(type), slotId(std::move(id)),
              name(std::move(slotName)), backupIndex(backup) {}
    };

    class SaveOverlay
    {
    public:
        bool Initialize();
        void Shutdown();
        void OpenSave();
        void OpenLoad();
        void Close();
        bool IsOpen() const;
        bool IsNaming() const;
        void DismissDeleteConfirmation();
        void TextInput(const char* text);
        SaveUiResult KeyDown(SDL_Scancode key);
        SaveUiResult Click(int width, int height, float x, float y,
                           bool menuAvailable, bool canSave, bool canContinue,
                           const std::vector<SaveSlotInfo>& slots);
        void Render(int width, int height, bool menuAvailable,
                    bool canSave, bool canContinue,
                    const std::vector<SaveSlotInfo>& slots);
        void SetMessage(std::string message);

    private:
        enum class Mode { Closed, Save, Load };
        Mode m_mode = Mode::Closed;
        std::string m_name;
        std::string m_message;
        int m_page = 0;
        bool m_confirmDelete = false;
        std::string m_deleteSlotId;
        std::string m_deleteSlotName;

        GLuint m_program = 0;
        GLuint m_vertexArray = 0;
        GLuint m_vertexBuffer = 0;
        std::vector<float> m_vertices;

        bool SetupGL();
        void AddRect(int width, int height, float x, float y,
                     float w, float h, float r, float g, float b, float a);
        void AddText(int width, int height, float x, float y,
                     const std::string& value, int scale,
                     float r, float g, float b, int maxChars = 60);
        void DrawButton(int width, int height, float x, float y,
                        float w, float h, const std::string& label,
                        bool enabled);
        bool Inside(float x, float y, float left, float top,
                    float width, float height) const;
    };
}
