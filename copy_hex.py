Import("env")
import os
import shutil
import re

def copy_hex_with_version(source, target, env):
    """
    Copy firmware.hex to project root with version number
    """
    # Get the version from Version.h
    version_file = os.path.join(env.get("PROJECT_DIR"), "lib", "aio_system", "Version.h")
    version = "unknown"
    
    try:
        with open(version_file, 'r') as f:
            content = f.read()
            # Look for FIRMWARE_VERSION definition
            match = re.search(r'#define\s+FIRMWARE_VERSION\s+"([^"]+)"', content)
            if match:
                version = match.group(1)
    except Exception as e:
        print(f"Warning: Could not read version from {version_file}: {e}")
    
    # Source hex file path
    build_dir = env.subst("$BUILD_DIR")
    prog_name = env.subst("$PROGNAME")
    hex_source = os.path.join(build_dir, f"{prog_name}.hex")
    
    # Destination hex file with version number
    hex_dest = os.path.join(env.get("PROJECT_DIR"), f"AiO_v{version}.hex")
    
    # Copy the hex file if it exists
    if os.path.exists(hex_source):
        try:
            shutil.copy2(hex_source, hex_dest)
            print(f"Copied firmware.hex to {os.path.basename(hex_dest)}")
        except Exception as e:
            print(f"Error copying hex file: {e}")
    else:
        print(f"Warning: Source hex file not found: {hex_source}")

# Add post-action to copy hex file after building
env.AddPostAction(
    "$BUILD_DIR/${PROGNAME}.hex",
    copy_hex_with_version
)