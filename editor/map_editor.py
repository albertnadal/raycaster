import dearpygui.dearpygui as dpg

TILE_SIZE=30
TILE_PADDING=1
MAP_WIDTH=10
MAP_HEIGHT=10
MAP_WINDOW_PADDING_TOP = 20
MAP_WINDOW_PADDING_LEFT = 5

map = [1,0,1,0,0,1,0,0,0,1,
       1,0,1,0,0,1,0,0,0,1]

textures = {}
cell_button_refs = []
selected_cell_ref = None

def cb_select_cell(sender, app_data, user_data):
    global selected_cell_ref
    if selected_cell_ref is not None:
        dpg.configure_item(selected_cell_ref, tint_color=(255, 255, 255, 255))

    selected_cell_ref = sender
    dpg.configure_item(sender, tint_color=(255, 100, 100, 100))

dpg.create_context()
dpg.create_viewport(title='Map editor', width=600, height=600)

def register_texture(texture_id, filename):
    global textures
    width, height, channels, data = dpg.load_image(filename)
    textures[texture_id] = dpg.add_dynamic_texture(width, height, data)

with dpg.texture_registry():
    register_texture("floor", "textures/floor.png")
    register_texture("graystone", "textures/graystone.png")
    register_texture("bluestone", "textures/bluestone.png")
    register_texture("colorstone", "textures/colorstone.png")
    register_texture("eagle", "textures/eagle.png")
    register_texture("mossystone", "textures/mossystone.png")
    register_texture("purplestone", "textures/purplestone.png")
    register_texture("redbrick", "textures/redbrick.png")
    register_texture("wood", "textures/wood.png")

with dpg.window(label="2D map", width=MAP_WIDTH*(TILE_SIZE+TILE_PADDING*2)+MAP_WINDOW_PADDING_LEFT, height=MAP_HEIGHT*(TILE_SIZE+TILE_PADDING*2)+MAP_WINDOW_PADDING_TOP) as main_window:
    for index, cell_type in enumerate(map):
        x = index % MAP_WIDTH
        y = index // MAP_WIDTH
        cell_button_refs.append(dpg.add_image_button(textures['colorstone'],
                                                     width=TILE_SIZE,
                                                     height=TILE_SIZE,
                                                     pos=(MAP_WINDOW_PADDING_LEFT+(x*(TILE_SIZE+TILE_PADDING*2)), MAP_WINDOW_PADDING_TOP+(y*(TILE_SIZE+TILE_PADDING*2))),
                                                     callback=cb_select_cell,
                                                     user_data={},
                                                     frame_padding=TILE_PADDING))

dpg.setup_dearpygui()
dpg.show_viewport()
dpg.start_dearpygui()
dpg.destroy_context()
