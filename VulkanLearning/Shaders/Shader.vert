#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform UniformBufferObject
{
    mat4 Model;
    mat4 View;
    mat4 Projection;
} Transformation;

layout(location = 0) in vec2 Position;
layout(location = 1) in vec3 Color;

layout(location = 0) out vec3 FragColor;

void main()
{
    gl_Position = Transformation.Projection * Transformation.View * Transformation.Model * vec4(Position, 0.0, 1.0);
    FragColor = Color;
}