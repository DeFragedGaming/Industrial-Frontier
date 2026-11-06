#include "industrial_frontier/world/TerrainGenerationWorker.hpp"
#include "industrial_frontier/world/VoxelTerrain.hpp"
#include "industrial_frontier/world/VoxelMesher.hpp"

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
            m_voxelTerrain = std::make_shared<VoxelTerrain>(m_settings, m_chunkSize, m_vertexSpacing);
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
            m_voxelTerrain.reset();
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
        QueueChunk(chunkX, chunkZ, false);
    }

    void TerrainGenerationWorker::RegenerateChunk(int chunkX, int chunkZ)
    {
        QueueChunk(chunkX, chunkZ, true);
    }

    void TerrainGenerationWorker::QueueChunk(int chunkX, int chunkZ, bool force)
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_running || m_stopping || !m_voxelTerrain)
                return;
            const std::uint64_t key = MakeChunkKey(chunkX, chunkZ);
            if (force)
            {
                m_requested.erase(key);
                m_completed.erase(
                    std::remove_if(m_completed.begin(), m_completed.end(),
                        [chunkX, chunkZ](const TerrainChunk& chunk)
                        {
                            return chunk.GetChunkX() == chunkX &&
                                   chunk.GetChunkZ() == chunkZ;
                        }),
                    m_completed.end());
            }
            if (!m_requested.insert(key).second)
                return;
            const int dx = chunkX - m_centerX;
            const int dz = chunkZ - m_centerZ;
            m_requests.push({chunkX, chunkZ, dx * dx + dz * dz,
                             key, m_voxelTerrain->GetRevision()});
        }
        m_condition.notify_one();
    }

    float TerrainGenerationWorker::SampleDensity(float x, float y, float z) const
    {
        std::shared_ptr<VoxelTerrain> terrain;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            terrain = m_voxelTerrain;
        }
        return terrain ? terrain->SampleDensity(x, y, z) : -1.0f;
    }

    std::shared_ptr<const VoxelTerrain>
    TerrainGenerationWorker::GetVoxelTerrain() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_voxelTerrain;
    }

    bool TerrainGenerationWorker::DigSphere(float x, float y, float z, float radius)
    {
        std::shared_ptr<VoxelTerrain> terrain;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            terrain = m_voxelTerrain;
        }
        if (!terrain || !terrain->DigSphere(x, y, z, radius))
            return false;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_requests = {};
            m_requested.clear();
            m_completed.clear();
        }
        return true;
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
        std::shared_ptr<VoxelTerrain> terrain;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            terrain = m_voxelTerrain;
        }
        VoxelMesherSettings mesherSettings{};
        mesherSettings.spacing = 2.0f;
        VoxelMesher mesher(mesherSettings);

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

                if (request.revision != terrain->GetRevision())
                    continue;

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

            VoxelMesh mesh;
            const bool generated =
                mesher.GenerateChunk(*terrain, request.x, request.z, mesh) &&
                chunk.SetVoxelMesh(std::move(mesh));

            {
                std::lock_guard<std::mutex> lock(m_mutex);

                if (request.revision != terrain->GetRevision())
                    continue;

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