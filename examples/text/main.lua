-- several problems demonstrated by this example.

-- * gfx is an object, but i really don't wanna have to type both dots and colons.
-- * color is not an optional argument when drawing text. error is cryptic.
-- * no documentation or definitions for luals (which makes all the above problems much much worse.)
-- * there is no dynamic atlas generation (most non-ascii characters fail.)
-- * scaled text does not look good by default.
-- * no kerning (we should probably integrate harfbuzz.)

function rana.configure(c)
    c.window_title = "Text rendering example"
    c.window_width = 1280
    c.window_height = 720
end

function rana.load()
    font = rana.gfx.loadFont("NotoSansJP-Regular.otf", 24)
end

function rana.draw()
    rana.gfx:clear(0.95, 0.95, 0.95)
    local y = 32
    for i = 0, 14 do
        local fontsz = 12 + i * 4
        rana.gfx:fontSize(fontsz)
        -- from https://ja.wikipedia.org/wiki/Lorem_ipsum, CC BY-SA 4.0.
        rana.gfx:text(font, 32, y, "lorem ipsum（ロレム・イプサム、略してリプサム lipsum ともいう）とは、出版、ウェブデザイン、グラフィックデザインなどの諸分野において使用されている典型的なダミーテキスト（英語版）。", 0,0,0,1)
        y = y + fontsz + 8
    end
end
