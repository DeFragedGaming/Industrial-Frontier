#include "industrial_frontier/world/TerrainGenerationWorker.hpp"
#include "industrial_frontier/world/VoxelMesher.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace industrial_frontier
{
    TerrainGenerationWorker::TerrainGenerationWorker() = default;

    TerrainGenerationWorker::~TerrainGenerationWorker()
    {
        Stop();
    }

    std::uint64_t TerrainGenerationWorker::MakeChunkKey(int x, int z)
    {
        return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(x)) << 32) |
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
            m_centerX = 0;
            m_centerZ = 0;
            m_requests = {};
            m_completed.clear();
            m_pending.clear();
            m_voxelTerrain = std::make_shared<VoxelTerrain>(
                m_settings, m_chunkSize, m_vertexSpacing);
            m_stopping = false;
            m_running = true;
        }

        constexpr int WorkerCount = 2;
        for (int i = 0; i < WorkerCount; ++i)
        {
            m_threads.emplace_back(&TerrainGenerationWorker::WorkerLoop, this);
        }
    }

    void TerrainGenerationWorker::Stop()
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stopping = true;
        }
        m_condition.notify_all();

        for (std::thread& thread : m_threads)
        {
            if (thread.joinable())
            {
                thread.join();
            }
        }
        m_threads.clear();

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_running = false;
            m_stopping = false;
            m_requests = {};
            m_completed.clear();
            m_pending.clear();
            m_voxelTerrain.reset();
        }
    }

    void TerrainGenerationWorker::ReprioritizeLocked()
    {
        std::priority_queue<Request, std::vector<Request>, RequestCompare> reordered;

        while (!m_requests.empty())
        {
            Request request = m_requests.top();
            m_requests.pop();

            const auto pending = m_pending.find(request.key);
            if (pending == m_pending.end() || pending->second != request.ticket)
            {
                continue;
            }

            const int dx = request.x - m_centerX;
            const int dz = request.z - m_centerZ;
            if (std::abs(dx) > m_maxDistance || std::abs(dz) > m_maxDistance)
            {
                m_pending.erase(pending);
                continue;
            }

            request.distanceSquared = dx * dx + dz * dz;
            reordered.push(request);
        }

        m_requests.swap(reordered);

        for (auto iterator = m_completed.begin(); iterator != m_completed.end();)
        {
            const auto pending = m_pending.find(iterator->key);
            if (pending == m_pending.end() || pending->second != iterator->ticket ||
                std::abs(iterator->chunk.GetChunkX() - m_centerX) > m_maxDistance ||
                std::abs(iterator->chunk.GetChunkZ() - m_centerZ) > m_maxDistance)
            {
                if (pending != m_pending.end() && pending->second == iterator->ticket)
                {
                    m_pending.erase(pending);
                }
                iterator = m_completed.erase(iterator);
            }
            else
            {
                ++iterator;
            }
        }
    }

    void TerrainGenerationWorker::SetCenter(int chunkX, int chunkZ)
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (chunkX == m_centerX && chunkZ == m_centerZ)
            {
                return;
            }
            m_centerX = chunkX;
            m_centerZ = chunkZ;
            ReprioritizeLocked();
        }
        m_condition.notify_all();
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
            {
                return;
            }

            const int dx = chunkX - m_centerX;
            const int dz = chunkZ - m_centerZ;
            if (std::abs(dx) > m_maxDistance || std::abs(dz) > m_maxDistance)
            {
                return;
            }

            const std::uint64_t key = MakeChunkKey(chunkX, chunkZ);
            if (!force && m_pending.find(key) != m_pending.end())
            {
                return;
            }

            if (force)
            {
                m_completed.erase(
                    std::remove_if(m_completed.begin(), m_completed.end(),
                        [key](const Completed& completed)
                        {
                            return completed.key == key;
                        }),
                    m_completed.end());
            }

            const std::uint64_t ticket = ++m_nextTicket;
            m_pending[key] = ticket;
            m_requests.push({chunkX, chunkZ, dx * dx + dz * dz,
                key, ticket, force});
            if (force)
            {
                ReprioritizeLocked();
            }
        }
        m_condition.notify_one();
    }

    bool TerrainGenerationWorker::DigSphere(
        float x, float y, float z, float radius)
    {
        std::shared_ptr<VoxelTerrain> terrain;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            terrain = m_voxelTerrain;
        }
        return terrain && terrain->DigSphere(x, y, z, radius);
    }

    bool TerrainGenerationWorker::ReplaceEdits(const std::vector<VoxelEdit>& edits)
    {
        std::shared_ptr<VoxelTerrain> terrain;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            terrain = m_voxelTerrain;
        }
        if (!terrain)
        {
            return false;
        }
        terrain->SetEdits(edits);
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_requests = {};
            m_completed.clear();
            m_pending.clear();
        }
        return true;
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

    std::shared_ptr<const VoxelTerrain> TerrainGenerationWorker::GetVoxelTerrain() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_voxelTerrain;
    }

    bool TerrainGenerationWorker::TryGetCompletedChunk(TerrainChunk& chunk)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        while (!m_completed.empty())
        {
            Completed completed = std::move(m_completed.front());
            m_completed.pop_front();
            const auto pending = m_pending.find(completed.key);
            if (pending == m_pending.end() || pending->second != completed.ticket)
            {
                continue;
            }
            m_pending.erase(pending);
            chunk = std::move(completed.chunk);
            return true;
        }
        return false;
    }

    bool TerrainGenerationWorker::IsRunning() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_running && !m_stopping;
    }

    void TerrainGenerationWorker::WorkerLoop()
    {
        std::shared_ptr<VoxelTerrain> terrain;
        float chunkSize = 32.0f;
        float vertexSpacing = 1.0f;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            terrain = m_voxelTerrain;
            chunkSize = m_chunkSize;
            vertexSpacing = m_vertexSpacing;
        }

        VoxelMesherSettings mesherSettings{};
        mesherSettings.spacing = 2.0f;
        VoxelMesher mesher(mesherSettings);

        while (true)
        {
            Request request{};
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_condition.wait(lock, [this]
                {
                    return m_stopping || !m_requests.empty();
                });
                if (m_stopping)
                {
                    return;
                }

                request = m_requests.top();
                m_requests.pop();

                const auto pending = m_pending.find(request.key);
                if (pending == m_pending.end() || pending->second != request.ticket)
                {
                    continue;
                }

                if (std::abs(request.x - m_centerX) > m_maxDistance ||
                    std::abs(request.z - m_centerZ) > m_maxDistance)
                {
                    m_pending.erase(pending);
                    continue;
                }
            }

            TerrainChunk chunk(request.x, request.z, chunkSize, vertexSpacing);
            VoxelMesh mesh;
            const bool generated = mesher.GenerateChunk(
                *terrain, request.x, request.z, mesh);
            if (generated && !mesh.IsEmpty())
            {
                chunk.SetVoxelMesh(std::move(mesh));
            }

            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (m_stopping)
                {
                    return;
                }

                const auto pending = m_pending.find(request.key);
                if (pending == m_pending.end() || pending->second != request.ticket)
                {
                    continue;
                }

                if (!generated ||
                    std::abs(request.x - m_centerX) > m_maxDistance ||
                    std::abs(request.z - m_centerZ) > m_maxDistance)
                {
                    m_pending.erase(pending);
                    continue;
                }

                if (request.urgent)
                {
                    m_completed.push_front({std::move(chunk), request.key, request.ticket});
                }
                else
                {
                    m_completed.push_back({std::move(chunk), request.key, request.ticket});
                }
            }
        }
    }
}
