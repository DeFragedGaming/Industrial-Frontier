#pragma once

#include "industrial_frontier/world/TerrainChunk.hpp"
#include "industrial_frontier/world/TerrainGenerator.hpp"

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_set>
#include <deque>
#include <memory>
#include "industrial_frontier/world/VoxelTerrain.hpp"

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
            std::uint64_t revision = 0;
        };

        struct RequestCompare
        {
            bool operator()(const Request& a, const Request& b) const
            {
                return a.distanceSquared > b.distanceSquared;
            }
        };

        mutable std::mutex m_mutex;
        std::condition_variable m_condition;

        std::priority_queue<
            Request,
            std::vector<Request>,
            RequestCompare
        > m_requests;

        std::deque<TerrainChunk> m_completed;
        std::unordered_set<std::uint64_t> m_requested;

        std::thread m_thread;

        TerrainSettings m_settings{};
        std::shared_ptr<VoxelTerrain> m_voxelTerrain;
        void QueueChunk(int chunkX, int chunkZ, bool force);

        float m_chunkSize = 32.0f;
        float m_vertexSpacing = 1.0f;

        int m_centerX = 0;
        int m_centerZ = 0;
        int m_maxDistance = 6;

        bool m_running = false;
        bool m_stopping = false;

        void WorkerLoop();

        static std::uint64_t MakeChunkKey(int x, int z);
    };
}