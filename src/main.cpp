#include "raygui.h"
#include "raylib.h"
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <string>

// [HACK] IXWebsocket includes some of Windows' headers, this manua define
// circumvents a link conflict between Raylib's functions and Windows'.
#if defined(_WIN32) || defined(_WIN64)
#define _WINUSER_
#define _IMM_
#define _APISETCONSOLEL3_
#define _WINGDI_
#endif
#include "ixwebsocket/IXWebSocketMessageType.h"
#include "ixwebsocket/IXWebSocketServer.h"

bool connected = false;

struct AsepriteImage
{
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<Color> pixels{};

    AsepriteImage() = default;

    AsepriteImage(AsepriteImage &&other) noexcept
        : width(other.width)
        , height(other.height)
        , pixels(std::move(other.pixels))
    {
        other.height = 0;
        other.width = 0;
    }

    AsepriteImage &operator=(AsepriteImage &&other) noexcept
    {
        width = other.width;
        height = other.height;
        pixels = std::move(other.pixels);
        other.height = 0;
        other.width = 0;
        return *this;
    }
};

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

struct Upscaler
{
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

    Upscaler(const char *path)
        : shaderPath(path)
    {
        reload();
    }

    ~Upscaler()
    {
        if (shader.id != 0)
        {
            UnloadShader(shader);
            shader = {};
        }
    }

    void reload()
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

    void add_uniform(Uniform uniform)
    {
        uniforms.push_back(uniform);
    }

    bool draw_settings(float x, float y)
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

            y += 24;
        }

        return changed;
    }

    void draw(Texture2D texture, RenderTexture2D output)
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

    std::vector<Uniform> uniforms;
    std::string shaderPath;
    Shader shader{};
};

struct ImageServer
{
    AsepriteImage lastReadyImage;

    void onMessage(std::shared_ptr<ix::ConnectionState> connectionState,
                   ix::WebSocket &webSocket,
                   const ix::WebSocketMessagePtr &msg)
    {
        uint32_t w{}, h{};
        switch (msg->type)
        {
        case ix::WebSocketMessageType::Close:
            connected = false;
            break;
        case ix::WebSocketMessageType::Open:
            connected = true;
            break;
        case ix::WebSocketMessageType::Message:
            if (msg->binary)
            {

                unsigned long *hdr = (unsigned long *)msg->str.c_str();
                unsigned char *data =
                    (unsigned char *)(msg->str.c_str()) + 3 * sizeof(unsigned long);

                if (hdr[0] == 'I')
                {
                    AsepriteImage newImage;
                    newImage.width = hdr[1];
                    newImage.height = hdr[2];
                    uint64_t dataSize = uint64_t(newImage.width) * uint64_t(newImage.height);
                    newImage.pixels.reserve(dataSize);
                    for (uint32_t i = 0; i < dataSize; ++i)
                    {
                        Color nextByte;
                        nextByte.r = data[0];
                        nextByte.g = data[1];
                        nextByte.b = data[2];
                        nextByte.a = data[3];
                        newImage.pixels.emplace_back(nextByte);
                        data += 4;
                    }
                    lastReadyImage = std::move(newImage);
                }
            }
            break;
        default:
            break;
        }
    }
};

int main(void)
{

    // Initialization
    //--------------------------------------------------------------------------------------
    int screenWidth = 800;
    int screenHeight = 450;

    bool showGui = false;

    ImageServer imageServer;
    // init
    ix::initNetSystem();
    ix::WebSocketServer serv(34613);
    serv.disablePerMessageDeflate();
    serv.setOnClientMessageCallback(
        [&imageServer](std::shared_ptr<ix::ConnectionState> connectionState,
                       ix::WebSocket &webSocket,
                       const ix::WebSocketMessagePtr &msg) {
            imageServer.onMessage(connectionState, webSocket, msg);
        });

    serv.listenAndStart();

    InitWindow(screenWidth, screenHeight, "Squint live viewer");
    SetWindowState(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);

    SetTargetFPS(60); // Set our game to run at 60 frames-per-second

    Texture2D currentTexture{};
    RenderTexture2D upscaledTexture{};

    Upscaler xbrLv1("../xbr-lv1.frag");
    xbrLv1.add_uniform(Upscaler::Uniform{Upscaler::Uniform::Type::Float, "Luma Weight", "XbrYWeight", 48.f, 0.f, 100.f});
    xbrLv1.add_uniform(Upscaler::Uniform{Upscaler::Uniform::Type::Float, "Color match threshold", "XbrEqThreshold", 30.f, 0.f, 50.f});
    Upscaler xbrLv2("../xbr-lv2.frag");
    xbrLv2.add_uniform(Upscaler::Uniform{Upscaler::Uniform::Type::Int, "Xbr Scale", "XbrScale", 4, 0.f, 5.f});
    xbrLv2.add_uniform(Upscaler::Uniform{Upscaler::Uniform::Type::Float, "Luma Weight", "XbrYWeight", 48.f, 0.f, 100.f});
    xbrLv2.add_uniform(Upscaler::Uniform{Upscaler::Uniform::Type::Float, "Color match threshold", "XbrEqThreshold", 30.f, 0.f, 50.f});
    xbrLv2.add_uniform(Upscaler::Uniform{Upscaler::Uniform::Type::Float, "Lv 2 coefficient", "XbrLv2Coefficient", 2.f, 0.f, 3.f});

    int selectedUpscaler = 0;
    bool upscalerComboBoxActive = false;
    bool refreshUpscalee = false;
    bool refreshRenderTarget = false;

    //--------------------------------------------------------------------------------------

    // Main game loop
    int renderScale = 5;

    while (!WindowShouldClose()) // Detect window close button or ESC key
    {

        if (IsKeyPressed(KEY_R))
        {
            xbrLv2.reload();
        }

        if (IsKeyPressed(KEY_TAB))
        {
            showGui = !showGui;
        }

        // Draw
        //----------------------------------------------------------------------------------
        {
            BeginDrawing();

            if(IsWindowResized())
            {
                screenWidth = GetScreenWidth();
                screenHeight = GetScreenHeight();
            }

            if (!connected)
            {
                ClearBackground(GRAY);
                Vector2 size =
                    MeasureTextEx(GetFontDefault(), "Waiting for connection.", 20, 0);
                Vector2 pos;
                pos.x = (screenWidth - size.x) / 2;
                pos.y = (screenHeight - size.y) / 2;
                DrawText("Waiting for connection.", int(pos.x), int(pos.y), 20, LIGHTGRAY);
            }
            else
            {
                ClearBackground(WHITE);
                AsepriteImage lastImage = std::move(imageServer.lastReadyImage);
                Image currentImage;
                currentImage.width = lastImage.width;
                currentImage.height = lastImage.height;
                currentImage.data = lastImage.pixels.data();
                currentImage.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
                currentImage.mipmaps = 1;

                if ((lastImage.width != 0 && lastImage.height != 0))
                {
                    refreshUpscalee = true;
                    bool sizeMismatch = lastImage.width != currentTexture.width ||
                                        lastImage.height != currentTexture.height;
                    // Regenerate the base texture
                    if (sizeMismatch)
                    {
                        UnloadTexture(currentTexture);
                        currentTexture = LoadTextureFromImage(currentImage);
                        refreshRenderTarget = true;
                    }
                    else
                    {
                        UpdateTexture(currentTexture, currentImage.data);
                    }
                }

                if (refreshRenderTarget)
                {
                    refreshRenderTarget = false;
                    UnloadRenderTexture(upscaledTexture);

                    upscaledTexture = LoadRenderTexture(currentTexture.width * renderScale,
                                                        currentTexture.height * renderScale);
                    SetTextureFilter(currentTexture, TEXTURE_FILTER_POINT);
                    SetTextureFilter(upscaledTexture.texture, TEXTURE_FILTER_POINT);
                }

                if (refreshUpscalee)
                {
                    refreshUpscalee = false;
                    switch (selectedUpscaler)
                    {
                    case 0: {
                        BeginTextureMode(upscaledTexture);
                        ClearBackground(BLANK);
                        // RenderTextures in OpenGL must be flipped on the Y axis.
                        Rectangle src{0,
                                      0,
                                      float(currentTexture.width),
                                      -float(currentTexture.height)};
                        Rectangle dest{0,
                                       0,
                                       float(upscaledTexture.texture.width),
                                       float(upscaledTexture.texture.height)};
                        DrawTexturePro(currentTexture, src, dest, {0, 0}, 0.f, WHITE);
                        EndTextureMode();
                    }
                    break;
                    case 1:
                        xbrLv1.draw(currentTexture, upscaledTexture);
                        break;
                    case 2:
                        xbrLv2.draw(currentTexture, upscaledTexture);
                        break;
                    }
                }

                Vector2 texturePosition{(screenWidth - upscaledTexture.texture.width) / 2.f,
                                        (screenHeight - upscaledTexture.texture.height) / 2.f};

                DrawTextureEx(upscaledTexture.texture, texturePosition, 0, 1, WHITE);
            }

            if (showGui)
            {
                float selectedScale =
                    GuiSliderBar({8, 8, 256, 20}, nullptr, "Scale", float(renderScale), 1, 6);
                int integerScale = roundf(selectedScale);
                if (integerScale != renderScale)
                {
                    renderScale = integerScale;
                    refreshRenderTarget = true;
                    refreshUpscalee = true;
                }

                int previousSelection = selectedUpscaler;
                if (GuiDropdownBox({8, 40, 256, 20},
                                   "- None -;XBR-lv1;XBR-lv2",
                                   &selectedUpscaler,
                                   upscalerComboBoxActive))
                {
                    upscalerComboBoxActive = !upscalerComboBoxActive;
                }

                if (!upscalerComboBoxActive)
                {
                    switch (selectedUpscaler)
                    {
                    case 1:
                        refreshUpscalee |= xbrLv1.draw_settings(24, 72);
                        break;
                    case 2:
                        refreshUpscalee |= xbrLv2.draw_settings(24, 72);
                        break;
                    default:;
                    }
                }

                if (selectedUpscaler != previousSelection)
                {
                    refreshUpscalee = true;
                }
            }

            EndDrawing();
        }
        //----------------------------------------------------------------------------------
    }

    serv.stop();
    ix::uninitNetSystem();

    // De-Initialization
    //--------------------------------------------------------------------------------------
    UnloadRenderTexture(upscaledTexture);
    UnloadTexture(currentTexture);

    CloseWindow(); // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}