# Allows PlatformIO to compile shared sources from ../src
# Each shared .cpp/.c file must be listed explicitly.

Import('env')
import os

# Path to shared sources (relative to the radar/ project directory)
shared_dir = os.path.join(env['PROJECT_DIR'], '..', 'src')

# Add to include path (belt-and-suspenders with build_flags)
env.Append(CPPPATH=[shared_dir])

# Explicit whitelist of shared sources to compile
shared_sources = [
    'aircraft_data.cpp',
    'coord_convert.cpp',
    'radar_ui.cpp',
    'lv_font_monoid_12.c',
]

# Add each source to the build
if 'SRC_BUILD' not in env:
    env['SRC_BUILD'] = []
for f in shared_sources:
    src_path = os.path.join(shared_dir, f)
    if os.path.isfile(src_path):
        env['SRC_BUILD'].append(env.Object(source=src_path))
