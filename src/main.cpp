#include "raylib.h"
#include "raygui.h"
#include <string>
#include <cstdint>
#include <cstdio>

// [HACK] IXWebsocket includes some of Windows' headers, this manua define sircumvents a link conflict between Raylib's functions and Windows'.
#if defined(_WIN32) || defined(_WIN64)
#define _WINUSER_
#define _IMM_
#define _APISETCONSOLEL3_
#define _WINGDI_
#endif
#include "ixwebsocket/IXWebSocketServer.h"
#include "ixwebsocket/IXWebSocketMessageType.h"

bool connected = false;

struct AsepriteImage
{
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<Color> pixels{};

    AsepriteImage() = default;

    AsepriteImage(AsepriteImage&& other) noexcept
        : width(other.width)
        , height(other.height)
        , pixels(std::move(other.pixels))
    {
        other.height = 0;
        other.width = 0;
    }

    AsepriteImage& operator=(AsepriteImage&& other) noexcept
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
        std::string name;
        std::string uniformName;
        float value;
    };


    Upscaler(const char* path)
        :shaderPath(path)
    {
        reload();
    }
    
    ~Upscaler()
    {
        UnloadShader(shader);
    }

    void reload()
    {
        char* shaderText = LoadFileText(shaderPath.c_str());
        Shader newShader = LoadShaderFromMemory(defaultVertexShader, shaderText);
        UnloadFileText(shaderText);
        if(newShader.id != 0)
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
        for (Uniform& uniform : uniforms)
        {
            float newValue = GuiSlider(Rectangle{ x, y, 256.f, 20.f }, nullptr, uniform.name.c_str(), uniform.value, 0, 256.f);
            if (newValue != uniform.value)
            {
                SetShaderValue(shader, GetShaderLocation(shader, uniform.uniformName.c_str()), &uniform.value, SHADER_UNIFORM_FLOAT);
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
        Rectangle src{ 0, 0, float(texture.width), -float(texture.height) };
        Rectangle dest{ 0, 0, float(output.texture.width), float(output.texture.height) };
        DrawTexturePro(texture, src, dest, { 0, 0 }, 0.f, WHITE);
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
        ix::WebSocket& webSocket, const ix::WebSocketMessagePtr& msg)
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
                uint32_t* hdr = (uint32_t*)msg->str.c_str();
                unsigned char* data = (unsigned char*)(msg->str.c_str()) + 3 * sizeof(uint32_t);

                if (hdr[0] == 'I') {
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

// async callback
void handleMessage(std::shared_ptr<ix::ConnectionState> connectionState,
    ix::WebSocket& webSocket, const ix::WebSocketMessagePtr& msg)
{
    unsigned long w, h;
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
            unsigned long* hdr = (unsigned long*)msg->str.c_str();
            unsigned char* data = (unsigned char*)msg->str.c_str() + 3 * sizeof(unsigned long);

            if (hdr[0] == 'I') {
                w = hdr[1];
                h = hdr[2];
                printf("Got an image data : %u %u", w, h);
            }

        }
        break;
    default:
        break;
    }
}


int main(void)
{

    // Initialization
    //--------------------------------------------------------------------------------------
    const int screenWidth = 800;
    const int screenHeight = 450;

    bool showGui = false;

    ImageServer imageServer;
    // init
    ix::initNetSystem();
    ix::WebSocketServer serv(34613);
    serv.disablePerMessageDeflate();
    serv.setOnClientMessageCallback([&imageServer](std::shared_ptr<ix::ConnectionState> connectionState,
        ix::WebSocket& webSocket, const ix::WebSocketMessagePtr& msg)
        {
            imageServer.onMessage(connectionState, webSocket, msg);
        });
    //serv.setOnClientMessageCallback(&handleMessage);

    serv.listenAndStart();

    InitWindow(screenWidth, screenHeight, "Squint live viewer");

    SetTargetFPS(60);               // Set our game to run at 60 frames-per-second

    //Image currentImage{};
    Texture2D currentTexture{};
    RenderTexture2D upscaledTexture{};

    enum class XbrVersion
    {
        LV1,
        LV2
    };
    XbrVersion selectedVersion = XbrVersion::LV1;

    Upscaler xbrLv1("../xbr-lv1.frag");
    xbrLv1.add_uniform(Upscaler::Uniform{ "Luma Weight", "XbrYWeight", 48.f });
    xbrLv1.add_uniform(Upscaler::Uniform{ "Color match threshold", "XbrEqThreshold", 30.f });
    //Upscaler xbrLv2("../xbr-lv2.frag");

    int selectedUpscaler = 0;
    bool upscalerComboBoxActive = false;
    bool refreshUpscalee = false;
    bool refreshRenderTarget = false;


    //--------------------------------------------------------------------------------------

    // Main game loop
    int renderScale = 5;


    while (!WindowShouldClose())    // Detect window close button or ESC key
    {

        if (IsKeyPressed(KEY_R))
        {
            xbrLv1.reload();
        }

        if (IsKeyPressed(KEY_TAB))
        {
            showGui = !showGui;
        }

        // Draw
        //----------------------------------------------------------------------------------
        {
            BeginDrawing();

            if (!connected)
            {
                ClearBackground(GRAY);
                Vector2 size = MeasureTextEx(GetFontDefault(), "Waiting for connection.", 20, 0);
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
                    bool sizeMismatch = lastImage.width != currentTexture.width || lastImage.height != currentTexture.height;
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

                    upscaledTexture = LoadRenderTexture(currentTexture.width * renderScale, currentTexture.height * renderScale);
                    SetTextureFilter(currentTexture, TEXTURE_FILTER_POINT);
                    SetTextureFilter(upscaledTexture.texture, TEXTURE_FILTER_POINT);
                }


                if (refreshUpscalee)
                {
                    refreshUpscalee = false;
                    switch (selectedUpscaler)
                    {
                    case 0:
                    {
                        BeginTextureMode(upscaledTexture);
                        ClearBackground(BLANK);
                        // RenderTextures in OpenGL must be flipped on the Y axis.
                        Rectangle src{ 0, 0, float(currentTexture.width), -float(currentTexture.height) };
                        Rectangle dest{ 0, 0, float(upscaledTexture.texture.width), float(upscaledTexture.texture.height) };
                        DrawTexturePro(currentTexture, src, dest, { 0, 0 }, 0.f, WHITE);
                        EndTextureMode();
                    }
                    break;
                    case 1:
                        xbrLv1.draw(currentTexture, upscaledTexture);
                        break;
                    }

                }


                Vector2 texturePosition{
                    (screenWidth - upscaledTexture.texture.width) / 2.f,
                    (screenHeight - upscaledTexture.texture.height) / 2.f
                };

                DrawTextureEx(upscaledTexture.texture, texturePosition, 0, 1, WHITE);

            }

            if (showGui)
            {
                switch (selectedUpscaler)
                {
                case 1:
                    refreshUpscalee =  xbrLv1.draw_settings(24, 72);
                    break;
                default:;

                }

                int previousSelection = selectedUpscaler;
                if (GuiDropdownBox({ 8, 8, 256, 20 }, "- None -;XBR-lv1;XBR-lv2", &selectedUpscaler, upscalerComboBoxActive))
                {
                    upscalerComboBoxActive = !upscalerComboBoxActive;
                }
                float selectedScale = GuiSliderBar({ 8, 40, 256, 20 }, nullptr, "Scale", float(renderScale), 1, 6);
                if (int(selectedScale) != renderScale)
                {
                    renderScale = int(selectedScale);
                    refreshRenderTarget = true;
                    refreshUpscalee = true;
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

    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}