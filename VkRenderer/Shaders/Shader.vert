#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform MvpUniformBufferObject
{
    mat4 Model;
    mat4 View;
    mat4 Projection;
} Transformation;

layout(location = 0) in vec3 Position;
layout(location = 1) in vec3 Color;
layout(location = 2) in vec3 Normal;
layout(location = 3) in vec2 TexCoord;

layout(location = 0) out vec4 FragPosition;
layout(location = 1) out vec3 FragColor;
layout(location = 2) out vec3 FragNormal;
layout(location = 3) out vec2 FragTexCoord;
layout(location = 4) out vec3 FragPositionW;

void main()
{
    FragPosition = Transformation.Projection * Transformation.View * Transformation.Model * vec4(Position, 1.0);
    FragColor = Color;
    FragNormal = mat3(Transformation.Model) * Normal;
    FragTexCoord = TexCoord;
    FragPositionW = (Transformation.Model * vec4(Position, 1.0)).xyz;

    gl_Position = FragPosition;
}

