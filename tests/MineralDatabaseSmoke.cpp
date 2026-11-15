#include "industrial_frontier/resources/MineralDatabase.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "Expected mineral database path\n";
        return 1;
    }

    industrial_frontier::MineralDatabase database;
    std::string error;
    if (!database.LoadFromFile(argv[1], error))
    {
        std::cerr << error << '\n';
        return 2;
    }

    const auto* hematite = database.FindById("hematite");
    const auto* magnetite = database.FindByName("MAGNETITE");
    const auto* bauxite = database.FindById("bauxite");
    const auto* chalcopyrite = database.FindById("chalcopyrite");

    if (database.Count() < 90 || !hematite || !magnetite ||
        !bauxite || !chalcopyrite ||
        hematite->formula != "Fe2O3" ||
        magnetite->formula != "Fe3O4" ||
        bauxite->kind != industrial_frontier::ResourceKind::Rock ||
        !bauxite->formula.empty() ||
        chalcopyrite->processingRouteId != "copper_sulfide" ||
        database.FindByCommodity("Copper").size() < 7 ||
        database.Search("iron").empty())
    {
        std::cerr << "Mineral database metadata test failed\n";
        return 3;
    }

    const auto previousCount = database.Count();
    const auto invalidPath = std::filesystem::temp_directory_path() /
        "industrial_frontier_invalid_minerals.tsv";

    {
        std::ifstream source(argv[1], std::ios::binary);
        std::string header;
        std::string firstEntry;
        std::getline(source, header);
        std::getline(source, firstEntry);
        std::ofstream invalid(invalidPath, std::ios::binary | std::ios::trunc);
        invalid << header << '\n' << firstEntry << '\n' << firstEntry << '\n';
    }

    const bool loadedInvalid = database.LoadFromFile(invalidPath, error);
    std::error_code removeError;
    std::filesystem::remove(invalidPath, removeError);

    if (loadedInvalid || error.find("Duplicate") == std::string::npos ||
        database.Count() != previousCount ||
        database.FindById("hematite") == nullptr)
    {
        std::cerr << "Mineral database duplicate rejection test failed\n";
        return 4;
    }

    std::cout << "Stage 10A database OK: " << database.Count()
              << " resources loaded, queried and validated\n";
    return 0;
}
