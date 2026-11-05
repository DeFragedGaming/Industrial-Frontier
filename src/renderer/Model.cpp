
#include "industrial_frontier/renderer/Model.hpp"
#include "industrial_frontier/renderer/Shader.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace industrial_frontier
{
    namespace
    {
        constexpr float Pi = 3.14159265358979323846f;

        struct Vertex
        {
            float position[3]{};
            float normal[3]{};
            float uv[2]{};
        };

        struct Matrix
        {
            float v[16]{};
        };

        Matrix Identity()
        {
            Matrix m{};
            m.v[0] = 1.0f;
            m.v[5] = 1.0f;
            m.v[10] = 1.0f;
            m.v[15] = 1.0f;
            return m;
        }

        Matrix Multiply(const Matrix& a, const Matrix& b)
        {
            Matrix result{};

            for (int c = 0; c < 4; ++c)
            {
                for (int r = 0; r < 4; ++r)
                {
                    for (int k = 0; k < 4; ++k)
                    {
                        result.v[c * 4 + r] +=
                            a.v[k * 4 + r] *
                            b.v[c * 4 + k];
                    }
                }
            }

            return result;
        }

        Matrix Translation(float x, float y, float z)
        {
            Matrix m = Identity();
            m.v[12] = x;
            m.v[13] = y;
            m.v[14] = z;
            return m;
        }

        Matrix RotationY(float radians)
        {
            Matrix m = Identity();

            const float c = std::cos(radians);
            const float s = std::sin(radians);

            m.v[0] = c;
            m.v[2] = -s;
            m.v[8] = s;
            m.v[10] = c;

            return m;
        }

        Matrix Scale(float value)
        {
            Matrix m = Identity();
            m.v[0] = value;
            m.v[5] = value;
            m.v[10] = value;
            return m;
        }

        template<typename T>
        bool LoadProc(T& proc, const char* name)
        {
            proc = reinterpret_cast<T>(
                SDL_GL_GetProcAddress(name)
            );

            if (!proc)
            {
                std::cerr << "Model OpenGL function missing: "
                          << name << '\n';
                return false;
            }

            return true;
        }

        using GenVertexArraysFn =
            void(APIENTRY*)(GLsizei, GLuint*);
        using BindVertexArrayFn =
            void(APIENTRY*)(GLuint);
        using DeleteVertexArraysFn =
            void(APIENTRY*)(GLsizei, const GLuint*);
        using GenBuffersFn =
            void(APIENTRY*)(GLsizei, GLuint*);
        using BindBufferFn =
            void(APIENTRY*)(GLenum, GLuint);
        using BufferDataFn =
            void(APIENTRY*)(GLenum, GLsizeiptr, const void*, GLenum);
        using DeleteBuffersFn =
            void(APIENTRY*)(GLsizei, const GLuint*);
        using EnableAttribFn =
            void(APIENTRY*)(GLuint);
        using AttribPointerFn =
            void(APIENTRY*)(GLuint, GLint, GLenum, GLboolean,
                            GLsizei, const void*);
        using DrawElementsFn =
            void(APIENTRY*)(GLenum, GLsizei, GLenum, const void*);
        using ActiveTextureFn =
            void(APIENTRY*)(GLenum);
        using GenerateMipmapFn =
            void(APIENTRY*)(GLenum);
    }

    struct Model::Impl
    {
        struct Mesh
        {
            GLuint vao = 0;
            GLuint vbo = 0;
            GLuint ebo = 0;
            GLuint texture = 0;
            GLsizei indexCount = 0;

            float color[4]{
                1.0f, 1.0f, 1.0f, 1.0f
            };
        };

        GenVertexArraysFn GenVertexArrays = nullptr;
        BindVertexArrayFn BindVertexArray = nullptr;
        DeleteVertexArraysFn DeleteVertexArrays = nullptr;

        GenBuffersFn GenBuffers = nullptr;
        BindBufferFn BindBuffer = nullptr;
        BufferDataFn BufferData = nullptr;
        DeleteBuffersFn DeleteBuffers = nullptr;

        EnableAttribFn EnableAttrib = nullptr;
        AttribPointerFn AttribPointer = nullptr;
        DrawElementsFn DrawElements = nullptr;
        ActiveTextureFn ActiveTexture = nullptr;
        GenerateMipmapFn GenerateMipmap = nullptr;

        Shader shader;
        std::vector<Mesh> meshes;

        float minX = 0.0f;
        float minY = 0.0f;
        float minZ = 0.0f;

        float maxX = 0.0f;
        float maxY = 0.0f;
        float maxZ = 0.0f;

        bool loaded = false;

        bool LoadFunctions()
        {
            bool ok = true;

            ok &= LoadProc(
                GenVertexArrays, "glGenVertexArrays"
            );
            ok &= LoadProc(
                BindVertexArray, "glBindVertexArray"
            );
            ok &= LoadProc(
                DeleteVertexArrays, "glDeleteVertexArrays"
            );

            ok &= LoadProc(GenBuffers, "glGenBuffers");
            ok &= LoadProc(BindBuffer, "glBindBuffer");
            ok &= LoadProc(BufferData, "glBufferData");
            ok &= LoadProc(DeleteBuffers, "glDeleteBuffers");

            ok &= LoadProc(
                EnableAttrib, "glEnableVertexAttribArray"
            );
            ok &= LoadProc(
                AttribPointer, "glVertexAttribPointer"
            );
            ok &= LoadProc(
                DrawElements, "glDrawElements"
            );
            ok &= LoadProc(
                ActiveTexture, "glActiveTexture"
            );
            ok &= LoadProc(
                GenerateMipmap, "glGenerateMipmap"
            );

            return ok;
        }

        void DeleteMesh(Mesh& mesh)
        {
            if (mesh.texture)
            {
                glDeleteTextures(1, &mesh.texture);
                mesh.texture = 0;
            }

            if (mesh.ebo)
            {
                DeleteBuffers(1, &mesh.ebo);
                mesh.ebo = 0;
            }

            if (mesh.vbo)
            {
                DeleteBuffers(1, &mesh.vbo);
                mesh.vbo = 0;
            }

            if (mesh.vao)
            {
                DeleteVertexArrays(1, &mesh.vao);
                mesh.vao = 0;
            }
        }

        void Clear()
        {
            if (DeleteBuffers && DeleteVertexArrays)
            {
                for (Mesh& mesh : meshes)
                {
                    DeleteMesh(mesh);
                }
            }

            meshes.clear();
            shader.Shutdown();
            loaded = false;
        }

        GLuint LoadTexture(const cgltf_texture* texture)
        {
            if (!texture || !texture->image)
            {
                return 0;
            }

            const cgltf_image* image = texture->image;

            if (!image->buffer_view ||
                !image->buffer_view->buffer ||
                !image->buffer_view->buffer->data)
            {
                return 0;
            }

            const cgltf_buffer_view* view =
                image->buffer_view;

            const auto* data =
                static_cast<const unsigned char*>(
                    view->buffer->data
                ) + view->offset;

            if (view->size >
                static_cast<cgltf_size>(
                    std::numeric_limits<int>::max()))
            {
                return 0;
            }

            int width = 0;
            int height = 0;
            int channels = 0;

            stbi_uc* pixels = stbi_load_from_memory(
                data,
                static_cast<int>(view->size),
                &width,
                &height,
                &channels,
                STBI_rgb_alpha
            );

            if (!pixels)
            {
                std::cerr << "Unable to decode model texture.\n";
                return 0;
            }

            GLuint id = 0;
            glGenTextures(1, &id);

            if (!id)
            {
                stbi_image_free(pixels);
                return 0;
            }

            glBindTexture(GL_TEXTURE_2D, id);

            glTexParameteri(
                GL_TEXTURE_2D,
                GL_TEXTURE_MIN_FILTER,
                GL_LINEAR_MIPMAP_LINEAR
            );

            glTexParameteri(
                GL_TEXTURE_2D,
                GL_TEXTURE_MAG_FILTER,
                GL_LINEAR
            );

            glTexParameteri(
                GL_TEXTURE_2D,
                GL_TEXTURE_WRAP_S,
                GL_REPEAT
            );

            glTexParameteri(
                GL_TEXTURE_2D,
                GL_TEXTURE_WRAP_T,
                GL_REPEAT
            );

            glTexImage2D(
                GL_TEXTURE_2D,
                0,
                GL_RGBA,
                width,
                height,
                0,
                GL_RGBA,
                GL_UNSIGNED_BYTE,
                pixels
            );

            GenerateMipmap(GL_TEXTURE_2D);

            glBindTexture(GL_TEXTURE_2D, 0);
            stbi_image_free(pixels);

            return id;
        }

        bool UploadPrimitive(
            const cgltf_primitive& primitive,
            const Matrix& worldTransform)
        {
            if (primitive.type !=
                cgltf_primitive_type_triangles)
            {
                return true;
            }

            const cgltf_accessor* positions = nullptr;
            const cgltf_accessor* normals = nullptr;
            const cgltf_accessor* texcoords = nullptr;

            for (cgltf_size a = 0;
                 a < primitive.attributes_count; ++a)
            {
                const cgltf_attribute& attribute =
                    primitive.attributes[a];

                if (attribute.type ==
                    cgltf_attribute_type_position)
                {
                    positions = attribute.data;
                }
                else if (attribute.type ==
                         cgltf_attribute_type_normal)
                {
                    normals = attribute.data;
                }
                else if (attribute.type ==
                         cgltf_attribute_type_texcoord &&
                         attribute.index == 0)
                {
                    texcoords = attribute.data;
                }
            }

            if (!positions || positions->count == 0)
            {
                return false;
            }

            std::vector<Vertex> vertices(
                static_cast<std::size_t>(positions->count)
            );

            for (cgltf_size i = 0; i < positions->count; ++i)
            {
                Vertex& vertex =
                    vertices[static_cast<std::size_t>(i)];

                float position[4]{};

                cgltf_accessor_read_float(
                    positions, i, position, 3
                );

                const float px = position[0];
                const float py = position[1];
                const float pz = position[2];

                vertex.position[0] =
                    worldTransform.v[0] * px +
                    worldTransform.v[4] * py +
                    worldTransform.v[8] * pz +
                    worldTransform.v[12];

                vertex.position[1] =
                    worldTransform.v[1] * px +
                    worldTransform.v[5] * py +
                    worldTransform.v[9] * pz +
                    worldTransform.v[13];

                vertex.position[2] =
                    worldTransform.v[2] * px +
                    worldTransform.v[6] * py +
                    worldTransform.v[10] * pz +
                    worldTransform.v[14];

                if (normals && i < normals->count)
                {
                    float normal[4]{};

                    cgltf_accessor_read_float(
                        normals, i, normal, 3
                    );

                    vertex.normal[0] =
                        worldTransform.v[0] * normal[0] +
                        worldTransform.v[4] * normal[1] +
                        worldTransform.v[8] * normal[2];

                    vertex.normal[1] =
                        worldTransform.v[1] * normal[0] +
                        worldTransform.v[5] * normal[1] +
                        worldTransform.v[9] * normal[2];

                    vertex.normal[2] =
                        worldTransform.v[2] * normal[0] +
                        worldTransform.v[6] * normal[1] +
                        worldTransform.v[10] * normal[2];

                    const float length = std::sqrt(
                        vertex.normal[0] * vertex.normal[0] +
                        vertex.normal[1] * vertex.normal[1] +
                        vertex.normal[2] * vertex.normal[2]
                    );

                    if (length > 0.00001f)
                    {
                        for (float& component : vertex.normal)
                        {
                            component /= length;
                        }
                    }
                }
                else
                {
                    vertex.normal[1] = 1.0f;
                }

                if (texcoords && i < texcoords->count)
                {
                    float uv[2]{};

                    cgltf_accessor_read_float(
                        texcoords, i, uv, 2
                    );

                    vertex.uv[0] = uv[0];
                    vertex.uv[1] = uv[1];
                }

                if (meshes.empty() && i == 0)
                {
                    minX = maxX = vertex.position[0];
                    minY = maxY = vertex.position[1];
                    minZ = maxZ = vertex.position[2];
                }

                minX = std::min(minX, vertex.position[0]);
                minY = std::min(minY, vertex.position[1]);
                minZ = std::min(minZ, vertex.position[2]);

                maxX = std::max(maxX, vertex.position[0]);
                maxY = std::max(maxY, vertex.position[1]);
                maxZ = std::max(maxZ, vertex.position[2]);
            }

            std::vector<unsigned int> indices;

            if (primitive.indices)
            {
                const cgltf_accessor* accessor =
                    primitive.indices;

                indices.reserve(
                    static_cast<std::size_t>(accessor->count)
                );

                for (cgltf_size i = 0; i < accessor->count; ++i)
                {
                    indices.push_back(
                        static_cast<unsigned int>(
                            cgltf_accessor_read_index(
                                accessor, i
                            )
                        )
                    );
                }
            }
            else
            {
                indices.reserve(vertices.size());

                for (std::size_t i = 0;
                     i < vertices.size(); ++i)
                {
                    indices.push_back(
                        static_cast<unsigned int>(i)
                    );
                }
            }

            if (indices.empty())
            {
                return false;
            }

            Mesh mesh{};

            if (primitive.material &&
                primitive.material->has_pbr_metallic_roughness)
            {
                const auto& pbr =
                    primitive.material->pbr_metallic_roughness;

                for (int i = 0; i < 4; ++i)
                {
                    mesh.color[i] =
                        pbr.base_color_factor[i];
                }

                mesh.texture = LoadTexture(
                    pbr.base_color_texture.texture
                );
            }

            GenVertexArrays(1, &mesh.vao);
            GenBuffers(1, &mesh.vbo);
            GenBuffers(1, &mesh.ebo);

            if (!mesh.vao || !mesh.vbo || !mesh.ebo)
            {
                DeleteMesh(mesh);
                return false;
            }

            BindVertexArray(mesh.vao);

            BindBuffer(GL_ARRAY_BUFFER, mesh.vbo);

            BufferData(
                GL_ARRAY_BUFFER,
                static_cast<GLsizeiptr>(
                    vertices.size() * sizeof(Vertex)
                ),
                vertices.data(),
                GL_STATIC_DRAW
            );

            BindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);

            BufferData(
                GL_ELEMENT_ARRAY_BUFFER,
                static_cast<GLsizeiptr>(
                    indices.size() * sizeof(unsigned int)
                ),
                indices.data(),
                GL_STATIC_DRAW
            );

            const GLsizei stride =
                static_cast<GLsizei>(sizeof(Vertex));

            EnableAttrib(0);

            AttribPointer(
                0, 3, GL_FLOAT, GL_FALSE, stride,
                reinterpret_cast<const void*>(
                    offsetof(Vertex, position)
                )
            );

            EnableAttrib(1);

            AttribPointer(
                1, 3, GL_FLOAT, GL_FALSE, stride,
                reinterpret_cast<const void*>(
                    offsetof(Vertex, normal)
                )
            );

            EnableAttrib(2);

            AttribPointer(
                2, 2, GL_FLOAT, GL_FALSE, stride,
                reinterpret_cast<const void*>(
                    offsetof(Vertex, uv)
                )
            );

            BindVertexArray(0);

            mesh.indexCount =
                static_cast<GLsizei>(indices.size());

            meshes.push_back(mesh);
            return true;
        }

        bool ProcessNode(const cgltf_node* node)
        {
            if (!node)
            {
                return true;
            }

            float transform[16]{};
            cgltf_node_transform_world(node, transform);

            Matrix world{};

            for (int i = 0; i < 16; ++i)
            {
                world.v[i] = transform[i];
            }

            if (node->mesh)
            {
                for (cgltf_size i = 0;
                     i < node->mesh->primitives_count; ++i)
                {
                    if (!UploadPrimitive(
                        node->mesh->primitives[i],
                        world
                    ))
                    {
                        return false;
                    }
                }
            }

            for (cgltf_size i = 0;
                 i < node->children_count; ++i)
            {
                if (!ProcessNode(node->children[i]))
                {
                    return false;
                }
            }

            return true;
        }
    };

    Model::Model()
        : m_impl(std::make_unique<Impl>())
    {
    }

    Model::~Model()
    {
        Shutdown();
    }

    bool Model::Load(const std::string& path)
    {
        if (!m_impl)
        {
            m_impl = std::make_unique<Impl>();
        }

        m_impl->Clear();

        if (!m_impl->LoadFunctions())
        {
            return false;
        }

        cgltf_options options{};
        cgltf_data* data = nullptr;

        const cgltf_result parseResult =
            cgltf_parse_file(
                &options,
                path.c_str(),
                &data
            );

        if (parseResult != cgltf_result_success || !data)
        {
            std::cerr << "Unable to parse GLB: "
                      << path << '\n';
            return false;
        }

        const cgltf_result bufferResult =
            cgltf_load_buffers(
                &options,
                data,
                path.c_str()
            );

        if (bufferResult != cgltf_result_success)
        {
            std::cerr << "Unable to load GLB buffers.\n";
            cgltf_free(data);
            return false;
        }

        if (!m_impl->shader.LoadFromFiles(
            "assets/shaders/model.vert",
            "assets/shaders/model.frag"
        ))
        {
            cgltf_free(data);
            return false;
        }

        bool success = true;

        const cgltf_scene* scene = data->scene;

        if (!scene && data->scenes_count > 0)
        {
            scene = &data->scenes[0];
        }

        if (scene)
        {
            for (cgltf_size i = 0;
                 i < scene->nodes_count; ++i)
            {
                if (!m_impl->ProcessNode(scene->nodes[i]))
                {
                    success = false;
                    break;
                }
            }
        }
        else
        {
            success = false;
        }

        cgltf_free(data);

        if (!success || m_impl->meshes.empty())
        {
            std::cerr << "GLB contains no usable scene meshes.\n";
            m_impl->Clear();
            return false;
        }

        m_impl->loaded = true;

        std::cout << "GLB model loaded: " << path << '\n';
        std::cout << "Model primitives: "
                  << m_impl->meshes.size() << '\n';

        return true;
    }

    void Model::Draw(
        const float* viewProjection,
        float x,
        float y,
        float z,
        float yawDegrees)
    {
        if (!m_impl || !m_impl->loaded ||
            !viewProjection)
        {
            return;
        }

        const float modelHeight =
            m_impl->maxY - m_impl->minY;

        const float scale = modelHeight > 0.0001f
            ? 1.75f / modelHeight
            : 1.0f;

        const float centerX =
            (m_impl->minX + m_impl->maxX) * 0.5f;

        const float centerZ =
            (m_impl->minZ + m_impl->maxZ) * 0.5f;

        const Matrix offset = Translation(
            -centerX,
            -m_impl->minY,
            -centerZ
        );

        const Matrix rotation = RotationY(
            yawDegrees * Pi / 180.0f
        );

        const Matrix model = Multiply(
            Translation(x, y, z),
            Multiply(
                rotation,
                Multiply(Scale(scale), offset)
            )
        );

        Matrix vp{};

        for (int i = 0; i < 16; ++i)
        {
            vp.v[i] = viewProjection[i];
        }

        m_impl->shader.Use();

        m_impl->shader.SetMat4(
            "uMVP", vp.v
        );

        m_impl->shader.SetMat4(
            "uModel", model.v
        );

        m_impl->shader.SetVec3(
            "uLightDirection",
            -0.45f, -1.0f, -0.35f
        );

        m_impl->shader.SetVec3(
            "uLightColor",
            1.0f, 0.96f, 0.88f
        );

        m_impl->shader.SetVec3(
            "uAmbientColor",
            0.42f, 0.48f, 0.58f
        );

        m_impl->shader.SetInt(
            "uBaseColorTexture", 0
        );

        glDisable(GL_CULL_FACE);

        for (const Impl::Mesh& mesh : m_impl->meshes)
        {
            m_impl->shader.SetBool(
                "uHasBaseColorTexture",
                mesh.texture != 0
            );

            m_impl->shader.SetVec3(
                "uUnused",
                0.0f, 0.0f, 0.0f
            );

            m_impl->ActiveTexture(GL_TEXTURE0);

            glBindTexture(
                GL_TEXTURE_2D,
                mesh.texture
            );

            m_impl->BindVertexArray(mesh.vao);

            m_impl->DrawElements(
                GL_TRIANGLES,
                mesh.indexCount,
                GL_UNSIGNED_INT,
                nullptr
            );
        }

        m_impl->BindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glEnable(GL_CULL_FACE);
    }

    bool Model::IsLoaded() const
    {
        return m_impl && m_impl->loaded;
    }

    void Model::Shutdown()
    {
        if (m_impl)
        {
            m_impl->Clear();
        }
    }
}
