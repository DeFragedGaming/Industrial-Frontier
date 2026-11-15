#include "industrial_frontier/resources/OreProcessing.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string_view>
#include <vector>

namespace industrial_frontier
{
    namespace
    {
        bool In(std::string_view id, std::initializer_list<std::string_view> values)
        {
            for (const auto value : values)
            {
                if (id == value) return true;
            }
            return false;
        }

        bool Roastable(std::string_view id)
        {
            return In(id, {"hematite", "goethite", "siderite", "limonite",
                "chalcopyrite", "bornite", "chalcocite", "covellite"});
        }

        float CaptureFraction(std::string_view id, bool gangue,
                              ProcessingOperation operation, bool roasted)
        {
            if (gangue) return operation == ProcessingOperation::Flotation ? 0.045f : 0.055f;
            if (operation == ProcessingOperation::MagneticSeparation)
            {
                if (id == "magnetite") return 0.95f;
                if (id == "hematite") return roasted ? 0.85f : 0.73f;
                if (id == "goethite") return roasted ? 0.83f : 0.28f;
                if (id == "siderite") return roasted ? 0.915f : 0.08f;
                if (id == "limonite") return roasted ? 0.78f : 0.19f;
                return 0.02f;
            }
            if (In(id, {"chalcopyrite", "bornite", "chalcocite", "covellite", "enargite"}))
                return 0.88f;
            if (In(id, {"galena", "sphalerite", "pentlandite", "molybdenite", "stannite"}))
                return 0.84f;
            if (id == "pyrite") return 0.08f;
            return 0.015f;
        }

        std::string Mass(float value)
        {
            std::ostringstream output;
            output << std::fixed << std::setprecision(2) << value;
            return output.str();
        }

        InventoryOreBatch CreateStream(const InventoryOreBatch& original,
                                       const std::vector<double>& masses,
                                       double totalMass, OreProcessingStage stage,
                                       const std::string& name)
        {
            InventoryOreBatch result = original;
            result.processingStage = stage;
            result.displayName = name;
            result.massKg = static_cast<float>(totalMass);
            result.components.clear();
            result.oreMineralFraction = 0.0f;
            for (std::size_t i = 0; i < masses.size(); ++i)
            {
                if (masses[i] <= 0.0) continue;
                InventoryMineralComponent component = original.components[i];
                component.massFraction = static_cast<float>(masses[i] / totalMass);
                if (!component.gangue)
                    result.oreMineralFraction += component.massFraction;
                result.components.push_back(std::move(component));
            }
            return result;
        }
    }

    const char* OreProcessing::OperationName(ProcessingOperation operation)
    {
        switch (operation)
        {
            case ProcessingOperation::Crush: return "Crush";
            case ProcessingOperation::Grind: return "Grind";
            case ProcessingOperation::Roast: return "Roast";
            case ProcessingOperation::MagneticSeparation: return "Magnetic separation";
            case ProcessingOperation::Flotation: return "Flotation";
        }
        return "Unknown";
    }

    const char* OreProcessing::StageName(OreProcessingStage stage)
    {
        switch (stage)
        {
            case OreProcessingStage::Raw: return "Raw ore";
            case OreProcessingStage::Crushed: return "Crushed ore";
            case OreProcessingStage::Ground: return "Ground ore";
            case OreProcessingStage::Roasted: return "Roasted / conditioned ore";
            case OreProcessingStage::Concentrate: return "Concentrate";
            case OreProcessingStage::Tailings: return "Tailings";
        }
        return "Unknown";
    }

    ProcessingResult OreProcessing::Execute(const InventoryOreBatch& batch,
                                             ProcessingOperation operation)
    {
        ProcessingResult result;
        if (batch.components.empty() || !std::isfinite(batch.massKg) ||
            batch.massKg <= 0.0f)
        {
            result.message = "Invalid ore batch";
            return result;
        }
        if (batch.processingStage == OreProcessingStage::Concentrate ||
            batch.processingStage == OreProcessingStage::Tailings)
        {
            result.message = "This output is stored for later refining or reprocessing";
            return result;
        }

        if (operation == ProcessingOperation::Crush)
        {
            if (batch.processingStage != OreProcessingStage::Raw)
            {
                result.message = "Only raw ore can be crushed";
                return result;
            }
            auto output = batch;
            output.processingStage = OreProcessingStage::Crushed;
            output.displayName = "Crushed " + batch.displayName;
            result.outputs.push_back(std::move(output));
            result.success = true;
            result.message = "Crushed " + Mass(batch.massKg) + " kg with no modeled mass loss";
            return result;
        }
        if (operation == ProcessingOperation::Grind)
        {
            if (batch.processingStage != OreProcessingStage::Crushed)
            {
                result.message = "Crush ore before grinding";
                return result;
            }
            auto output = batch;
            output.processingStage = OreProcessingStage::Ground;
            output.displayName = "Ground " + batch.primaryMineralId + " ore";
            result.outputs.push_back(std::move(output));
            result.success = true;
            result.message = "Ground " + Mass(batch.massKg) + " kg for separation";
            return result;
        }
        if (operation == ProcessingOperation::Roast)
        {
            if (batch.processingStage != OreProcessingStage::Ground)
            {
                result.message = "Grinding is required before roasting";
                return result;
            }
            bool applicable = false;
            for (const auto& component : batch.components)
                if (!component.gangue && Roastable(component.mineralId))
                    applicable = true;
            if (!applicable)
            {
                result.message = "No supported mineral for thermal conditioning";
                return result;
            }
            auto output = batch;
            output.processingStage = OreProcessingStage::Roasted;
            output.displayName = "Conditioned " + batch.primaryMineralId + " ore";
            result.outputs.push_back(std::move(output));
            result.success = true;
            result.message = "Thermally conditioned; gas loss/phase chemistry not modeled yet";
            return result;
        }

        if (batch.processingStage != OreProcessingStage::Ground &&
            batch.processingStage != OreProcessingStage::Roasted)
        {
            result.message = "Crushing and grinding must precede separation";
            return result;
        }

        const bool roasted = batch.processingStage == OreProcessingStage::Roasted;
        std::vector<double> concentrateMasses;
        std::vector<double> tailingMasses;
        concentrateMasses.reserve(batch.components.size());
        tailingMasses.reserve(batch.components.size());
        double concentrateTotal = 0.0;
        double tailingTotal = 0.0;
        double usefulFeed = 0.0;
        double usefulRecovered = 0.0;
        for (const auto& component : batch.components)
        {
            const double componentMass = static_cast<double>(batch.massKg) * component.massFraction;
            const float fraction = CaptureFraction(component.mineralId, component.gangue,
                                                    operation, roasted);
            const double concentrate = componentMass * fraction;
            const double tailings = componentMass - concentrate;
            concentrateMasses.push_back(concentrate);
            tailingMasses.push_back(tailings);
            concentrateTotal += concentrate;
            tailingTotal += tailings;
            if (!component.gangue && fraction >= 0.25f)
            {
                usefulFeed += componentMass;
                usefulRecovered += concentrate;
            }
        }
        if (usefulFeed < 0.00001)
        {
            result.message = "No suitable mineral for this separation method";
            return result;
        }
        if (concentrateTotal < 0.00001 || tailingTotal < 0.00001)
        {
            result.message = "Batch too small to separate";
            return result;
        }
        const std::string type = operation == ProcessingOperation::MagneticSeparation
            ? "Magnetic" : "Flotation";
        result.outputs.push_back(CreateStream(batch, concentrateMasses,
            concentrateTotal, OreProcessingStage::Concentrate,
            type + " concentrate: " + batch.primaryMineralId));
        result.outputs.push_back(CreateStream(batch, tailingMasses,
            tailingTotal, OreProcessingStage::Tailings,
            type + " tailings: " + batch.primaryMineralId));
        result.concentrateKg = static_cast<float>(concentrateTotal);
        result.tailingsKg = static_cast<float>(tailingTotal);
        result.targetRecoveryPercent = static_cast<float>(100.0 * usefulRecovered / usefulFeed);
        result.success = true;
        result.message = type + ": " + Mass(result.concentrateKg) + " kg concentrate, " +
            Mass(result.tailingsKg) + " kg tailings; target recovery " +
            Mass(result.targetRecoveryPercent) + "%";
        return result;
    }
}
