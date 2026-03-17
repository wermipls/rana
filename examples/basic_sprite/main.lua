-- problems revealed by this example:
-- * rana.gfx has inconsistent use of methods (colon syntax) and functions (dot syntax).
-- * we are loading a texture, but drawing a sprite? nonsense.

function rana.load()
    sprite = rana.gfx.loadTexture("seal.png")
end

t = 0
function rana.update(dt)
    t = t + dt
end

function rana.draw()
    rana.gfx:clear(0.6, 0.3, 0.7)

    rana.gfx:drawSprite(sprite, t * 50 % 1200 - 200, 200 + math.sin(t / 2), 1, 1, 0, 1,1,1,0.5)

    local scale = 1 + math.sin(t / 10)
    rana.gfx:drawSprite(sprite, 400 + math.sin(t) * 100, 300 + math.cos(t / 3) * 100, scale, scale, math.cos(t / 7) * math.pi * 8, 1,1,1,1)
end
