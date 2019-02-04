#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform MvpUniformBufferObject
{
    mat4 Model;
    mat4 ModelInvTranspose;
    mat4 View;
    mat4 Projection;
} Transformation;

layout(location = 0) in vec3 Position;
layout(location = 1) in vec3 Color;
layout(location = 2) in vec3 Normal;
layout(location = 3) in vec3 Tangent;
layout(location = 4) in vec2 TexCoord;

layout(location = 0) out vec4 FragPositionH;
layout(location = 1) out vec3 FragColor;
layout(location = 2) out vec2 FragTexCoord;
layout(location = 3) out vec3 FragPositionW;
layout(location = 4) out vec3 FragNormalW;
layout(location = 5) out vec3 FragTangentW;

void main()
{
    FragPositionH = Transformation.Projection * Transformation.View * Transformation.Model * vec4(Position, 1.0);
    FragColor = Color;
    FragTexCoord = TexCoord;
    FragPositionW = (Transformation.Model * vec4(Position, 1.0)).xyz;
    FragNormalW = (Transformation.ModelInvTranspose * vec4(Normal, 1.0)).xyz;
    FragTangentW = (Transformation.Model * vec4(Tangent, 1.0)).xyz;
    gl_Position = FragPositionH;
}

