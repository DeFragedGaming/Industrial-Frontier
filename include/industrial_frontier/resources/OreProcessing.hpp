#pragma once

#include "industrial_frontier/resources/PlayerInventory.hpp"

#include <string>
#include <vector>

namespace industrial_frontier
{
    enum class ProcessingOperation
    {
        Crush,
        Grind,
        Roast,
        MagneticSeparation,
        Flotation
    };

    struct ProcessingResult
    {
        bool success = false;
        std::string message;
        std::vector<InventoryOreBatch> outputs;
        float concentrateKg = 0.0f;
        float tailingsKg = 0.0f;
        float targetRecoveryPercent = 0.0f;
    };

    class OreProcessing
    {
    public:
        static ProcessingResult Execute(const InventoryOreBatch& batch,
                                        ProcessingOperation operation);
        static const char* OperationName(ProcessingOperation operation);
        static const char* StageName(OreProcessingStage stage);
    };
}
