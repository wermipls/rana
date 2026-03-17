-- several problems uncovered by this example.

-- * main.lua that FAILS to load on syntax just. doesn't error properly. it gives random garbage
-- * rana.gfx:clear doesn't handle optional arguments.
-- * why is font size stateful?
-- * there's no good way to use default font.
-- * there's no good way to get size of the default font.
-- * shouldn't the backend automatically handle atlas baking when size is too divergent?
-- * and so, shouldn't the font size when generating a font not be a thing?
-- * this demo should really be a one-liner.

function rana.draw()
    rana.gfx:clear(1,1,1)
    rana.gfx:fontSize(36)
    rana.gfx:text(rana._font_default, 64, 64, "hello rana", 0,0,0,1)
end