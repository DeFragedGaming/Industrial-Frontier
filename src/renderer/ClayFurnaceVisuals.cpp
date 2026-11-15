#include "industrial_frontier/renderer/ClayFurnaceVisuals.hpp"

#include <algorithm>
#include <cmath>

namespace industrial_frontier
{
    namespace
    {
        constexpr float Pi = 3.14159265358979323846f;
        struct V { float x,y,z; };
        struct C { float r,g,b; };

        void Tri(VoxelMesh& mesh, V a, V b, V c, C tint)
        {
            const V u{b.x-a.x,b.y-a.y,b.z-a.z};
            const V v{c.x-a.x,c.y-a.y,c.z-a.z};
            V n{u.y*v.z-u.z*v.y,u.z*v.x-u.x*v.z,u.x*v.y-u.y*v.x};
            const float length = std::sqrt(n.x*n.x+n.y*n.y+n.z*n.z);
            if (length < 0.00001f) return;
            n={n.x/length,n.y/length,n.z/length};
            const auto index=static_cast<unsigned>(mesh.vertices.size());
            for (const V p : {a,b,c})
            {
                TerrainVertex vertex{};
                vertex.position[0]=p.x; vertex.position[1]=p.y;
                vertex.position[2]=p.z;
                vertex.normal[0]=n.x; vertex.normal[1]=n.y;
                vertex.normal[2]=n.z;
                vertex.color[0]=tint.r; vertex.color[1]=tint.g;
                vertex.color[2]=tint.b;
                mesh.vertices.push_back(vertex);
            }
            for (unsigned j=0;j<3;++j) mesh.indices.push_back(index+j);
        }

        void Quad(VoxelMesh& mesh, V a, V b, V c, V d, C tint)
        {
            Tri(mesh,a,b,c,tint);
            Tri(mesh,a,c,d,tint);
        }

        void Box(VoxelMesh& mesh, V lo, V hi, C tint)
        {
            Quad(mesh,{lo.x,lo.y,lo.z},{hi.x,lo.y,lo.z},
                {hi.x,hi.y,lo.z},{lo.x,hi.y,lo.z},tint);
            Quad(mesh,{hi.x,lo.y,hi.z},{lo.x,lo.y,hi.z},
                {lo.x,hi.y,hi.z},{hi.x,hi.y,hi.z},tint);
            Quad(mesh,{lo.x,lo.y,hi.z},{lo.x,lo.y,lo.z},
                {lo.x,hi.y,lo.z},{lo.x,hi.y,hi.z},tint);
            Quad(mesh,{hi.x,lo.y,lo.z},{hi.x,lo.y,hi.z},
                {hi.x,hi.y,hi.z},{hi.x,hi.y,lo.z},tint);
            Quad(mesh,{lo.x,hi.y,lo.z},{hi.x,hi.y,lo.z},
                {hi.x,hi.y,hi.z},{lo.x,hi.y,hi.z},tint);
            Quad(mesh,{lo.x,lo.y,hi.z},{hi.x,lo.y,hi.z},
                {hi.x,lo.y,lo.z},{lo.x,lo.y,lo.z},tint);
        }

        void Mold(VoxelMesh& mesh, float x, float y, float z, bool hot)
        {
            const C shell{0.17f,0.18f,0.19f};
            const C recess{0.065f,0.068f,0.072f};
            Box(mesh,{x-0.40f,y,z-0.22f},
                {x+0.40f,y+0.09f,z+0.22f},shell);
            Box(mesh,{x-0.32f,y+0.091f,z-0.14f},
                {x+0.32f,y+0.10f,z+0.14f},recess);
            Box(mesh,{x+0.39f,y+0.03f,z-0.035f},
                {x+0.93f,y+0.07f,z+0.035f},shell);
            if (hot)
            {
                Box(mesh,{x-0.285f,y+0.10f,z-0.115f},
                    {x+0.285f,y+0.123f,z+0.115f},
                    {1.00f,0.37f,0.045f});
                Box(mesh,{x-0.18f,y+0.124f,z-0.07f},
                    {x+0.18f,y+0.126f,z+0.07f},
                    {1.00f,0.74f,0.21f});
            }
        }

        void CastingTray(VoxelMesh& mesh, const ClayFurnaceState& f)
        {
            const bool multiple = f.moldCapacity == 8;
            const int cols = multiple ? 4 : 1;
            const int rows = multiple ? 2 : 1;
            const float startX = f.x - (multiple ? 0.62f : 0.29f);
            const float startZ = f.z + 1.15f;
            const float y = f.y + 0.16f;
            const float dx = multiple ? 0.31f : 0.58f;
            const float dz = multiple ? 0.31f : 0.38f;
            Box(mesh,{startX-0.07f,y,startZ-0.06f},
                {startX+cols*dx+0.07f,y+0.12f,startZ+rows*dz+0.05f},
                {0.22f,0.23f,0.24f});
            for (int r = 0; r < rows; ++r)
            {
                for (int c = 0; c < cols; ++c)
                {
                    const auto index = static_cast<std::uint32_t>(r*cols+c);
                    const float x = startX+c*dx;
                    const float z = startZ+r*dz;
                    Box(mesh,{x,y+0.125f,z},{x+dx-0.035f,y+0.132f,z+dz-0.035f},
                        {0.055f,0.060f,0.065f});
                    if (index < f.castCount)
                    {
                        const bool hot = f.castingCoolingSeconds > 0.0f;
                        const C metal = hot ? C{1.00f,0.47f,0.08f} :
                            f.metalId == "copper" ? C{0.72f,0.38f,0.18f} :
                            f.metalId == "tin" ? C{0.70f,0.75f,0.78f} :
                            C{0.50f,0.54f,0.59f};
                        Box(mesh,{x+0.018f,y+0.137f,z+0.015f},
                            {x+dx-0.053f,y+0.157f,z+dz-0.053f},metal);
                    }
                }
            }
        }
    }

    void ClayFurnaceVisuals::BuildBodies(
        const std::vector<ClayFurnaceState>& furnaces, VoxelMesh& output)
    {
        output.Clear();
        constexpr int sides=16;
        for (const auto& f : furnaces)
        {
            const std::size_t firstVertex = output.vertices.size();
            for (int ring=0;ring<5;++ring)
            {
                const float heights[6]{0.03f,0.39f,0.85f,1.35f,1.92f,2.03f};
                const float radii[6]{0.84f,0.82f,0.69f,0.55f,0.43f,0.43f};
                for (int i=0;i<sides;++i)
                {
                    const float a=2.0f*Pi*i/sides;
                    const float b=2.0f*Pi*(i+1)/sides;
                    const float sa=std::sin(a), ca=std::cos(a);
                    const float sb=std::sin(b), cb=std::cos(b);
                    const float variation=0.88f+0.035f*((i+ring*7)%5);
                    const C clay{0.49f*variation,0.31f*variation,
                        0.20f*variation};
                    const V p{f.x+radii[ring]*sa,f.y+heights[ring],
                        f.z+radii[ring]*ca};
                    const V q{f.x+radii[ring]*sb,f.y+heights[ring],
                        f.z+radii[ring]*cb};
                    const V u{f.x+radii[ring+1]*sa,f.y+heights[ring+1],
                        f.z+radii[ring+1]*ca};
                    const V v{f.x+radii[ring+1]*sb,f.y+heights[ring+1],
                        f.z+radii[ring+1]*cb};
                    Quad(output,p,q,v,u,clay);
                }
            }
            for (int i=0;i<sides;++i)
            {
                const float a=2.0f*Pi*i/sides;
                const float b=2.0f*Pi*(i+1)/sides;
                const V outerA{f.x+0.43f*std::sin(a),f.y+2.03f,
                    f.z+0.43f*std::cos(a)};
                const V outerB{f.x+0.43f*std::sin(b),f.y+2.03f,
                    f.z+0.43f*std::cos(b)};
                const V innerA{f.x+0.29f*std::sin(a),f.y+2.015f,
                    f.z+0.29f*std::cos(a)};
                const V innerB{f.x+0.29f*std::sin(b),f.y+2.015f,
                    f.z+0.29f*std::cos(b)};
                Quad(output,innerA,innerB,outerB,outerA,
                    {0.36f,0.23f,0.15f});
            }
            Box(output,{f.x-0.32f,f.y+0.12f,f.z+0.815f},
                {f.x+0.32f,f.y+0.53f,f.z+0.845f},
                {0.075f,0.058f,0.048f});
            for (int i=0;i<5;++i)
            {
                const float a=Pi*(0.1f+0.8f*i/4.0f);
                const float x=0.35f*std::cos(a);
                const float y=0.25f*std::sin(a);
                Box(output,{f.x+x-0.05f,f.y+0.45f+y,f.z+0.83f},
                    {f.x+x+0.05f,f.y+0.49f+y,f.z+0.91f},
                    {0.27f,0.18f,0.12f});
            }
            Box(output,{f.x-0.40f,f.y+0.01f,f.z+1.03f},
                {f.x+0.40f,f.y+0.13f,f.z+1.15f},
                {0.40f,0.32f,0.22f});
            if (f.moldCapacity != 0)
                CastingTray(output,f);
            const float radians = f.yawDegrees * Pi / 180.0f;
            const float c = std::cos(radians);
            const float s = std::sin(radians);
            for (std::size_t v = firstVertex; v < output.vertices.size(); ++v)
            {
                auto& vertex = output.vertices[v];
                const float dx = vertex.position[0] - f.x;
                const float dz = vertex.position[2] - f.z;
                vertex.position[0] = f.x + c * dx - s * dz;
                vertex.position[2] = f.z + s * dx + c * dz;
                const float nx = vertex.normal[0];
                const float nz = vertex.normal[2];
                vertex.normal[0] = c * nx - s * nz;
                vertex.normal[2] = s * nx + c * nz;
            }
        }
    }

    void ClayFurnaceVisuals::BuildEffects(
        const std::vector<ClayFurnaceState>& furnaces,
        float time, VoxelMesh& flames, VoxelMesh& smoke)
    {
        flames.Clear(); smoke.Clear();
        for (const auto& f : furnaces)
        {
            if (f.fuelSeconds <= 0.0f)
                continue;
            const std::size_t firstFlameVertex = flames.vertices.size();
            const std::size_t firstSmokeVertex = smoke.vertices.size();
            for (int i=0;i<5;++i)
            {
                const float t=time*5.0f+2.1f*i;
                const float x=f.x+0.12f*std::cos(i*2.4f);
                const float z=f.z+0.12f*std::sin(i*2.4f);
                const float h=0.21f+0.16f*(0.5f+0.5f*std::sin(t));
                Tri(flames,{x-0.10f,f.y+1.96f,z},
                    {x+0.10f,f.y+1.96f,z},
                    {x+0.08f*std::sin(t),f.y+2.02f+h,z},
                    {1.0f,0.55f,0.075f});
                Tri(flames,{x-0.17f,f.y+0.39f,z+0.86f},
                    {x+0.17f,f.y+0.39f,z+0.86f},
                    {x+0.08f*std::sin(t),f.y+0.59f+h,z+0.86f},
                    {1.0f,0.28f,0.025f});
            }
            for (int i=0;i<12;++i)
            {
                const float phase=std::fmod(time*0.33f+i*0.117f,1.0f);
                const float height=2.17f+phase*2.7f;
                const float spin=i*2.39996f+time*0.28f;
                const float r=0.07f+phase*0.35f;
                const float x=f.x+std::cos(spin)*r;
                const float z=f.z+std::sin(spin)*r;
                const float size=0.12f+phase*0.20f;
                const C gray{0.36f+phase*0.13f,0.37f+phase*0.13f,
                    0.38f+phase*0.13f};
                Quad(smoke,{x-size,f.y+height,z},
                    {x+size,f.y+height,z},
                    {x+size*0.8f,f.y+height+size*1.4f,z},
                    {x-size*0.8f,f.y+height+size*1.4f,z},gray);
                Quad(smoke,{x,f.y+height,z-size},
                    {x,f.y+height,z+size},
                    {x,f.y+height+size*1.4f,z+size*0.8f},
                    {x,f.y+height+size*1.4f,z-size*0.8f},gray);
            }
            const float radians = f.yawDegrees * Pi / 180.0f;
            const float c = std::cos(radians);
            const float si = std::sin(radians);
            for (auto [mesh,first] : {
                std::pair{&flames,firstFlameVertex},
                std::pair{&smoke,firstSmokeVertex}})
            {
                for (std::size_t v = first; v < mesh->vertices.size(); ++v)
                {
                    auto& vertex = mesh->vertices[v];
                    const float dx = vertex.position[0] - f.x;
                    const float dz = vertex.position[2] - f.z;
                    vertex.position[0] = f.x + c * dx - si * dz;
                    vertex.position[2] = f.z + si * dx + c * dz;
                    const float nx = vertex.normal[0];
                    const float nz = vertex.normal[2];
                    vertex.normal[0] = c * nx - si * nz;
                    vertex.normal[2] = si * nx + c * nz;
                }
            }
        }
    }

    void ClayFurnaceVisuals::BuildHeldMold(float x,float y,float z,
        float forwardX,float forwardY,float forwardZ,
        bool hot,VoxelMesh& mesh)
    {
        mesh.Clear();
        const float horizontal=std::sqrt(forwardX*forwardX+forwardZ*forwardZ);
        if (horizontal < 0.001f) return;
        const float dx=forwardX/horizontal;
        const float dz=forwardZ/horizontal;
        const float rx=-dz;
        const float rz=dx;
        const float ox=x+forwardX*0.9f+rx*0.36f;
        const float oy=y+forwardY*0.9f-0.39f;
        const float oz=z+forwardZ*0.9f+rz*0.36f;
        Mold(mesh,ox,oy,oz,hot);
    }
}
