# Allows PlatformIO to compile shared sources from ../src
# (radar_ui.cpp, aircraft_data.cpp, coord_convert.cpp)

Import('env')
import os

# Path to shared sources (relative to the radar/ project directory)
shared_dir = os.path.join(env['PROJECT_DIR'], '..', 'src')

# Add to include path (belt-and-suspenders with build_flags)
env.Append(CPPPATH=[shared_dir])

# Add shared .cpp/.c files to the build
# SRC_BUILD may not be pre-initialized in newer SCons versions
if 'SRC_BUILD' not in env:
    env['SRC_BUILD'] = []
for f in os.listdir(shared_dir):
    if f.endswith(('.cpp', '.c')) and f != 'main.cpp':
        env['SRC_BUILD'].append(env.Object(source=os.path.join(shared_dir, f)))
