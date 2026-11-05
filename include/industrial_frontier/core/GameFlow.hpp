
#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <string_view>

namespace industrial_frontier
{
    enum class GameState
    {
        MainMenu,
        NewGameIntro,
        Playing,
        Paused,
        Settings,
        Credits
    };

    struct StoryScene
    {
        std::string_view heading;
        std::string_view narrative;
        float durationSeconds;
    };

    class GameFlow
    {
    public:
        GameFlow();

        void Update(float deltaTime);

        void StartNewGame();
        void SkipIntroduction();
        void AdvanceIntroduction();

        void PauseGame();
        void ResumeGame();
        void ReturnToMainMenu();

        void OpenSettings();
        void OpenCredits();
        void CloseSubmenu();

        GameState GetState() const;

        bool IsPlaying() const;
        bool IsIntroductionActive() const;
        bool IsMenuVisible() const;

        std::size_t GetStorySceneIndex() const;
        std::size_t GetStorySceneCount() const;

        const StoryScene& GetCurrentStoryScene() const;
        float GetStorySceneProgress() const;

        void SetCompanyName(const std::string& name);
        const std::string& GetCompanyName() const;

    private:
        static constexpr std::array<StoryScene, 6> StoryScenes =
        {{
            {
                "THE EXODUS",
                "Earth was humanity's beginning. For centuries, "
                "we consumed its resources without considering "
                "what would remain.",
                8.0f
            },
            {
                "THE COLLAPSE",
                "The oceans rose. Farmland disappeared. "
                "Industrial networks failed. Eventually, "
                "Earth could no longer sustain the civilization "
                "we had built.",
                9.0f
            },
            {
                "A NEW FRONTIER",
                "Humanity reached beyond its dying home. "
                "New worlds offered survival, but most "
                "developed territories fell under the "
                "control of powerful corporations.",
                9.0f
            },
            {
                "YOUR LAST INVESTMENT",
                "You were never wealthy. You inherited "
                "no company and owned no machinery. "
                "Everything you had went toward passage "
                "to a frontier world and a small land claim.",
                10.0f
            },
            {
                "PLANETFALL",
                "The transport has departed. There are "
                "no factories waiting for you, no employees, "
                "and no established mining operation. "
                "Only an unfamiliar world and the chance "
                "to begin again.",
                10.0f
            },
            {
                "FIRST CLAIM",
                "Your company begins here. Survey the land. "
                "Discover its resources. Build the machines "
                "that will shape your future.",
                8.0f
            }
        }};

        GameState m_state = GameState::MainMenu;
        GameState m_previousState = GameState::MainMenu;

        std::size_t m_storySceneIndex = 0;
        float m_storySceneElapsed = 0.0f;

        std::string m_companyName = "Frontier Mining Co.";
    };
}
