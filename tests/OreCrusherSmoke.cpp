#include "industrial_frontier/resources/OreCrusher.hpp"
#include "industrial_frontier/resources/PrimitiveItems.hpp"
#include "industrial_frontier/renderer/OreCrusherVisuals.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace industrial_frontier;

namespace
{
    void Require(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAIL: " << message << '\n';
            std::exit(1);
        }
    }

    InventoryOreBatch CreateBatch(const std::string& mineral,float mass,
        std::uint64_t id)
    {
        InventoryOreBatch result;
        result.depositId=id;
        result.primaryMineralId=mineral;
        result.displayName=mineral+" Ore";
        result.massKg=mass;
        result.oreMineralFraction=0.70f;
        result.components={{mineral,0.70f,false},{"quartz",0.30f,true}};
        return result;
    }
}

int main()
{
    Require(PrimitiveRecipes.size()>=18,"crusher recipe available");
    bool recipe=false;
    for (const auto& entry:PrimitiveRecipes)
        if (entry.id=="ore_crusher_recipe" && entry.resultId=="ore_crusher_kit")
            recipe=true;
    Require(recipe,"craftable crusher kit");
    PlayerInventory player;
    Require(player.AddItem("raw_stone",12),"stone resources");
    Require(player.AddItem("log",8),"log resources");
    Require(player.AddItem("stick",8),"stick resources");
    Require(player.AddItem("clay_soil",4),"clay resources");
    std::string message;
    Require(player.Craft("ore_crusher_recipe",message),"crusher crafting");
    Require(player.ItemCount("ore_crusher_kit")==1,"crusher kit created");
    OreCrusherWorld wheelWorld;
    const auto wheelId=wheelWorld.Place(0.0f,0.0f,0.0f,0.0f);
    Require(wheelId>0,"wheel orientation test placement");
    VoxelMesh wheelMesh;
    OreCrusherVisuals::Build(wheelWorld.Crushers(),wheelMesh);
    int radialFaces=0;
    for (const auto& vertex : wheelMesh.vertices)
    {
        if (vertex.position[1]>0.78f && vertex.position[1]<1.28f &&
            vertex.position[0]>0.54f && vertex.position[0]<1.06f &&
            std::abs(vertex.normal[0])>0.92f &&
            std::abs(vertex.normal[2])<0.12f)
            ++radialFaces;
    }
    Require(radialFaces>=40,"wheel faces radially and rolls along the circular path");
    OreCrusherWorld world;
    const auto id=world.Place(10.0f,15.0f,10.0f,45.0f);
    Require(id>0,"place crusher");
    Require(world.Place(10.0f,15.0f,10.0f,0.0f)==0,"reject overlap");
    for (const auto& [name,tier] : std::vector<std::pair<std::string,int>>{
        {"malachite",1},{"chalcopyrite",2},{"hematite",3},
        {"cassiterite",3},{"quartz",3},{"gold",3}})
    {
        const auto original=CreateBatch(name,30.0f,static_cast<std::uint64_t>(tier+name.size()));
        Require(OreCrusherWorld::HardnessTier(original)==tier,"ore tier");
        Require(world.Load(id,original),"accept raw ore across tiers");
        Require(!world.Load(id,original),"cannot double-load");
        Require(!world.Remove(id),"cannot dismantle loaded machine");
        const auto previous=world.Crushers();
        Require(OreCrusherWorld::Validate(previous),"valid raw crusher state");
        OreCrusherWorld restored;
        Require(restored.Restore(previous),"restore in-progress work");
        const float steps=restored.Find(id)->workRequired;
        Require(steps>=3.0f,"nonzero work cost");
        for (int stroke=0;stroke<static_cast<int>(steps);++stroke)
            Require(restored.Crank(id),"crank advances work");
        Require(restored.Find(id)->batch.processingStage==OreProcessingStage::Crushed,
            "batch crushed");
        Require(!restored.Crank(id),"no additional work when done");
        Require(OreCrusherWorld::Validate(restored.Crushers()),"finished state valid");
        VoxelMesh mesh;
        OreCrusherVisuals::Build(restored.Crushers(),mesh);
        Require(!mesh.IsEmpty(),"visible procedural model");
        Require(mesh.vertices.size()>300,"crusher includes detailed geometry");
        InventoryOreBatch output;
        Require(restored.Collect(id,output),"collect crushed ore");
        Require(output.processingStage==OreProcessingStage::Crushed,"output stage");
        Require(std::abs(output.massKg-30.0f)<0.001f,"no mass loss");
        Require(output.primaryMineralId==name,"mineral identity preserved");
        Require(output.components.size()==2 &&
            std::abs(output.components[0].massFraction-0.7f)<0.0001f,
            "grade and gangue preserved");
        Require(world.Restore(restored.Crushers()),"reload empty machine");
    }
    auto oversized=CreateBatch("malachite",31.0f,100);
    Require(!world.Load(id,oversized),"30 kg capacity enforced");
    Require(world.Remove(id),"empty crusher can dismantle");
    std::cout << "Stage 10H-1 edge-runner crusher: all tiers, processing, model, and persistence passed\n";
}
