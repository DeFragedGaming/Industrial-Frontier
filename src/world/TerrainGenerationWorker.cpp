#include "industrial_frontier/world/TerrainGenerationWorker.hpp"

#include <algorithm>
#include <utility>
#include <vector>

namespace industrial_frontier
{
    TerrainGenerationWorker::TerrainGenerationWorker() = default;

    TerrainGenerationWorker::~TerrainGenerationWorker()
    {
        Stop();
    }

    std::uint64_t TerrainGenerationWorker::MakeChunkKey(int x, int z)
    {
        return
            (static_cast<std::uint64_t>(
                static_cast<std::uint32_t>(x)
            ) << 32) |
            static_cast<std::uint32_t>(z);
    }

    void TerrainGenerationWorker::Start(
        const TerrainSettings& settings,
        float chunkSize,
        float vertexSpacing)
    {
        Stop();

        {
            std::lock_guard<std::mutex> lock(m_mutex);

            m_settings = settings;
            m_chunkSize = std::max(chunkSize, 1.0f);
            m_vertexSpacing = std::max(vertexSpacing, 0.1f);

            m_stopping = false;
            m_running = true;

            m_centerX = 0;
            m_centerZ = 0;
        }

        m_thread = std::thread(
            &TerrainGenerationWorker::WorkerLoop,
            this
        );
    }

    void TerrainGenerationWorker::Stop()
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            if (!m_running && !m_thread.joinable())
            {
                return;
            }

            m_stopping = true;
        }

        m_condition.notify_all();

        if (m_thread.joinable())
        {
            m_thread.join();
        }

        {
            std::lock_guard<std::mutex> lock(m_mutex);

            m_running = false;
            m_stopping = false;

            m_requests = {};
            m_completed.clear();
            m_requested.clear();
        }
    }

    void TerrainGenerationWorker::SetCenter(int chunkX, int chunkZ)
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            m_centerX = chunkX;
            m_centerZ = chunkZ;
        }

        m_condition.notify_one();
    }

    void TerrainGenerationWorker::RequestChunk(int chunkX, int chunkZ)
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            if (!m_running || m_stopping)
            {
                return;
            }

            const std::uint64_t key = MakeChunkKey(chunkX, chunkZ);

            if (!m_requested.insert(key).second)
            {
                return;
            }

            const int dx = chunkX - m_centerX;
            const int dz = chunkZ - m_centerZ;

            m_requests.push({
                chunkX,
                chunkZ,
                dx * dx + dz * dz,
                key
            });
        }

        m_condition.notify_one();
    }

    bool TerrainGenerationWorker::TryGetCompletedChunk(TerrainChunk& chunk)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_completed.empty())
        {
            return false;
        }

        chunk = std::move(m_completed.front());
        m_completed.pop_front();

        m_requested.erase(
            MakeChunkKey(chunk.GetChunkX(), chunk.GetChunkZ())
        );

        return true;
    }

    bool TerrainGenerationWorker::IsRunning() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_running && !m_stopping;
    }

    void TerrainGenerationWorker::WorkerLoop()
    {
        TerrainGenerator generator(m_settings);

        while (true)
        {
            Request request{};

            {
                std::unique_lock<std::mutex> lock(m_mutex);

                m_condition.wait(
                    lock,
                    [this]
                    {
                        return m_stopping || !m_requests.empty();
                    }
                );

                if (m_stopping)
                {
                    break;
                }

                request = m_requests.top();
                m_requests.pop();

                const int dx = request.x - m_centerX;
                const int dz = request.z - m_centerZ;

                if (std::abs(dx) > m_maxDistance ||
                    std::abs(dz) > m_maxDistance)
                {
                    m_requested.erase(request.key);
                    continue;
                }
            }

            TerrainChunk chunk(
                request.x,
                request.z,
                m_chunkSize,
                m_vertexSpacing
            );

            const bool generated = chunk.Generate(generator);

            {
                std::lock_guard<std::mutex> lock(m_mutex);

                if (!generated)
                {
                    m_requested.erase(request.key);
                    continue;
                }

                if (m_stopping)
                {
                    break;
                }

                const int dx = request.x - m_centerX;
                const int dz = request.z - m_centerZ;

                if (std::abs(dx) > m_maxDistance ||
                    std::abs(dz) > m_maxDistance)
                {
                    m_requested.erase(request.key);
                    continue;
                }

                m_completed.push_back(std::move(chunk));
            }
        }
    }
}