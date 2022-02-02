#include "AsepriteConnection.h"

AsepriteImage::AsepriteImage(AsepriteImage &&other) noexcept
    : width(other.width)
    , height(other.height)
    , pixels(std::move(other.pixels))
{
    other.height = 0;
    other.width = 0;
}

AsepriteImage &AsepriteImage::operator=(AsepriteImage &&other) noexcept
{
    width = other.width;
    height = other.height;
    pixels = std::move(other.pixels);
    other.height = 0;
    other.width = 0;
    return *this;
}

void AsepriteConnection::onMessage(std::shared_ptr<ix::ConnectionState> connectionState,
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