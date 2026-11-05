
#include "industrial_frontier/core/GameFlow.hpp"

#include <algorithm>

namespace industrial_frontier
{
    GameFlow::GameFlow() = default;

    void GameFlow::Update(float deltaTime)
    {
        if (m_state != GameState::NewGameIntro)
        {
            return;
        }

        if (deltaTime <= 0.0f)
        {
            return;
        }

        m_storySceneElapsed +=
            std::min(deltaTime, 0.1f);

        if (m_storySceneElapsed >=
            GetCurrentStoryScene().durationSeconds)
        {
            AdvanceIntroduction();
        }
    }

    void GameFlow::StartNewGame()
    {
        m_storySceneIndex = 0;
        m_storySceneElapsed = 0.0f;
        m_state = GameState::NewGameIntro;
    }

    void GameFlow::SkipIntroduction()
    {
        if (m_state == GameState::NewGameIntro)
        {
            m_state = GameState::Playing;
        }
    }

    void GameFlow::AdvanceIntroduction()
    {
        if (m_state != GameState::NewGameIntro)
        {
            return;
        }

        ++m_storySceneIndex;
        m_storySceneElapsed = 0.0f;

        if (m_storySceneIndex >= StoryScenes.size())
        {
            m_storySceneIndex = StoryScenes.size() - 1;
            m_state = GameState::Playing;
        }
    }

    void GameFlow::PauseGame()
    {
        if (m_state == GameState::Playing)
        {
            m_state = GameState::Paused;
        }
    }

    void GameFlow::ResumeGame()
    {
        if (m_state == GameState::Paused)
        {
            m_state = GameState::Playing;
        }
    }

    void GameFlow::ReturnToMainMenu()
    {
        m_state = GameState::MainMenu;
        m_storySceneIndex = 0;
        m_storySceneElapsed = 0.0f;
    }

    void GameFlow::OpenSettings()
    {
        if (m_state == GameState::MainMenu ||
            m_state == GameState::Paused)
        {
            m_previousState = m_state;
            m_state = GameState::Settings;
        }
    }

    void GameFlow::OpenCredits()
    {
        if (m_state == GameState::MainMenu)
        {
            m_previousState = m_state;
            m_state = GameState::Credits;
        }
    }

    void GameFlow::CloseSubmenu()
    {
        if (m_state == GameState::Settings ||
            m_state == GameState::Credits)
        {
            m_state = m_previousState;
        }
    }

    GameState GameFlow::GetState() const
    {
        return m_state;
    }

    bool GameFlow::IsPlaying() const
    {
        return m_state == GameState::Playing;
    }

    bool GameFlow::IsIntroductionActive() const
    {
        return m_state == GameState::NewGameIntro;
    }

    bool GameFlow::IsMenuVisible() const
    {
        return m_state == GameState::MainMenu ||
               m_state == GameState::Paused ||
               m_state == GameState::Settings ||
               m_state == GameState::Credits;
    }

    std::size_t GameFlow::GetStorySceneIndex() const
    {
        return m_storySceneIndex;
    }

    std::size_t GameFlow::GetStorySceneCount() const
    {
        return StoryScenes.size();
    }

    const StoryScene& GameFlow::GetCurrentStoryScene() const
    {
        return StoryScenes[m_storySceneIndex];
    }

    float GameFlow::GetStorySceneProgress() const
    {
        if (m_state != GameState::NewGameIntro)
        {
            return 0.0f;
        }

        const float duration =
            GetCurrentStoryScene().durationSeconds;

        if (duration <= 0.0f)
        {
            return 1.0f;
        }

        return std::clamp(
            m_storySceneElapsed / duration,
            0.0f,
            1.0f
        );
    }

    void GameFlow::SetCompanyName(const std::string& name)
    {
        if (!name.empty())
        {
            m_companyName = name;
        }
    }

    const std::string& GameFlow::GetCompanyName() const
    {
        return m_companyName;
    }
}
