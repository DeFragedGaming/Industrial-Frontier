#include "industrial_frontier/renderer/CampfireVisuals.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace industrial_frontier
{
    namespace
    {
        constexpr float Pi = 3.14159265358979323846f;

        struct V
        {
            float x;
            float y;
            float z;
        };

        struct C
        {
            float r;
            float g;
            float b;
        };

        V Add(V a, V b)
        {
            return {a.x + b.x, a.y + b.y, a.z + b.z};
        }

        V Sub(V a, V b)
        {
            return {a.x - b.x, a.y - b.y, a.z - b.z};
        }

        V Cross(V a, V b)
        {
            return {a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z,
                a.x*b.y-a.y*b.x};
        }

        void Triangle(VoxelMesh& mesh, V a, V b, V c, C color)
        {
            V n = Cross(Sub(b,a), Sub(c,a));
            const float size = std::sqrt(n.x*n.x+n.y*n.y+n.z*n.z);
            if (size < 0.00001f)
                return;
            n = {n.x/size, n.y/size, n.z/size};
            const auto index = static_cast<unsigned int>(mesh.vertices.size());
            for (V p : {a,b,c})
            {
                TerrainVertex v{};
                v.position[0] = p.x;
                v.position[1] = p.y;
                v.position[2] = p.z;
                v.normal[0] = n.x;
                v.normal[1] = n.y;
                v.normal[2] = n.z;
                v.color[0] = color.r;
                v.color[1] = color.g;
                v.color[2] = color.b;
                mesh.vertices.push_back(v);
            }
            for (unsigned int i = 0; i != 3; ++i)
                mesh.indices.push_back(index + i);
        }

        void Quad(VoxelMesh& mesh, V a, V b, V c, V d, C color)
        {
            Triangle(mesh,a,b,c,color);
            Triangle(mesh,a,c,d,color);
        }

        void Rock(VoxelMesh& mesh, V center, float radius, int seed)
        {
            const float height = radius * (0.68f + 0.08f * (seed%3));
            constexpr int sides = 7;
            const V top{center.x,center.y+height,center.z};
            const V bottom{center.x,center.y-height*0.28f,center.z};
            for (int i=0; i<sides; ++i)
            {
                const float a0 = 2.0f*Pi*i/sides;
                const float a1 = 2.0f*Pi*(i+1)/sides;
                const float r0 = radius*(0.89f+0.12f*((i+seed)%3));
                const float r1 = radius*(0.89f+0.12f*((i+1+seed)%3));
                const V p{center.x+std::cos(a0)*r0,center.y,center.z+std::sin(a0)*r0};
                const V q{center.x+std::cos(a1)*r1,center.y,center.z+std::sin(a1)*r1};
                const float shade=0.55f + 0.04f * ((i+seed)%5);
                Triangle(mesh, top, p, q, {shade,shade*0.93f,shade*0.84f});
                Triangle(mesh, bottom, q, p, {shade*0.78f,shade*0.76f,shade*0.72f});
            }
        }

        void Log(VoxelMesh& mesh, V origin, float angle, float length,
            float radius, int seed)
        {
            const V axis{std::cos(angle),0.0f,std::sin(angle)};
            const V right{-std::sin(angle),0.0f,std::cos(angle)};
            const V start{origin.x-axis.x*length*0.5f,origin.y,
                origin.z-axis.z*length*0.5f};
            const V end{origin.x+axis.x*length*0.5f,origin.y,
                origin.z+axis.z*length*0.5f};
            constexpr int sides=8;
            for (int i=0;i<sides;++i)
            {
                const float a = 2.0f*Pi*i/sides;
                const float b = 2.0f*Pi*(i+1)/sides;
                const V ra{right.x*std::cos(a)*radius,std::sin(a)*radius,
                    right.z*std::cos(a)*radius};
                const V rb{right.x*std::cos(b)*radius,std::sin(b)*radius,
                    right.z*std::cos(b)*radius};
                const float tint = 0.79f + 0.045f*((i+seed)%4);
                const C bark{0.42f*tint,0.24f*tint,0.125f*tint};
                Quad(mesh,Add(start,ra),Add(start,rb),Add(end,rb),Add(end,ra),bark);
                Triangle(mesh,start,Add(start,ra),Add(start,rb),
                    {0.63f,0.43f,0.24f});
                Triangle(mesh,end,Add(end,rb),Add(end,ra),
                    {0.65f,0.45f,0.26f});
            }
        }
    }

    namespace
    {
        void ClayCover(VoxelMesh& mesh, float x, float y, float z)
        {
            constexpr int sides = 10;
            const V top{x,y+0.37f,z};
            for (int i=0;i<sides;++i)
            {
                const float a=2.0f*Pi*i/sides;
                const float b=2.0f*Pi*(i+1)/sides;
                const V left{x+0.52f*std::cos(a),y+0.14f,
                    z+0.52f*std::sin(a)};
                const V right{x+0.52f*std::cos(b),y+0.14f,
                    z+0.52f*std::sin(b)};
                const float factor=0.75f+0.04f*(i%4);
                Triangle(mesh,left,right,top,
                    {0.45f*factor,0.235f*factor,0.13f*factor});
            }
        }
    }

    void CampfireVisuals::BuildPit(const std::vector<CampfireState>& fires,
        VoxelMesh& output)
    {
        output.Clear();
        for (const auto& fire : fires)
        {
            const std::size_t firstVertex = output.vertices.size();
            for (int i=0;i<12;++i)
            {
                const float angle = 2.0f*Pi*i/12.0f;
                const float radius=0.64f;
                Rock(output,{fire.x+std::cos(angle)*radius,
                    fire.y+0.09f,fire.z+std::sin(angle)*radius},0.23f,i);
            }
            for (int i=0;i<5;++i)
            {
                const float angle=Pi*i/5.0f;
                Log(output,{fire.x,fire.y+0.19f+i*0.016f,fire.z},angle,
                    1.05f,0.089f,i);
            }
            if (fire.covered)
                ClayCover(output,fire.x,fire.y,fire.z);
            for (int i=0;i<6;++i)
            {
                const float angle=2.0f*Pi*i/6.0f;
                const float radius=0.3f;
                Rock(output,{fire.x+std::cos(angle)*radius,
                    fire.y+0.07f,fire.z+std::sin(angle)*radius},0.13f,i+18);
            }
            const float radians = fire.yawDegrees * Pi / 180.0f;
            const float c = std::cos(radians);
            const float s = std::sin(radians);
            for (std::size_t v = firstVertex; v < output.vertices.size(); ++v)
            {
                auto& vertex = output.vertices[v];
                const float dx = vertex.position[0] - fire.x;
                const float dz = vertex.position[2] - fire.z;
                vertex.position[0] = fire.x + c * dx - s * dz;
                vertex.position[2] = fire.z + s * dx + c * dz;
                const float nx = vertex.normal[0];
                const float nz = vertex.normal[2];
                vertex.normal[0] = c * nx - s * nz;
                vertex.normal[2] = s * nx + c * nz;
            }
        }
    }

    void CampfireVisuals::BuildParticles(const std::vector<CampfireState>& fires,
        float time, VoxelMesh& flame, VoxelMesh& smoke)
    {
        flame.Clear();
        smoke.Clear();
        for (const auto& fire : fires)
        {
            if (fire.fuelSeconds <= 0.0f)
                continue;
            if (!fire.covered)
            for (int i=0;i<7;++i)
            {
                const float angle = i*2.39996f;
                const float shift=0.13f + 0.025f*(i%3);
                const float x=fire.x+std::cos(angle)*shift;
                const float z=fire.z+std::sin(angle)*shift;
                const float t=time*(3.6f+0.35f*(i%4)) + i*1.9f;
                const float wiggle=std::sin(t)*0.11f;
                const float height=0.52f+0.36f*(0.5f+0.5f*std::sin(t+1.0f));
                const float base=fire.y+0.25f;
                const V a{x-0.14f,base,z};
                const V b{x+0.14f,base,z};
                const V c{x+wiggle,base+height,z+std::sin(t*0.6f)*0.035f};
                const V d{x+wiggle*0.45f,base+height*0.48f,z};
                const C orange{1.0f,0.34f+0.04f*(i%3),0.035f};
                const C yellow{1.0f,0.80f,0.14f};
                Triangle(flame,a,b,d,yellow);
                Triangle(flame,a,d,c,orange);
                Triangle(flame,b,c,d,orange);
                const V az{x,base,z-0.11f};
                const V bz{x,base,z+0.11f};
                Triangle(flame,az,bz,c,orange);
            }
            for (int i=0;i<12;++i)
            {
                const float phase = std::fmod(time*0.42f + 0.107f*i, 1.0f);
                const float rise = phase * 2.6f;
                const float angle=i*2.39996f + time*0.24f;
                const float drift=0.08f + 0.22f*phase;
                const float x=fire.x+std::cos(angle)*drift;
                const float y=fire.y+0.9f+rise;
                const float z=fire.z+std::sin(angle)*drift;
                const float size=0.12f+0.17f*phase;
                const float shade=0.30f+0.18f*phase;
                const C gray{shade,shade+0.012f,shade+0.02f};
                Quad(smoke,{x-size,y-size*0.5f,z},
                    {x+size,y-size*0.5f,z},
                    {x+size*0.8f,y+size*0.8f,z},
                    {x-size*0.8f,y+size*0.8f,z},gray);
                Quad(smoke,{x,y-size*0.5f,z-size},
                    {x,y-size*0.5f,z+size},
                    {x,y+size*0.8f,z+size*0.8f},
                    {x,y+size*0.8f,z-size*0.8f},gray);
            }
        }
    }
}
