#include "raygui.h"
#include "raylib.h"
#include "raymath.h"

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

#include "platformSetup.h"

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


int start()
{
    // Initialization
    //--------------------------------------------------------------------------------------
    
    bool useFullScreen = false;

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
    InitWindow(160, 120, "Squint micro viewer");
    SetExitKey(KEY_NULL);
    SetWindowState(FLAG_VSYNC_HINT);

    SetWindowMinSize(160, 120);

    SetTargetFPS(60);

    Texture2D currentTexture{};

    //--------------------------------------------------------------------------------------

    int renderScale = 1;

    Vector2 windowSize = Vector2{160.f, 120.f};

    bool previouslyConnected = false;

    while (!WindowShouldClose()) // Detect window close button or ESC key
    {
        if (IsKeyPressed(KEY_F))
        {
            unsigned int currentMonitor = GetCurrentMonitor();
            Vector2 currentMonitorPosition = GetMonitorPosition(currentMonitor);
            Vector2 currentMonitorSize =
                Vector2{float(GetMonitorWidth(currentMonitor)), float(GetMonitorHeight(currentMonitor))};

            useFullScreen = !useFullScreen;
            if (useFullScreen)
            {

                windowSize = currentMonitorSize;
                SetWindowSize(currentMonitorSize.x, currentMonitorSize.y);
                SetWindowState(FLAG_VSYNC_HINT | FLAG_WINDOW_TOPMOST);
                SetWindowPosition(currentMonitorPosition.x, currentMonitorPosition.y);
            }
            else
            {
                windowSize = {160.f, 120.f};

                unsigned int currentMonitor = GetCurrentMonitor();
                Vector2 centeredLocalPosition =
                    Vector2{(currentMonitorSize.x - 160.f) / 2.f,
                            (currentMonitorSize.y - 120.f) / 2.f};

                Vector2 finalPosition =
                    Vector2Add(currentMonitorPosition, centeredLocalPosition);
                SetWindowSize(160, 120);
                SetWindowState(FLAG_VSYNC_HINT);
                ClearWindowState(FLAG_WINDOW_TOPMOST);
                SetWindowPosition(finalPosition.x, finalPosition.y);
            }
        }


        // Draw
        //----------------------------------------------------------------------------------
        {
            BeginDrawing();

            if (!imageServer.connected)
            {
                ClearBackground(GRAY);
                Vector2 size =
                    MeasureTextEx(GetFontDefault(), "Waiting for connection.", 10, 0);
                Vector2 pos;
                pos.x = (windowSize.x - size.x) / 2.f;
                pos.y = (windowSize.y - size.y) / 2.f;
                DrawText("Waiting for connection.", int(pos.x), int(pos.y), 10, LIGHTGRAY);
            }
            else
            {
                ClearBackground(WHITE);
                AsepriteImage lastImage; 
                if (imageServer.lastReadyImageMutex.try_lock())
                {
                    lastImage = std::move(imageServer.lastReadyImage);
                    imageServer.lastReadyImageMutex.unlock();
                }
                Image currentImage;
                currentImage.width = lastImage.width;
                currentImage.height = lastImage.height;
                currentImage.data = lastImage.pixels.data();
                currentImage.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
                currentImage.mipmaps = 1;

                if ((lastImage.width != 0 && lastImage.height != 0))
                {
                    bool sizeMismatch = lastImage.width != currentTexture.width ||
                                        lastImage.height != currentTexture.height;
                    // Regenerate the base texture
                    if (sizeMismatch)
                    {
                        UnloadTexture(currentTexture);
                        currentTexture = LoadTextureFromImage(currentImage);
                    }
                    else
                    {
                        UpdateTexture(currentTexture, currentImage.data);
                    }
                }
                else if (imageServer.connected && !previouslyConnected)
                {
                    // Clear the texture in case of reconnection.
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
                    blankPixel.data = (void *)singlePixel;
                    blankPixel.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
                    blankPixel.mipmaps = 1;
                    currentTexture = LoadTextureFromImage(blankPixel);
                }

                Vector2 texturePosition{(windowSize.x - currentTexture.width) / 2.f,
                                        (windowSize.y - currentTexture.height) / 2.f};

                DrawTextureEx(currentTexture, texturePosition, 0, 1, WHITE);
            }

            previouslyConnected = imageServer.connected;
            EndDrawing();
        }
        //----------------------------------------------------------------------------------
    }

    serv.stop();
    ix::uninitNetSystem();

    // De-Initialization
    //--------------------------------------------------------------------------------------
    // Manual shader unload to avoid crashes due to unload order.
    UnloadTexture(currentTexture);

    CloseWindow(); // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}

int main(void)
{
    setupLoggingOutput();
    int result = start();
    unsetupLoggingOutput();
}
