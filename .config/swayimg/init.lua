--
-- swayimg configuration file
--
-- https://github.com/artemsen/swayimg
-- swayimg(1)
-- /usr/share/doc/swayimg/CONFIG.md
--

swayimg.set_window_size(800, 600)
swayimg.viewer.set_default_scale("optimal")

swayimg.imagelist.enable_adjacent(true)

swayimg.text.set_foreground(0xffffffff)
swayimg.text.set_background(0x80000000)
swayimg.text.set_shadow(0)
swayimg.text.set_font("sans-serif")
swayimg.text.set_size(16)
swayimg.text.set_timeout(0)

function toggle_text()
  if swayimg.text.visible() then
    swayimg.text.hide()
  else
    swayimg.text.show()
  end
end

function zoom(factor)
  local scale = swayimg.viewer.get_scale()
  swayimg.viewer.set_abs_scale(scale * factor);
end

function zoom_cursor(factor)
  local pos = swayimg.get_mouse_pos()
  local scale = swayimg.viewer.get_scale()
  swayimg.viewer.set_abs_scale(scale * factor, pos.x, pos.y);
end

swayimg.viewer.on_key("q", swayimg.exit)
swayimg.viewer.on_key("Tab", toggle_text)
swayimg.viewer.on_key("space", function () swayimg.viewer.set_fix_scale("optimal") end)
swayimg.viewer.on_key("1", function () swayimg.viewer.set_fix_scale("real") end)
swayimg.viewer.on_key("2", function () swayimg.viewer.set_abs_scale(2) end)
swayimg.viewer.on_key("3", function () swayimg.viewer.set_abs_scale(0.5) end)
swayimg.viewer.on_key("w", function () swayimg.viewer.set_fix_scale("width") end)
swayimg.viewer.on_key("Shift-w", function () swayimg.viewer.set_fix_scale("height") end)
swayimg.viewer.on_key("v", function () swayimg.viewer.flip_vertical() end)
swayimg.viewer.on_key("Shift-v", function () swayimg.viewer.flip_horizontal() end)
swayimg.viewer.on_key("equal", function () zoom(11/10) end)
swayimg.viewer.on_key("minus", function () zoom(10/11) end)
swayimg.viewer.on_key("comma", function () swayimg.viewer.switch_image("prev") end)
swayimg.viewer.on_key("period", function () swayimg.viewer.switch_image("next") end)
swayimg.viewer.on_mouse("ScrollUp", function () zoom_cursor(11/10) end)
swayimg.viewer.on_mouse("ScrollDown", function () zoom_cursor(10/11) end)

swayimg.gallery.on_key("q", swayimg.exit)
