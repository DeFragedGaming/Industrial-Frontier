#include "industrial_frontier/resources/GeologyGenerator.hpp"
#include "industrial_frontier/world/VoxelTerrain.hpp"
#include <cmath>
#include <iostream>
#include <unordered_set>
#include <algorithm>
int main()
{
    using namespace industrial_frontier;
    TerrainSettings settings{};
    VoxelTerrain world(settings, 32.0f, 2.0f);
    const auto* geology = world.GetGeology();
    if (!geology || !geology->IsReady()) return 1;
    const auto caves = world.FindCaveEntrances(-750, 750, -750, 750);
    const auto surface = geology->FindSurfaceDeposits(-750, 750, -750, 750);
    const auto veins = geology->FindUndergroundVeins(-750, 750, -79, 150, -750, 750);
    std::cout << "caves=" << caves.size() << " surface=" << surface.size() << " veins=" << veins.size() << '\n';
    if (caves.empty() || surface.empty() || veins.empty()) return 2;
    if (!geology->FindUndergroundNodes(-750,750,-79,150,-750,750).empty()) return 3;
    std::size_t finite=0, permanent=0, collisions=0;
    for (const auto& d : surface)
    {
        if (d.kind == DepositKind::FiniteOutcrop) ++finite;
        if (d.kind == DepositKind::PermanentNode)
        {
            ++permanent;
            for (const auto& c: caves)
            {
                const float vx=c.chamberX-c.x, vz=c.chamberZ-c.z;
                const float t=std::clamp(((d.x-c.x)*vx+(d.z-c.z)*vz)/(vx*vx+vz*vz),0.f,1.f);
                const float ax=c.x+t*vx, az=c.z+t*vz;
                const float ds=std::hypot(d.x-ax,d.z-az);
                if (ds < 98.f+d.radius) ++collisions;
            }
        }
    }
    std::unordered_set<std::uint64_t> ids;
    std::size_t visible=0, onCave=0;
    for (const auto& d: veins)
    {
        if (!ids.insert(d.id).second) return 5;
        bool nearCave=false;
        for(const auto& c:caves)
        {
            if(std::hypot(d.x-c.chamberX,d.z-c.chamberZ)<43.f || std::hypot(d.x-c.x,d.z-c.z)<43.f) nearCave=true;
        }
        if(nearCave) ++onCave;
        for(float r=-6;r<=6;r+=2)
            for(float t=-6;t<=6;t+=2)
            {
                float x=d.x+d.directionX*r-d.directionZ*t;
                float z=d.z+d.directionZ*r+d.directionX*t;
                float y=d.y;
                if(std::abs(world.SampleDensity(x,y,z))<1.f && GeologyGenerator::VeinInfluence(d,x,y,z)>0.08f) {++visible;goto next;}
            }
        next:;
    }
    std::cout << "finite="<<finite<<" perm="<<permanent<<" exclusion violations="<<collisions<<" near cave="<<onCave<<" visible sample="<<visible<<" unique="<<ids.size()<<'\n';
    if(finite<1 || permanent<1 || collisions || onCave<veins.size()*0.80 || visible<veins.size()*0.25) return 4;
    const auto again=geology->FindUndergroundVeins(-750,750,-79,150,-750,750);
    if(again.size()!=veins.size()) return 6;
    for(size_t i=0;i<again.size();++i) if(again[i].id!=veins[i].id) return 7;
    return 0;
}
