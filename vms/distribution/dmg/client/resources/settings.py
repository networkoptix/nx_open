default_view = 'icon_view'
window_rect = ((400, 400), (600, 400))

include_icon_view_settings = True
grid_spacing = 48
text_size = 13
icon_size = 56
show_pathbar = False

icon = defines.get('dmg_icon')
background = defines.get('dmg_background')
files = [defines.get('dmg_app_path')]
symlinks = {'Applications': '/Applications'}
icon_locations = {
    defines.get('dmg_app_bundle'): (134, 234),
    'Applications': (466, 234)
}
