#include "industrial_frontier/renderer/OreCrusherVisuals.hpp"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <string_view>

namespace industrial_frontier
{
    namespace
    {
        constexpr float Pi = 3.14159265358979323846f;
        struct P { float x, y, z; };
        struct C { float r, g, b; };
        constexpr C Stone{0.43f,0.42f,0.39f};
        constexpr C Rim{0.31f,0.30f,0.28f};
        constexpr C Wheel{0.53f,0.52f,0.48f};
        constexpr C Wood{0.39f,0.23f,0.12f};
        constexpr C Iron{0.18f,0.19f,0.19f};

        void Tri(VoxelMesh& mesh, P a, P b, P c, C color)
        {
            const P u{b.x-a.x,b.y-a.y,b.z-a.z};
            const P v{c.x-a.x,c.y-a.y,c.z-a.z};
            P n{u.y*v.z-u.z*v.y,u.z*v.x-u.x*v.z,u.x*v.y-u.y*v.x};
            const float length=std::sqrt(n.x*n.x+n.y*n.y+n.z*n.z);
            if (length < 0.00001f) return;
            n={n.x/length,n.y/length,n.z/length};
            const auto base=static_cast<unsigned>(mesh.vertices.size());
            for (const P p : {a,b,c})
            {
                TerrainVertex vertex{};
                vertex.position[0]=p.x; vertex.position[1]=p.y;
                vertex.position[2]=p.z;
                vertex.normal[0]=n.x; vertex.normal[1]=n.y;
                vertex.normal[2]=n.z;
                vertex.color[0]=color.r; vertex.color[1]=color.g;
                vertex.color[2]=color.b;
                mesh.vertices.push_back(vertex);
            }
            for (unsigned j=0;j<3;++j) mesh.indices.push_back(base+j);
        }

        void Quad(VoxelMesh& mesh,P a,P b,P c,P d,C color)
        {
            Tri(mesh,a,b,c,color);
            Tri(mesh,a,c,d,color);
        }

        void Box(VoxelMesh& mesh,P lo,P hi,C color)
        {
            Quad(mesh,{lo.x,lo.y,lo.z},{hi.x,lo.y,lo.z},{hi.x,hi.y,lo.z},{lo.x,hi.y,lo.z},color);
            Quad(mesh,{hi.x,lo.y,hi.z},{lo.x,lo.y,hi.z},{lo.x,hi.y,hi.z},{hi.x,hi.y,hi.z},color);
            Quad(mesh,{lo.x,lo.y,hi.z},{lo.x,lo.y,lo.z},{lo.x,hi.y,lo.z},{lo.x,hi.y,hi.z},color);
            Quad(mesh,{hi.x,lo.y,lo.z},{hi.x,lo.y,hi.z},{hi.x,hi.y,hi.z},{hi.x,hi.y,lo.z},color);
            Quad(mesh,{lo.x,hi.y,lo.z},{hi.x,hi.y,lo.z},{hi.x,hi.y,hi.z},{lo.x,hi.y,hi.z},color);
            Quad(mesh,{lo.x,lo.y,hi.z},{hi.x,lo.y,hi.z},{hi.x,lo.y,lo.z},{lo.x,lo.y,lo.z},color);
        }

        void Cylinder(VoxelMesh& mesh, P center, float radius, float bottom,
            float top, C color, int sides = 16)
        {
            for (int i=0;i<sides;++i)
            {
                const float a=2.0f*Pi*static_cast<float>(i)/sides;
                const float b=2.0f*Pi*static_cast<float>(i+1)/sides;
                const P p{center.x+radius*std::cos(a),0.0f,center.z+radius*std::sin(a)};
                const P q{center.x+radius*std::cos(b),0.0f,center.z+radius*std::sin(b)};
                Quad(mesh,{p.x,bottom,p.z},{q.x,bottom,q.z},
                    {q.x,top,q.z},{p.x,top,p.z},color);
                Tri(mesh,{center.x,top,center.z},{p.x,top,p.z},{q.x,top,q.z},color);
                Tri(mesh,{center.x,bottom,center.z},{q.x,bottom,q.z},{p.x,bottom,p.z},color);
            }
        }

        void RimRing(VoxelMesh& mesh)
        {
            constexpr int sides=32;
            for (int i=0;i<sides;++i)
            {
                const float a=2.0f*Pi*static_cast<float>(i)/sides;
                const float b=2.0f*Pi*static_cast<float>(i+1)/sides;
                const auto point=[](float angle,float r,float y) -> P
                { return {r*std::cos(angle),y,r*std::sin(angle)}; };
                const C side=i%4==0?Stone:Rim;
                Quad(mesh,point(a,1.45f,0.12f),point(b,1.45f,0.12f),
                    point(b,1.45f,0.73f),point(a,1.45f,0.73f),side);
                Quad(mesh,point(b,1.16f,0.12f),point(a,1.16f,0.12f),
                    point(a,1.16f,0.73f),point(b,1.16f,0.73f),side);
                Quad(mesh,point(a,1.16f,0.73f),point(b,1.16f,0.73f),
                    point(b,1.45f,0.73f),point(a,1.45f,0.73f),Stone);
            }
        }

        void RollingStone(VoxelMesh& mesh,float rotation)
        {
            const float r=0.80f;
            const float angle=rotation;
            const float cx=r*std::cos(angle),cz=r*std::sin(angle);
            const float tx=-std::sin(angle),tz=std::cos(angle);
            const float ax=std::cos(angle),az=std::sin(angle);
            constexpr int slices=20;
            for (int i=0;i<slices;++i)
            {
                const float u=2.0f*Pi*static_cast<float>(i)/slices+
                    rotation*1.65f;
                const float v=2.0f*Pi*static_cast<float>(i+1)/slices+
                    rotation*1.65f;
                const auto point=[&](float t,float thickness) -> P
                {
                    return {cx+ax*thickness+tx*0.44f*std::cos(t),
                        0.89f+0.51f*std::sin(t),
                        cz+az*thickness+tz*0.44f*std::cos(t)};
                };
                const C stone=i%5==0?Rim:Wheel;
                Quad(mesh,point(u,-0.21f),point(v,-0.21f),
                    point(v,0.21f),point(u,0.21f),stone);
                Tri(mesh,{cx-ax*0.21f,0.89f,cz-az*0.21f},
                    point(v,-0.21f),point(u,-0.21f),stone);
                Tri(mesh,{cx+ax*0.21f,0.89f,cz+az*0.21f},
                    point(u,0.21f),point(v,0.21f),stone);
            }
            Box(mesh,{-0.13f,1.19f,-0.13f},{0.13f,1.66f,0.13f},Wood);
            const float minX=std::min(0.0f,cx),maxX=std::max(0.0f,cx);
            const float minZ=std::min(0.0f,cz),maxZ=std::max(0.0f,cz);
            Box(mesh,{minX-0.12f,1.51f,minZ-0.12f},
                {maxX+0.12f,1.70f,maxZ+0.12f},Wood);
            Box(mesh,{-0.14f,1.64f,-0.14f},{0.14f,1.78f,0.14f},Iron);
            Box(mesh,{-0.11f,1.75f,-0.11f},{0.11f,1.89f,1.74f},Wood);
            Box(mesh,{-0.12f,1.79f,1.58f},{0.12f,2.23f,1.79f},Wood);
        }

        C OreColor(std::string_view mineral)
        {
            if (mineral=="malachite" || mineral=="chrysocolla")
                return {0.22f,0.47f,0.29f};
            if (mineral=="azurite")
                return {0.16f,0.29f,0.59f};
            if (mineral=="cuprite" || mineral=="hematite")
                return {0.46f,0.22f,0.19f};
            if (mineral=="chalcopyrite" || mineral=="bornite")
                return {0.56f,0.42f,0.22f};
            if (mineral=="cassiterite" || mineral=="siderite")
                return {0.31f,0.27f,0.21f};
            if (mineral=="galena" || mineral=="magnetite")
                return {0.23f,0.29f,0.34f};
            if (mineral=="limonite" || mineral=="goethite")
                return {0.57f,0.37f,0.19f};
            if (mineral=="sulfur" || mineral=="native_sulfur")
                return {0.69f,0.62f,0.25f};
            return {0.39f,0.38f,0.34f};
        }

        void OrePieces(VoxelMesh& mesh,const OreCrusherState& crusher)
        {
            if (crusher.batch.massKg <= 0.0f)
                return;
            const float progress=crusher.workRequired>0.0f ?
                crusher.workDone/crusher.workRequired : 0.0f;
            const float scale=std::clamp(crusher.batch.massKg/OreCrusherWorld::CapacityKg,0.1f,1.0f);
            const int count=static_cast<int>(7+scale*19);
            const bool done=crusher.batch.processingStage==OreProcessingStage::Crushed;
            for (int i=0;i<count;++i)
            {
                const float angle=2.39996323f*static_cast<float>(i);
                const float radius=0.50f+0.47f*static_cast<float>((i*17)%13)/13.0f;
                const float x=radius*std::cos(angle),z=radius*std::sin(angle);
                const float size=(done?0.047f:0.12f)*(1.0f-0.52f*progress)+
                    0.018f*static_cast<float>(i%4);
                const C base=OreColor(crusher.batch.primaryMineralId);
                const float shade=0.78f+0.09f*static_cast<float>(i%5);
                const C color{std::min(1.0f,base.r*shade),
                    std::min(1.0f,base.g*shade),
                    std::min(1.0f,base.b*shade)};
                Box(mesh,{x-size,0.23f,z-size},
                    {x+size,0.23f+size*0.85f,z+size},color);
            }
        }

        void BuildOne(const OreCrusherState& crusher,VoxelMesh& mesh)
        {
            const auto first=mesh.vertices.size();
            Cylinder(mesh,{0.0f,0.0f,0.0f},1.28f,0.04f,0.23f,Stone,32);
            RimRing(mesh);
            OrePieces(mesh,crusher);
            RollingStone(mesh,crusher.rotationRadians);
            const float radians=crusher.yawDegrees*Pi/180.0f;
            const float c=std::cos(radians),s=std::sin(radians);
            for (std::size_t i=first;i<mesh.vertices.size();++i)
            {
                auto& vertex=mesh.vertices[i];
                const float x=vertex.position[0],z=vertex.position[2];
                vertex.position[0]=crusher.x+c*x-s*z;
                vertex.position[1]+=crusher.y;
                vertex.position[2]=crusher.z+s*x+c*z;
                const float nx=vertex.normal[0],nz=vertex.normal[2];
                vertex.normal[0]=c*nx-s*nz;
                vertex.normal[2]=s*nx+c*nz;
            }
        }
    }

    void OreCrusherVisuals::Build(const std::vector<OreCrusherState>& crushers,
        VoxelMesh& mesh)
    {
        mesh.Clear();
        for (const auto& crusher : crushers)
            BuildOne(crusher,mesh);
    }
}
