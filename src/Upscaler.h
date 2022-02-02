#ifndef _SQUINT_UPSCALER_H_
#define _SQUINT_UPSCALER_H_

#include "raylib.h"

#include <string>
#include <vector>

class Upscaler
{
  public:
    struct Uniform
    {
        enum class Type
        {
            Float,
            Int,
        };

        Type type;
        std::string name;
        std::string uniformName;
        float value;
        float min;
        float max;
    };

    Upscaler(const char *path);
    ~Upscaler();

    void unloadShader();

    void reload();

    void addUniform(Uniform uniform);

    int getTextWidth() const;

    bool drawSettings(float x, float y);

    void draw(Texture2D texture, RenderTexture2D output);

    size_t getNumUniforms() const;

  private:
    std::vector<Uniform> uniforms;
    std::string shaderPath;
    Shader shader{};
};

#endif // __SQUINT_UPSCALER_H_