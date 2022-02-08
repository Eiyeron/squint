#include "raygui.h"
#include "raylib.h"

// [HACK] IXWebsocket includes some of Windows' headers, this manua define
// circumvents a link conflict between Raylib's functions and Windows'.
#if defined(_WIN32) || defined(_WIN64)
#define _WINUSER_
#define _IMM_
#define _APISETCONSOLEL3_
#define _WINGDI_
#endif
#include "ixwebsocket/IXWebSocketServer.h"
#if defined(_WIN32) || defined(_WIN64)
#undef _WINUSER_
#undef _IMM_
#undef _APISETCONSOLEL3_
#undef _WINGDI_
#endif

#include "AsepriteConnection.h"
#include "Upscaler.h"

#include <cmath>
#include <cstdint>

enum class UiState
{
    Nothing,
    Main,
    Help,
};

static void DrawTextBorder(const char *text,
                           float x,
                           float y,
                           int size,
                           Color textColor,
                           Color outlineColor)
{
    for (int xb = -1; xb <= 1; ++xb)
    {
        for (int yb = -1; yb <= 1; ++yb)
        {
            if (xb == 0 && yb == 0)
            {
                continue;
            }

            DrawText(text, x + xb, y + yb, size, outlineColor);
        }
    }
    DrawText(text, x, y, size, textColor);
}

int main(void)
{
    using Uniform = Upscaler::Uniform;

    // Initialization
    //--------------------------------------------------------------------------------------
    int screenWidth = 800;
    int screenHeight = 450;

    UiState uiState = UiState::Nothing;

    AsepriteConnection imageServer;

    // Prepare the WebSocket server.
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

    // Prepare Raylib, the window, the graphics settings...
    InitWindow(screenWidth, screenHeight, "Squint live viewer");
    SetExitKey(KEY_NULL);
    SetWindowState(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    SetWindowMinSize(256, 256);

    SetTargetFPS(60); // Set our game to run at 60 frames-per-second

    Texture2D currentTexture{};
    RenderTexture2D upscaledTexture{};

    // Prepare the shaders.
    // xBR-lv1 (no blend version)
    Upscaler xbrLv1("shaders/xbr-lv1.frag");
    xbrLv1.addUniform(Uniform{Uniform::Type::Int, "Corner mode", "XbrCornerMode", 2, 0.f, 2.f});
    xbrLv1.addUniform(
        Uniform{Uniform::Type::Float, "Luma Weight", "XbrYWeight", 48.f, 0.f, 100.f});
    xbrLv1.addUniform(Uniform{Uniform::Type::Float,
                              "Color match threshold",
                              "XbrEqThreshold",
                              30.f,
                              0.f,
                              50.f});
    // xBR-lv2 (color blending version)
    Upscaler xbrLv2("shaders/xbr-lv2.frag");
    xbrLv2.addUniform(Uniform{Uniform::Type::Int, "Xbr Scale", "XbrScale", 4, 0.f, 5.f});
    xbrLv2.addUniform(Uniform{Uniform::Type::Int, "Corner mode", "XbrCornerMode", 0, 0.f, 3.f});
    xbrLv2.addUniform(
        Uniform{Uniform::Type::Float, "Luma Weight", "XbrYWeight", 48.f, 0.f, 100.f});
    xbrLv2.addUniform(Uniform{Uniform::Type::Float,
                              "Color match threshold",
                              "XbrEqThreshold",
                              30.f,
                              0.f,
                              50.f});
    xbrLv2.addUniform(
        Uniform{Uniform::Type::Float, "Lv 2 coefficient", "XbrLv2Coefficient", 2.f, 0.f, 3.f});

    int selectedUpscaler = 0;
    bool upscalerComboBoxActive = false;
    bool refreshUpscalee = false;
    bool refreshRenderTarget = false;

    bool darkBackground = false;
    bool willScreenshot = false;

    bool previouslyConnected = false;

    //--------------------------------------------------------------------------------------

    // Main game loop
    int renderScale = 5;

    while (!WindowShouldClose()) // Detect window close button or ESC key
    {
        if (IsKeyPressed(KEY_TAB) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
        {
            uiState = (uiState == UiState::Main) ? UiState::Nothing : UiState::Main;
        }

        if (IsKeyPressed(KEY_F1))
        {
            uiState = (uiState == UiState::Help) ? UiState::Nothing : UiState::Help;
        }

        if (IsKeyPressed(KEY_F2))
        {
            darkBackground = !darkBackground;
        }

        if (IsKeyPressed(KEY_S))
        {
            willScreenshot = true;
        }

        // Draw
        //----------------------------------------------------------------------------------
        {
            BeginDrawing();

            if (IsWindowResized())
            {
                screenWidth = GetScreenWidth();
                screenHeight = GetScreenHeight();
            }

            if (!imageServer.connected)
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
                ClearBackground(darkBackground ? DARKGRAY : WHITE);
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
                else if (imageServer.connected && !previouslyConnected)
                {
                    // Clear the texture in case of reconnection.
                    refreshUpscalee = true;
                    UnloadTexture(currentTexture);
                    Image blankPixel;
                    blankPixel.width = 1;
                    blankPixel.height = 1;
                    const char singlePixel[4] = {
                        0x0,
                        0x0,
                        0x0,
                        0x0,
                    };
                    blankPixel.data = (void*)singlePixel;
                    blankPixel.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
                    blankPixel.mipmaps = 1;
                    currentTexture = LoadTextureFromImage(blankPixel);
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

            previouslyConnected = imageServer.connected;

            if (uiState == UiState::Main)
            {
                int maxTextWidth = 0;
                int numFields = 0;
                {
                    if (selectedUpscaler == 0)
                    {
                        maxTextWidth = MeasureText("Filter", 10);
                    }
                    else if (selectedUpscaler == 1)
                    {
                        maxTextWidth = xbrLv1.getTextWidth() + 32.f;
                        numFields = xbrLv1.getNumUniforms();
                    }
                    else if (selectedUpscaler == 2)
                    {
                        maxTextWidth = xbrLv2.getTextWidth() + 32.f;
                        numFields = xbrLv2.getNumUniforms();
                    }
                }

                DrawRectangle(0,
                              0,
                              256.f + 16.f + maxTextWidth,
                              32.f * numFields + 18 + 48.f,
                              Color{255, 255, 255, 192});

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
                // The dropdown doesn't have a title, but let's add one just to have
                // it looking like the rest of the controls.
                // Ugly hacked coordinates.
                DrawText("Filter", 268, 45, 10, Fade(GetColor(GuiGetStyle(COMBOBOX, 2)), 1.f));

                if (!upscalerComboBoxActive)
                {
                    switch (selectedUpscaler)
                    {
                    case 1:
                        refreshUpscalee |= xbrLv1.drawSettings(32, 72);
                        break;
                    case 2:
                        refreshUpscalee |= xbrLv2.drawSettings(32, 72);
                        break;
                    }
                }

                if (selectedUpscaler != previousSelection)
                {
                    refreshUpscalee = true;
                }

                if (GuiButton({float(screenWidth) - 128 - 16, 8, 64, 20}, "Save!"))
                {
                    willScreenshot = true;
                }

                if (GuiButton({float(screenWidth) - 128 - 16, 40, 137, 20},
                              "Toggle background"))
                {
                    darkBackground = !darkBackground;
                }

                if (GuiButton({float(screenWidth) - 64 - 8, 8, 64, 20}, "Help!"))
                {
                    uiState = UiState::Help;
                }
            }
            else if (uiState == UiState::Help)
            {
                DrawRectangle(0, 0, screenWidth, screenHeight, Color{0, 0, 0, 128});
                DrawTextBorder("Controls", 16, 18, 30, WHITE, BLACK);
                DrawTextBorder(
                    R"END(- F1 to toggle this help
- F2 to toggle the background's color.
- Right-click or TAB to toggle the options.
- S to save the current result.
- F12 to screenshot.)END",
                    24,
                    60,
                    20,
                    WHITE,
                    BLACK);

                if (GuiButton({float(screenWidth) - 64 - 8, 8, 64, 20}, "Back"))
                {
                    uiState = UiState::Main;
                }
            }

            if (willScreenshot)
            {
                Image savedImage = LoadImageFromTexture(upscaledTexture.texture);
                ExportImage(savedImage, "saved.png");
                UnloadImage(savedImage);
                willScreenshot = false;
            }

            EndDrawing();
        }
        //----------------------------------------------------------------------------------
    }

    serv.stop();
    ix::uninitNetSystem();

    // De-Initialization
    //--------------------------------------------------------------------------------------
    // Manual shader unload to avoid crashes due to unload order.
    xbrLv1.unloadShader();
    xbrLv2.unloadShader();
    UnloadRenderTexture(upscaledTexture);
    UnloadTexture(currentTexture);

    CloseWindow(); // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}