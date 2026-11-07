#pragma once

#include "industrial_frontier/world/TerrainChunk.hpp"
#include "industrial_frontier/world/TerrainGenerator.hpp"
#include "industrial_frontier/world/VoxelTerrain.hpp"

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>
#include <vector>

namespace industrial_frontier
{
    class TerrainGenerationWorker
    {
    public:
        TerrainGenerationWorker();
        ~TerrainGenerationWorker();

        TerrainGenerationWorker(const TerrainGenerationWorker&) = delete;
        TerrainGenerationWorker& operator=(const TerrainGenerationWorker&) = delete;

        void Start(
            const TerrainSettings& settings,
            float chunkSize,
            float vertexSpacing
        );

        void Stop();
        void SetCenter(int chunkX, int chunkZ);
        void RequestChunk(int chunkX, int chunkZ);
        void RegenerateChunk(int chunkX, int chunkZ);
        bool DigSphere(float x, float y, float z, float radius);
        bool ReplaceEdits(const std::vector<VoxelEdit>& edits);
        float SampleDensity(float x, float y, float z) const;
        std::shared_ptr<const VoxelTerrain> GetVoxelTerrain() const;
        bool TryGetCompletedChunk(TerrainChunk& chunk);
        bool IsRunning() const;

    private:
        struct Request
        {
            int x = 0;
            int z = 0;
            int distanceSquared = 0;
            std::uint64_t key = 0;
            std::uint64_t ticket = 0;
            bool urgent = false;
        };

        struct RequestCompare
        {
            bool operator()(const Request& a, const Request& b) const
            {
                if (a.urgent != b.urgent)
                {
                    return !a.urgent;
                }
                if (a.distanceSquared != b.distanceSquared)
                {
                    return a.distanceSquared > b.distanceSquared;
                }
                return a.ticket > b.ticket;
            }
        };

        struct Completed
        {
            TerrainChunk chunk;
            std::uint64_t key = 0;
            std::uint64_t ticket = 0;
        };

        mutable std::mutex m_mutex;
        std::condition_variable m_condition;
        std::priority_queue<Request, std::vector<Request>, RequestCompare> m_requests;
        std::deque<Completed> m_completed;
        std::unordered_map<std::uint64_t, std::uint64_t> m_pending;
        std::vector<std::thread> m_threads;

        TerrainSettings m_settings{};
        std::shared_ptr<VoxelTerrain> m_voxelTerrain;
        float m_chunkSize = 32.0f;
        float m_vertexSpacing = 1.0f;
        int m_centerX = 0;
        int m_centerZ = 0;
        int m_maxDistance = 8;
        std::uint64_t m_nextTicket = 0;
        bool m_running = false;
        bool m_stopping = false;

        void QueueChunk(int chunkX, int chunkZ, bool force);
        void ReprioritizeLocked();
        void WorkerLoop();
        static std::uint64_t MakeChunkKey(int x, int z);
    };
}
