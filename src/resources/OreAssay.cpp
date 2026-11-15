#include "industrial_frontier/resources/OreAssay.hpp"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>

namespace industrial_frontier
{
    namespace
    {
        const std::unordered_map<std::string, double>& AtomicWeights()
        {
            static const std::unordered_map<std::string, double> weights = {
                {"H", 1.008}, {"He", 4.0026}, {"Li", 6.94}, {"Be", 9.0122},
                {"B", 10.81}, {"C", 12.011}, {"N", 14.007}, {"O", 15.999},
                {"F", 18.998}, {"Na", 22.990}, {"Mg", 24.305}, {"Al", 26.982},
                {"Si", 28.085}, {"P", 30.974}, {"S", 32.06}, {"Cl", 35.45},
                {"K", 39.098}, {"Ca", 40.078}, {"Sc", 44.956}, {"Ti", 47.867},
                {"V", 50.942}, {"Cr", 51.996}, {"Mn", 54.938}, {"Fe", 55.845},
                {"Co", 58.933}, {"Ni", 58.693}, {"Cu", 63.546}, {"Zn", 65.38},
                {"Ga", 69.723}, {"Ge", 72.630}, {"As", 74.922}, {"Se", 78.971},
                {"Rb", 85.468}, {"Sr", 87.62}, {"Y", 88.906}, {"Zr", 91.224},
                {"Nb", 92.906}, {"Mo", 95.95}, {"Ag", 107.868}, {"Cd", 112.414},
                {"In", 114.818}, {"Sn", 118.710}, {"Sb", 121.760}, {"Te", 127.60},
                {"Cs", 132.905}, {"Ba", 137.327}, {"La", 138.905}, {"Ce", 140.116},
                {"Pr", 140.908}, {"Nd", 144.242}, {"W", 183.84}, {"Au", 196.967},
                {"Hg", 200.592}, {"Tl", 204.38}, {"Pb", 207.2}, {"Bi", 208.980},
                {"Th", 232.038}, {"U", 238.029}, {"Ta", 180.948}, {"Hf", 178.49}
            };
            return weights;
        }

        using AtomCounts = std::map<std::string, double>;

        bool ReadNumber(std::string_view text, std::size_t& cursor,
            double& number)
        {
            const std::size_t start = cursor;
            unsigned int value = 0;
            while (cursor < text.size() &&
                std::isdigit(static_cast<unsigned char>(text[cursor])))
            {
                value = value * 10u + static_cast<unsigned int>(text[cursor] - '0');
                if (value > 100000u) return false;
                ++cursor;
            }
            number = cursor == start ? 1.0 : static_cast<double>(value);
            return number > 0.0;
        }

        bool ParseGroup(std::string_view formula, std::size_t& cursor,
            AtomCounts& result, bool nested)
        {
            while (cursor < formula.size())
            {
                const char ch = formula[cursor];
                if (ch == ')')
                {
                    if (!nested) return false;
                    ++cursor;
                    return true;
                }
                if (ch == '(')
                {
                    ++cursor;
                    AtomCounts inside;
                    if (!ParseGroup(formula, cursor, inside, true)) return false;
                    double multiplier = 1.0;
                    if (!ReadNumber(formula, cursor, multiplier)) return false;
                    for (const auto& item : inside)
                    {
                        result[item.first] += item.second * multiplier;
                    }
                    continue;
                }
                if (ch < 'A' || ch > 'Z') return false;
                std::string symbol(1, ch);
                ++cursor;
                if (cursor < formula.size() &&
                    formula[cursor] >= 'a' && formula[cursor] <= 'z')
                {
                    symbol.push_back(formula[cursor++]);
                }
                if (!AtomicWeights().contains(symbol)) return false;
                double number = 1.0;
                if (!ReadNumber(formula, cursor, number)) return false;
                result[symbol] += number;
            }
            return !nested;
        }

        bool ReadFormula(std::string_view formula, AtomCounts& counts)
        {
            if (formula.empty()) return false;
            std::size_t start = 0;
            while (start < formula.size())
            {
                const std::size_t end = formula.find("\xC2\xB7", start);
                const std::string_view part = formula.substr(start,
                    end == std::string_view::npos ? end : end - start);
                if (part.empty()) return false;
                std::size_t cursor = 0;
                double multiplier = 1.0;
                if (part[0] >= '0' && part[0] <= '9' &&
                    !ReadNumber(part, cursor, multiplier)) return false;
                AtomCounts local;
                if (!ParseGroup(part, cursor, local, false) ||
                    cursor != part.size() || local.empty()) return false;
                for (const auto& item : local)
                {
                    counts[item.first] += item.second * multiplier;
                }
                if (end == std::string_view::npos) break;
                start = end + 2;
            }
            return !counts.empty();
        }

        std::string Percentage(float fraction)
        {
            std::ostringstream stream;
            stream << std::fixed << std::setprecision(1)
                   << fraction * 100.0f << "%";
            return stream.str();
        }
    }

    OreGradeTier ClassifyOreGrade(float fraction)
    {
        if (fraction < 0.12f) return OreGradeTier::Poor;
        if (fraction < 0.25f) return OreGradeTier::Low;
        if (fraction < 0.45f) return OreGradeTier::Moderate;
        if (fraction < 0.65f) return OreGradeTier::Rich;
        return OreGradeTier::Exceptional;
    }

    const char* OreGradeTierName(OreGradeTier tier)
    {
        switch (tier)
        {
            case OreGradeTier::Poor: return "POOR";
            case OreGradeTier::Low: return "LOW";
            case OreGradeTier::Moderate: return "MODERATE";
            case OreGradeTier::Rich: return "RICH";
            case OreGradeTier::Exceptional: return "EXCEPTIONAL";
        }
        return "UNKNOWN";
    }

    bool TheoreticalElementFraction(
        std::string_view mineralFormula, std::string_view element,
        double& fraction)
    {
        fraction = 0.0;
        AtomCounts formula;
        if (!ReadFormula(mineralFormula, formula)) return false;
        double mass = 0.0;
        for (const auto& item : formula)
        {
            mass += item.second * AtomicWeights().at(item.first);
        }
        if (mass <= 0.0) return false;
        const auto it = formula.find(std::string(element));
        if (it != formula.end())
        {
            fraction = it->second * AtomicWeights().at(it->first) / mass;
        }
        return true;
    }

    OreAssay AnalyzeOre(
        const GeologicalDeposit& deposit, const GeologyGenerator& geology)
    {
        OreAssay assay;
        if (!std::isfinite(deposit.grade) || deposit.grade < 0.0f ||
            deposit.grade > 1.0f || deposit.components.empty()) return assay;
        double sum = 0.0;
        for (const auto& item : deposit.components)
        {
            if (item.fraction < 0.0f || !std::isfinite(item.fraction) ||
                !geology.FindMineral(item.mineralId)) return assay;
            sum += item.fraction;
        }
        if (std::abs(sum - 1.0) > 0.001) return assay;
        double gangueSum = 0.0;
        for (const auto& item : deposit.gangue)
        {
            if (item.fraction < 0.0f || !std::isfinite(item.fraction) ||
                !geology.FindMineral(item.mineralId)) return assay;
            gangueSum += item.fraction;
        }
        if (std::abs(gangueSum - (1.0 - deposit.grade)) > 0.001) return assay;

        assay.oreMineralFraction = deposit.grade;
        assay.gangueFraction = 1.0f - deposit.grade;
        std::map<std::string, AssayElement> elements;
        for (const auto& component : deposit.components)
        {
            const MineralDefinition* mineral = geology.FindMineral(component.mineralId);
            assay.minerals.push_back({component.mineralId,
                deposit.grade * component.fraction, false});
            for (const std::string& symbol : mineral->targetElements)
            {
                auto& element = elements[symbol];
                element.symbol = symbol;
            }
        }
        for (const auto& component : deposit.gangue)
        {
            assay.minerals.push_back({component.mineralId,
                component.fraction, true});
        }
        for (const auto& component : deposit.components)
        {
            const MineralDefinition* mineral = geology.FindMineral(component.mineralId);
            for (auto& pair : elements)
            {
                double pureFraction = 0.0;
                if (!TheoreticalElementFraction(mineral->formula,
                    pair.first, pureFraction))
                {
                    pair.second.complete = false;
                }
                else
                {
                    pair.second.massFraction +=
                        static_cast<float>(deposit.grade * component.fraction *
                            pureFraction);
                }
            }
        }
        for (const auto& item : elements)
        {
            assay.elements.push_back(item.second);
        }
        assay.valid = true;
        return assay;
    }

    std::string FormatOreAssay(
        const GeologicalDeposit& deposit, const GeologyGenerator& geology)
    {
        const OreAssay assay = AnalyzeOre(deposit, geology);
        if (!assay.valid) return {};
        std::string label = std::string(OreGradeTierName(
            ClassifyOreGrade(assay.oreMineralFraction))) +
            " ORE " + Percentage(assay.oreMineralFraction);
        const MineralDefinition* primary = geology.FindMineral(deposit.mineralId);
        if (primary && !primary->targetElements.empty())
        {
            const std::string& primaryElement = primary->targetElements.front();
            for (const auto& element : assay.elements)
            {
                if (element.symbol == primaryElement && element.complete)
                {
                    label += "   " + element.symbol + " " +
                        Percentage(element.massFraction);
                    break;
                }
            }
        }
        if (deposit.components.size() > 1)
        {
            const MineralDefinition* secondary = geology.FindMineral(
                deposit.components[1].mineralId);
            if (secondary)
            {
                label += "   + " + secondary->name;
            }
        }
        if (!deposit.gangue.empty())
        {
            const MineralDefinition* gangue = geology.FindMineral(
                deposit.gangue.front().mineralId);
            if (gangue)
            {
                label += "   GANGUE: " + gangue->name;
            }
        }
        return label;
    }
}
