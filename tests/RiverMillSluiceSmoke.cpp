#include "industrial_frontier/resources/PrimitiveProcessingMachines.hpp"
#include "industrial_frontier/resources/PrimitiveItems.hpp"
#include "industrial_frontier/renderer/PrimitiveMachineVisuals.hpp"
#include "industrial_frontier/renderer/RiverVisuals.hpp"
#include "industrial_frontier/world/TerrainGenerator.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace industrial_frontier;

namespace
{
    void Check(bool condition,const char* message)
    {
        if(!condition)
        {
            std::cerr<<"FAIL: "<<message<<'\n';
            std::exit(1);
        }
    }

    InventoryOreBatch Batch(const std::string& mineral,float mass,
        OreProcessingStage stage,std::uint64_t id)
    {
        InventoryOreBatch b;
        b.depositId=id;
        b.primaryMineralId=mineral;
        b.displayName=mineral+" ore";
        b.massKg=mass;
        b.oreMineralFraction=0.58f;
        b.processingStage=stage;
        b.components={{mineral,0.58f,false},{"quartz",0.42f,true}};
        return b;
    }
}

int main()
{
    TerrainSettings settings;
    settings.seed=16493;
    TerrainGenerator terrain(settings);
    const auto stream=RiverSystem::Sample(0.0f,18.0f,settings.seed);
    const float x=stream.centerX;
    float surface=0.0f,fx=0.0f,fz=0.0f,speed=0.0f;
    Check(terrain.GetRiverWater(x,18.0f,surface,fx,fz,speed),"center has water");
    Check(speed>=0.4f,"river current flows");
    Check(std::abs(fx*fx+fz*fz-1.0f)<0.001f,"normalized flow direction");
    Check(terrain.GetHeight(x,18.0f)<surface-0.5f,"river channel physically excavated");
    Check(!terrain.GetRiverWater(x+16.0f,18.0f,surface,fx,fz,speed),"bank is dry");
    Check(std::abs(terrain.GetHeight(x+20.0f,18.0f)-
        terrain.GetNaturalHeight(x+20.0f,18.0f))<0.001f,"terrain outside channel intact");
    const auto streamRepeat=RiverSystem::Sample(0.0f,18.0f,settings.seed);
    Check(streamRepeat.centerX==stream.centerX,"river reproducible by seed");
    const auto alternate=RiverSystem::Sample(0.0f,18.0f,settings.seed+1);
    Check(alternate.centerX!=stream.centerX,"different seeds move river");

    VoxelMesh waterMesh;
    const int cx=static_cast<int>(std::floor(x/32.0f));
    RiverVisuals::BuildChunk(terrain,cx,0,32.0f,waterMesh);
    Check(!waterMesh.IsEmpty(),"renderable water ribbon");
    VoxelMesh waterMeshOther;
    RiverVisuals::BuildChunk(terrain,cx+3,0,32.0f,waterMeshOther);
    Check(waterMeshOther.IsEmpty(),"no water far from river");

    bool millRecipe=false,sluiceRecipe=false;
    for(const auto& recipe:PrimitiveRecipes)
    {
        if(recipe.id=="hand_mill_recipe" && recipe.resultId=="hand_mill_kit")millRecipe=true;
        if(recipe.id=="sluice_recipe" && recipe.resultId=="sluice_kit")sluiceRecipe=true;
    }
    Check(millRecipe && sluiceRecipe,"both machine recipes registered");
    PlayerInventory crafter;
    Check(crafter.AddItem("raw_stone",14),"starter stone for machines");
    Check(crafter.AddItem("log",14),"starter logs for machines");
    Check(crafter.AddItem("stick",18),"starter sticks for machines");
    Check(crafter.AddItem("clay_soil",2),"starter clay for mill");
    std::string craftMessage;
    Check(crafter.Craft("hand_mill_recipe",craftMessage),"craft hand mill");
    Check(crafter.Craft("sluice_recipe",craftMessage),"craft river sluice");
    Check(crafter.ItemCount("hand_mill_kit")==1 &&
        crafter.ItemCount("sluice_kit")==1,"crafted both kits");
    PrimitiveProcessingMachines machines;
    Check(machines.Place(PrimitiveMachineKind::Sluice,x,10.0f,18.0f,0.0f,false)==0,
        "sluice requires flowing water");
    const auto mill=machines.Place(PrimitiveMachineKind::Mill,20.0f,10.0f,15.0f,45.0f,false);
    Check(mill>0,"mill placed on dry ground");
    const auto sluice=machines.Place(PrimitiveMachineKind::Sluice,x,10.0f,18.0f,0.0f,true);
    Check(sluice>0,"sluice placed in river");
    Check(machines.Place(PrimitiveMachineKind::Sluice,x,10.0f,18.0f,0.0f,true)==0,
        "machine overlap rejected");
    for(const auto& mineral:std::vector<std::string>{"malachite","chalcopyrite","cassiterite","hematite","native_gold"})
    {
        const auto input=Batch(mineral,21.0f,OreProcessingStage::Crushed,123u+mineral.size());
        Check(machines.Load(mill,input),"mill accepts crushed ore from all tiers");
        Check(!machines.Remove(mill),"loaded mill cannot dismantle");
        Check(PrimitiveProcessingMachines::Validate(machines.Machines()),"loaded state valid");
        PrimitiveProcessingMachines restored;
        Check(restored.Restore(machines.Machines()),"save/load in-progress mill state");
        const int turns=static_cast<int>(restored.Find(mill)->workRequired);
        for(int i=0;i<turns;++i)Check(restored.Crank(mill),"hand mill rotates");
        Check(restored.Find(mill)->feed.processingStage==OreProcessingStage::Ground,
            "crushed becomes ground");
        InventoryOreBatch ground,none;
        Check(restored.Collect(mill,ground,none),"ground ore collected");
        Check(ground.primaryMineralId==mineral && ground.massKg==input.massKg &&
            ground.oreMineralFraction==input.oreMineralFraction &&
            ground.components.size()==2,"mill preserves mineral composition and mass");
        Check(machines.Restore(restored.Machines()),"reset mill between batches");
        if(mineral=="malachite")
        {
            Check(!machines.Load(sluice,input),"sluice rejects unground ore");
            auto sluiceFeed=ground;
            sluiceFeed.massKg=20.0f;
            Check(machines.Load(sluice,sluiceFeed),"ground ore loads into sluice");
            Check(PrimitiveProcessingMachines::Validate(machines.Machines()),"loaded sluice valid");
            Check(!machines.Flow(sluice,0.2f,0.0f),"no flow no separation");
            for(int k=0;k<2000 && machines.Find(sluice)->feed.processingStage==OreProcessingStage::Ground;++k)
                Check(machines.Flow(sluice,0.1f,1.1f),"current advances sluice work");
            Check(machines.Find(sluice)->feed.processingStage==OreProcessingStage::Concentrate,
                "sluice finishes concentration");
            Check(PrimitiveProcessingMachines::Validate(machines.Machines()),"finished state valid");
            PrimitiveProcessingMachines loaded;
            Check(loaded.Restore(machines.Machines()),"completed state survives restore");
            InventoryOreBatch concentrate,tailings;
            Check(loaded.Collect(sluice,concentrate,tailings),"collect both mineral streams");
            Check(concentrate.processingStage==OreProcessingStage::Concentrate &&
                tailings.processingStage==OreProcessingStage::Tailings,"two valid outputs");
            Check(std::abs(concentrate.massKg+tailings.massKg-sluiceFeed.massKg)<0.002f,
                "mass conserved through gravity separation");
            Check(concentrate.oreMineralFraction>ground.oreMineralFraction,
                "concentrate upgraded mineral grade");
            Check(PlayerInventory::IsValidState({0.0f,{concentrate,tailings},{}}),
                "concentrate and tailings are valid inventory batches");
            Check(machines.Restore(loaded.Machines()),"reset sluice state after collect");
        }
    }
    VoxelMesh machinesMesh;
    PrimitiveMachineVisuals::Build(machines.Machines(),machinesMesh);
    Check(!machinesMesh.IsEmpty() && machinesMesh.vertices.size()>300,
        "procedurally generated mill and sluice 3D models");
    Check(machines.Remove(mill) && machines.Remove(sluice),"recover empty machines");
    std::cout<<"Stage 10H-2 rivers, hand mill, river sluice: passed\n";
}
