#include "Upscaler.h"

#include "raygui.h"
#include "raylib.h"
#include <cmath>

static const char defaultVertexShader[] =
    R"VERTEX(#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec4 vertexColor;
out vec2 fragTexCoord;
out vec4 fragColor;

uniform mat4 mvp;
void main()
{
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    gl_Position = mvp*vec4(vertexPosition, 1.0);
})VERTEX";

Upscaler::Upscaler(const char *path)
    : shaderPath(path)
{
    reload();
}

Upscaler::~Upscaler()
{
    if (shader.id != 0)
    {
        UnloadShader(shader);
        shader = {};
    }
}

void Upscaler::unloadShader()
{
    if (shader.id != 0)
    {
        UnloadShader(shader);
        shader = {};
    }
}

void Upscaler::reload()
{
    char *shaderText = LoadFileText(shaderPath.c_str());
    Shader newShader = LoadShaderFromMemory(defaultVertexShader, shaderText);
    UnloadFileText(shaderText);
    if (newShader.id != 0)
    {
        if (shader.id != 0)
        {
            UnloadShader(shader);
        }
        shader = newShader;
    }
}

void Upscaler::addUniform(Upscaler::Uniform uniform)
{
    uniforms.push_back(uniform);
}

int Upscaler::getTextWidth() const
{
    int maxTextWidth = 0;
    for (const Uniform &uniform : uniforms)
    {
        int textWidth = MeasureText(uniform.name.c_str(), 10);
        if (textWidth > maxTextWidth)
        {
            maxTextWidth = textWidth;
        }
    }

    return maxTextWidth;
}

bool Upscaler::drawSettings(float x, float y)
{
    bool changed = false;

    for (Uniform &uniform : uniforms)
    {
        float newValue = GuiSlider(Rectangle{x, y, 256.f, 20.f},
                                   nullptr,
                                   uniform.name.c_str(),
                                   uniform.value,
                                   uniform.min,
                                   uniform.max);
        if (uniform.type == Uniform::Type::Int)
        {
            newValue = roundf(newValue);
        }

        if (newValue != uniform.value)
        {
            if (uniform.type == Uniform::Type::Float)
            {
                SetShaderValue(shader,
                               GetShaderLocation(shader, uniform.uniformName.c_str()),
                               &uniform.value,
                               SHADER_UNIFORM_FLOAT);
            }
            else
            {
                int intValue = newValue;
                SetShaderValue(shader,
                               GetShaderLocation(shader, uniform.uniformName.c_str()),
                               &intValue,
                               SHADER_UNIFORM_INT);
            }

            uniform.value = newValue;
            changed = true;
        }

        y += 32;
    }

    return changed;
}

void Upscaler::draw(Texture2D texture, RenderTexture2D output)
{

    BeginTextureMode(output);
    BeginShaderMode(shader);
    ClearBackground(BLANK);
    // RenderTextures in OpenGL must be flipped on the Y axis.
    Rectangle src{0, 0, float(texture.width), -float(texture.height)};
    Rectangle dest{0, 0, float(output.texture.width), float(output.texture.height)};
    DrawTexturePro(texture, src, dest, {0, 0}, 0.f, WHITE);
    EndShaderMode();
    EndTextureMode();
}

size_t Upscaler::getNumUniforms() const
{
    return uniforms.size();
}