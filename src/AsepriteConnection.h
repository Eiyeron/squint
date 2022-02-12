#ifndef _SQUINT_ASEPRITECONNECTION_H_
#define _SQUINT_ASEPRITECONNECTION_H_

#include "ixwebsocket/IXConnectionState.h"
#include "ixwebsocket/IXWebSocket.h"
#include "ixwebsocket/IXWebSocketMessageType.h"
#include "raylib.h"

#include <cstdint>
#include <memory>
#include <vector>
#include <mutex>

struct AsepriteImage
{
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<Color> pixels{};

    AsepriteImage() = default;
    AsepriteImage(AsepriteImage &&other) noexcept;
    AsepriteImage &operator=(AsepriteImage &&other) noexcept;
    AsepriteImage(const AsepriteImage &other) = delete;
    AsepriteImage &operator=(const AsepriteImage &other) = delete;
};

struct AsepriteConnection
{
    mutable std::mutex lastReadyImageMutex;
    AsepriteImage lastReadyImage;
    bool connected = false;

    void onMessage(std::shared_ptr<ix::ConnectionState> connectionState,
                   ix::WebSocket &webSocket,
                   const ix::WebSocketMessagePtr &msg);
};

#endif // _SQUINT_ASEPRITECONNECTION_H_