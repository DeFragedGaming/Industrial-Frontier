#include "industrial_frontier/renderer/PrimitiveMachineVisuals.hpp"

#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace industrial_frontier
{
    namespace
    {
        constexpr float Pi=3.14159265358979323846f;
        struct P {float x,y,z;};
        struct C {float r,g,b;};
        constexpr C Stone{0.48f,0.47f,0.43f};
        constexpr C DarkStone{0.31f,0.30f,0.27f};
        constexpr C Wood{0.40f,0.26f,0.14f};
        constexpr C Rim{0.24f,0.16f,0.09f};
        constexpr C Ore{0.52f,0.43f,0.29f};
        constexpr C Metal{0.22f,0.23f,0.24f};

        void Triangle(VoxelMesh& mesh,P a,P b,P c,C color)
        {
            const P u{b.x-a.x,b.y-a.y,b.z-a.z},v{c.x-a.x,c.y-a.y,c.z-a.z};
            P n{u.y*v.z-u.z*v.y,u.z*v.x-u.x*v.z,u.x*v.y-u.y*v.x};
            const float l=std::sqrt(n.x*n.x+n.y*n.y+n.z*n.z);
            if(l<0.00001f)return;
            n={n.x/l,n.y/l,n.z/l};
            const unsigned start=static_cast<unsigned>(mesh.vertices.size());
            for(const P p:{a,b,c})
            {
                TerrainVertex vertex{};
                vertex.position[0]=p.x;vertex.position[1]=p.y;vertex.position[2]=p.z;
                vertex.normal[0]=n.x;vertex.normal[1]=n.y;vertex.normal[2]=n.z;
                vertex.color[0]=color.r;vertex.color[1]=color.g;vertex.color[2]=color.b;
                mesh.vertices.push_back(vertex);
                mesh.indices.push_back(static_cast<unsigned>(mesh.vertices.size()-1));
            }
            (void)start;
        }

        void Quad(VoxelMesh& mesh,P a,P b,P c,P d,C color)
        {
            Triangle(mesh,a,b,c,color);Triangle(mesh,a,c,d,color);
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

        void Disk(VoxelMesh& mesh,float radius,float bottom,float top,C color)
        {
            constexpr int sides=28;
            for(int i=0;i<sides;++i)
            {
                const float a=2.0f*Pi*i/sides,b=2.0f*Pi*(i+1)/sides;
                const P p{radius*std::cos(a),0.0f,radius*std::sin(a)};
                const P q{radius*std::cos(b),0.0f,radius*std::sin(b)};
                Quad(mesh,{p.x,bottom,p.z},{q.x,bottom,q.z},
                    {q.x,top,q.z},{p.x,top,p.z},color);
                Triangle(mesh,{0,top,0},{p.x,top,p.z},{q.x,top,q.z},color);
            }
        }

        void Mill(VoxelMesh& mesh,const PrimitiveMachineState& state)
        {
            for(int i=0;i<4;++i)
            {
                const float a=2.0f*Pi*i/4.0f;
                Box(mesh,{0.88f*std::cos(a)-0.15f,0.01f,0.88f*std::sin(a)-0.15f},
                    {0.88f*std::cos(a)+0.15f,0.76f,0.88f*std::sin(a)+0.15f},Wood);
            }
            Disk(mesh,1.19f,0.62f,0.85f,DarkStone);
            Disk(mesh,1.02f,0.85f,1.02f,Stone);
            if(state.feed.massKg>0.0f)
                Disk(mesh,0.81f,1.02f,1.07f,Ore);
            Disk(mesh,0.90f,1.08f,1.36f,Stone);
            Disk(mesh,0.18f,1.36f,1.43f,Metal);
            Box(mesh,{-0.10f,1.44f,-0.10f},{0.10f,1.60f,0.10f},Wood);
            const float angle=state.rotationRadians;
            const float hx=0.78f*std::cos(angle),hz=0.78f*std::sin(angle);
            Box(mesh,{std::min(0.0f,hx)-0.08f,1.54f,std::min(0.0f,hz)-0.08f},
                {std::max(0.0f,hx)+0.08f,1.68f,std::max(0.0f,hz)+0.08f},Wood);
            Box(mesh,{hx-0.09f,1.62f,hz-0.09f},{hx+0.09f,2.10f,hz+0.09f},Wood);
            for (int i=0;i<8;++i)
            {
                const float a=2.0f*Pi*i/8.0f;
                Box(mesh,{0.72f*std::cos(a)-0.07f,0.42f,0.72f*std::sin(a)-0.07f},
                    {0.72f*std::cos(a)+0.07f,0.66f,0.72f*std::sin(a)+0.07f},Rim);
            }
        }

        void Sluice(VoxelMesh& mesh,const PrimitiveMachineState& state)
        {
            Box(mesh,{-0.83f,-0.22f,-1.75f},{0.83f,-0.05f,1.75f},Wood);
            Box(mesh,{-0.89f,-0.15f,-1.75f},{-0.69f,0.43f,1.75f},Rim);
            Box(mesh,{0.69f,-0.15f,-1.75f},{0.89f,0.43f,1.75f},Rim);
            for(int i=0;i<7;++i)
            {
                const float z=-1.45f+i*0.46f;
                Box(mesh,{-0.72f,-0.05f,z},{0.72f,0.17f,z+0.13f},Stone);
            }
            Box(mesh,{-0.93f,-0.35f,-1.72f},{-0.73f,-0.08f,1.69f},Wood);
            Box(mesh,{0.73f,-0.35f,-1.72f},{0.93f,-0.08f,1.69f},Wood);
            if (state.feed.massKg>0.0f)
            {
                const float fraction=std::clamp(state.feed.massKg/20.0f,0.2f,1.0f);
                Box(mesh,{-0.59f,0.0f,-0.96f},{0.59f,0.05f+fraction*0.1f,0.28f},Ore);
            }
            for (int i=0;i<4;++i)
            {
                const float z=-1.2f+i*0.79f;
                const float glint=0.18f+0.06f*std::sin(state.rotationRadians+i);
                Box(mesh,{-0.58f,0.07f,z},{0.58f,0.078f,z+0.055f},
                    {glint,0.44f,0.53f});
            }
        }
    }

    void PrimitiveMachineVisuals::Build(const std::vector<PrimitiveMachineState>& machines,
        VoxelMesh& mesh)
    {
        mesh.Clear();
        for(const auto& state:machines)
        {
            VoxelMesh local;
            if(state.kind==PrimitiveMachineKind::Mill) Mill(local,state);
            else Sluice(local,state);
            const float r=state.yawDegrees*Pi/180.0f;
            const float co=std::cos(r),si=std::sin(r);
            const auto offset=static_cast<unsigned>(mesh.vertices.size());
            for(auto& v:local.vertices)
            {
                const float x=v.position[0],z=v.position[2];
                v.position[0]=state.x+co*x-si*z;
                v.position[1]+=state.y;
                v.position[2]=state.z+si*x+co*z;
                const float nx=v.normal[0],nz=v.normal[2];
                v.normal[0]=co*nx-si*nz;
                v.normal[2]=si*nx+co*nz;
                mesh.vertices.push_back(v);
            }
            for(auto index:local.indices) mesh.indices.push_back(offset+index);
        }
    }
}
