-- several problems uncovered by this example.

-- * it's awful to use subrects.
-- * no easy way to scale subrects.
-- * no easy way to change texture scale mode.
-- * error will randomly display white screen instead of error.

function rana.load()
    spritesheet = rana.gfx.loadTexture("spritesheet.png")
    spritesheet:setMagFilter(0x2600)
    frames = {
        { 0, 0 },
        { 0, 16 },
        { 0, 32 },
        { 0, 16 },
    }
end

t = 0
function rana.update(dt)
    t = t + dt / 2
    frame = math.floor(t * 5 % #frames)
end

function rana.configure(c)
    c.window_width = 960
    c.window_height = 720
end

function draw_sprite_frame(x, y, frame, scale, r, g, b, a)
    r = r or 1
    g = g or 1
    b = b or 1
    a = a or 1
    scale = scale or 1
    rana.gfx:pushTransform()
    rana.gfx:translate(x, y)
    rana.gfx:scale(scale, scale)

    x0, x1 = frames[frame + 1][1], frames[frame + 1][2]
    rana.gfx:drawTextureSub(spritesheet, x0, x1, 16, 16, -8, 16, r, g, b, a)

    rana.gfx:popTransform()
end

function rana.draw()
    rana.gfx:clear(0.2, 0.2, 0.2)

    -- those are arbitrary.
    speeds = { 1, 4, 3, 7, 5, 10, 2, 8, 9, 5, 1, 10, 4, 3, 8, 7, 2, 9, 5, 2 }
    for i = 1, 20 do
        local intensity = i / 20
        local x = (t * (10 + speeds[i]) * i - i * 777) % 1200 - 100
        local y = 100 * math.sin(t * (i + 5) / 40 - i)
        draw_sprite_frame(x, y, (frame + i) % #frames, i, intensity, intensity, intensity)
    end
end
