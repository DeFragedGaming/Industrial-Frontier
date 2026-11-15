#include "industrial_frontier/renderer/BuildStructureVisuals.hpp"

#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace industrial_frontier
{
    namespace
    {
        constexpr float Pi = 3.14159265358979323846f;
        struct P { float x, y, z; };
        struct C { float r, g, b; };
        constexpr C Wood{0.43f, 0.275f, 0.145f};
        constexpr C Bark{0.325f, 0.195f, 0.093f};
        constexpr C WoodEnd{0.65f, 0.46f, 0.26f};
        constexpr C Iron{0.155f, 0.16f, 0.17f};

        void Tri(VoxelMesh& mesh, P a, P b, P c, C color)
        {
            const P u{b.x-a.x, b.y-a.y, b.z-a.z};
            const P v{c.x-a.x, c.y-a.y, c.z-a.z};
            P n{u.y*v.z-u.z*v.y, u.z*v.x-u.x*v.z, u.x*v.y-u.y*v.x};
            const float length = std::sqrt(n.x*n.x+n.y*n.y+n.z*n.z);
            if (length < 0.00001f)
                return;
            n = {n.x/length, n.y/length, n.z/length};
            const auto base = static_cast<unsigned>(mesh.vertices.size());
            for (const P p : {a,b,c})
            {
                TerrainVertex vertex{};
                vertex.position[0] = p.x;
                vertex.position[1] = p.y;
                vertex.position[2] = p.z;
                vertex.normal[0] = n.x;
                vertex.normal[1] = n.y;
                vertex.normal[2] = n.z;
                vertex.color[0] = color.r;
                vertex.color[1] = color.g;
                vertex.color[2] = color.b;
                mesh.vertices.push_back(vertex);
            }
            for (unsigned i = 0; i < 3; ++i)
                mesh.indices.push_back(base+i);
        }

        void Quad(VoxelMesh& mesh, P a, P b, P c, P d, C color)
        {
            Tri(mesh,a,b,c,color);
            Tri(mesh,a,c,d,color);
        }

        void Box(VoxelMesh& mesh, P a, P b, C color)
        {
            Quad(mesh,{a.x,a.y,a.z},{b.x,a.y,a.z},{b.x,b.y,a.z},{a.x,b.y,a.z},color);
            Quad(mesh,{b.x,a.y,b.z},{a.x,a.y,b.z},{a.x,b.y,b.z},{b.x,b.y,b.z},color);
            Quad(mesh,{a.x,a.y,b.z},{a.x,a.y,a.z},{a.x,b.y,a.z},{a.x,b.y,b.z},color);
            Quad(mesh,{b.x,a.y,a.z},{b.x,a.y,b.z},{b.x,b.y,b.z},{b.x,b.y,a.z},color);
            Quad(mesh,{a.x,b.y,a.z},{b.x,b.y,a.z},{b.x,b.y,b.z},{a.x,b.y,b.z},color);
            Quad(mesh,{a.x,a.y,b.z},{b.x,a.y,b.z},{b.x,a.y,a.z},{a.x,a.y,a.z},color);
        }

        void Beam(VoxelMesh& mesh, P a, P b, float radius, C color)
        {
            const float dx = b.x-a.x;
            const float dy = b.y-a.y;
            const float dz = b.z-a.z;
            if (std::abs(dx) >= std::abs(dy) && std::abs(dx) >= std::abs(dz))
                Box(mesh,{std::min(a.x,b.x),a.y-radius,a.z-radius},
                    {std::max(a.x,b.x),a.y+radius,a.z+radius},color);
            else if (std::abs(dz) >= std::abs(dy))
                Box(mesh,{a.x-radius,a.y-radius,std::min(a.z,b.z)},
                    {a.x+radius,a.y+radius,std::max(a.z,b.z)},color);
            else
                Box(mesh,{a.x-radius,std::min(a.y,b.y),a.z-radius},
                    {a.x+radius,std::max(a.y,b.y),a.z+radius},color);
        }

        void BuildOne(const BuildStructure& s, VoxelMesh& mesh)
        {
            const auto first = mesh.vertices.size();
            switch (s.kind)
            {
            case BuildKind::Chest:
                Box(mesh,{-0.66f,0.04f,-0.43f},{0.66f,0.78f,0.43f},Wood);
                for (float x : {-0.55f,0.53f})
                    Box(mesh,{x,0.03f,-0.46f},{x+0.08f,0.88f,0.46f},Bark);
                Box(mesh,{-0.70f,0.78f,-0.47f},{0.70f,0.97f,0.47f},WoodEnd);
                for (float x : {-0.48f,0.47f})
                    Box(mesh,{x,0.78f,-0.50f},{x+0.045f,0.99f,0.50f},Iron);
                Box(mesh,{-0.12f,0.61f,0.44f},{0.12f,0.82f,0.50f},Iron);
                break;
            case BuildKind::Foundation:
                for (int i=0; i<9; ++i)
                {
                    const float z = -1.38f + 0.345f*static_cast<float>(i);
                    Beam(mesh,{-1.52f,0.24f,z},{1.52f,0.24f,z},0.15f,
                        (i%2==0)?Wood:Bark);
                }
                for (float z : {-1.43f,1.43f})
                    Beam(mesh,{-1.54f,0.14f,z},{1.54f,0.14f,z},0.17f,WoodEnd);
                break;
            case BuildKind::Wall:
                for (int i=0; i<10; ++i)
                {
                    const float y = 0.36f + 0.255f*static_cast<float>(i);
                    Beam(mesh,{-1.51f,y,0.0f},{1.51f,y,0.0f},0.14f,
                        i%3==0?Wood:Bark);
                }
                break;
            case BuildKind::Doorframe:
                for (float x : {-1.43f,1.43f,-0.70f,0.70f})
                    Beam(mesh,{x,0.28f,0.0f},{x,2.92f,0.0f},0.15f,Bark);
                for (float y : {2.65f,2.87f})
                    Beam(mesh,{-1.52f,y,0.0f},{1.52f,y,0.0f},0.15f,Wood);
                for (int i=0;i<9;++i)
                {
                    const float y=0.34f+i*0.25f;
                    for (float x : {-1.06f,1.06f})
                        Beam(mesh,{x-0.37f,y,0.0f},{x+0.37f,y,0.0f},0.14f,Wood);
                }
                break;
            case BuildKind::WindowWall:
                for (int i=0;i<10;++i)
                {
                    const float y=0.34f+i*0.25f;
                    if (i<3 || i>7)
                        Beam(mesh,{-1.52f,y,0.0f},{1.52f,y,0.0f},0.14f,Wood);
                    else
                    {
                        Beam(mesh,{-1.52f,y,0.0f},{-0.63f,y,0.0f},0.14f,Bark);
                        Beam(mesh,{0.63f,y,0.0f},{1.52f,y,0.0f},0.14f,Bark);
                    }
                }
                for (float x : {-0.63f,0.63f})
                    Beam(mesh,{x,1.03f,0.0f},{x,2.34f,0.0f},0.09f,WoodEnd);
                break;
            case BuildKind::Roof:
                for (int i=0;i<9;++i)
                {
                    const float z0=-1.60f+i*0.40f;
                    const float z1=z0+0.40f;
                    const C wood=i%2==0?Wood:Bark;
                    Quad(mesh,{-1.62f,3.03f,z0},{0.0f,3.97f,z0},
                        {0.0f,3.97f,z1},{-1.62f,3.03f,z1},wood);
                    Quad(mesh,{0.0f,3.97f,z0},{1.62f,3.03f,z0},
                        {1.62f,3.03f,z1},{0.0f,3.97f,z1},wood);
                    Quad(mesh,{-1.62f,3.015f,z1},{0.0f,3.955f,z1},
                        {0.0f,3.955f,z0},{-1.62f,3.015f,z0},Bark);
                    Quad(mesh,{0.0f,3.955f,z1},{1.62f,3.015f,z1},
                        {1.62f,3.015f,z0},{0.0f,3.955f,z0},Bark);
                }
                Beam(mesh,{0.0f,3.97f,-1.72f},{0.0f,3.97f,1.72f},0.125f,WoodEnd);
                break;
            case BuildKind::Support:
                Beam(mesh,{0.0f,0.06f,0.0f},{0.0f,3.20f,0.0f},0.20f,Bark);
                Beam(mesh,{-0.27f,0.21f,0.0f},{0.27f,0.21f,0.0f},0.12f,WoodEnd);
                break;
            }
            const float rad = s.yawDegrees * Pi / 180.0f;
            const float c=std::cos(rad);
            const float si=std::sin(rad);
            for (std::size_t i=first; i<mesh.vertices.size();++i)
            {
                auto& v=mesh.vertices[i];
                const float x=v.position[0], z=v.position[2];
                v.position[0]=s.x+c*x-si*z;
                v.position[1]+=s.y;
                v.position[2]=s.z+si*x+c*z;
                const float nx=v.normal[0], nz=v.normal[2];
                v.normal[0]=c*nx-si*nz;
                v.normal[2]=si*nx+c*nz;
            }
        }
    }

    void BuildStructureVisuals::Build(
        const std::vector<BuildStructure>& structures, VoxelMesh& mesh)
    {
        mesh.Clear();
        for (const auto& s : structures)
            BuildOne(s,mesh);
    }
}
