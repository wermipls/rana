function rana.draw()
    rana.gfx:clear(0.3, 0.6, 0.8)
    local y = 64
    local fontsz = 36
    rana.gfx:fontSize(fontsz)
    for i = 0, 10 do
        local alpha = i / 10
        rana.gfx:text(rana._font_default, 32, y, "this text has alpha of " .. tostring(alpha), 1,1,1,alpha)
        y = y + fontsz + 8
    end

    y = 64
    for i = 0, 10 do
        local alpha = i / 10
        rana.gfx:text(rana._font_default, 420, y, "this text has alpha of " .. tostring(alpha), 0,0,0,alpha)
        y = y + fontsz + 8
    end
end
