--[[
    Squint client script
        by Florian Dormont.

    Based on a sample project by Lampysprites.

    This script will keep running for a while if the dialog window 
    is closed with an "x" button :(

    If that happens, close the sprite and open again (Shift + Ctrl + T)
]]

local web_socket
local dialog
local current_sprite

--[[
Set up an image buffer for two reasons:
    a) the active cel might not be the same size as the sprite
    b) the sprite might not be in RGBA mode, and it's easier to use ase 
       than do conversions on the other side.
]]
local image_buffer

-- Send image command identifier
local IMAGE_ID = string.byte("I")


-- Forward declarations
local send_image_to_squint
local on_site_change


-- Clean up and exit
local function finish()
    if web_socket ~= nil then web_socket:close() end
    if dialog ~= nil then dialog:close() end
    if current_sprite ~= nil then current_sprite.events:off(send_image_to_squint) end
    app.events:off(on_site_change)
    current_sprite = nil
    dialog = nil
end

local function set_sprite_hooks(sprite)
    sprite.events:on('change', send_image_to_squint)
end

local function unset_sprite_hooks(sprite)
    sprite.events:off(send_image_to_squint)
end

local function setup_image_buffer()
    if image_buffer == nil then
        image_buffer = Image(current_sprite.width, current_sprite.height, ColorMode.RGB)
    elseif image_buffer.width ~= current_sprite.width or image_buffer.height ~= current_sprite.height then
        image_buffer:resize(current_sprite.width, current_sprite.height)
    end
end

send_image_to_squint = function()
    setup_image_buffer()
    image_buffer:clear()
    image_buffer:drawSprite(current_sprite, app.activeFrame.frameNumber)

    web_socket:sendBinary(string.pack("<LLL", IMAGE_ID, image_buffer.width, image_buffer.height), image_buffer.bytes)
end


-- close connection and ui if the sprite is closed
local frame = -1
on_site_change = function()
    if app.activeSprite ~= current_sprite then
        if current_sprite ~= nil then
           unset_sprite_hooks(current_sprite)
        end

        if app.activeSprite ~= nil then 
            current_sprite = app.activeSprite
            set_sprite_hooks(current_sprite)
            send_image_to_squint()
        end

        set_sprite_hooks(current_sprite)
    elseif current_sprite ~= nil then
        -- update the view after the frame changes
        if app.activeFrame.frameNumber ~= frame then
            frame = app.activeFrame.frameNumber
            send_image_to_squint()
        end
    end
end


local function on_squint_connection(message_type, message)
    if message_type == WebSocketMessageType.OPEN then
        dialog:modify{id="status", text="Connected"}
        app.events:on('sitechange', on_site_change)
        on_site_change()
    elseif message_type == WebSocketMessageType.CLOSE and dialog ~= nil then
        dialog:modify{id="status", text="No connection"}
        app.events:off(on_site_change)
    end
end

function exit(plugin)
    finish()
end

function init(plugin)
    plugin:newCommand {
        id="SquintStartClient",
        title="Connect to Squint",
        group="file_scripts",
        onclick=function()
            dialog = Dialog()
            current_sprite = app.activeSprite
            if current_sprite ~= nil then
                image_buffer = Image(current_sprite.width, current_sprite.height, ColorMode.RGB)
            end
            -- Set up a websocket
            -- TODO once I get an easy way to build squing with zlib, switch deflate to true
            web_socket = WebSocket{ url="http://127.0.0.1:34613", onreceive=on_squint_connection, deflate=false }

            -- Create the connection status popup
            dialog:label{ id="status", text="Connecting..." }
            dialog:button{ text="Close", onclick=finish}

            -- GO
            web_socket:connect()
            dialog:show{ wait=false }
        end
    }
end
