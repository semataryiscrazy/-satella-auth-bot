from PIL import Image

# Load the transparent image
img = Image.open("celtic_knot_transparent.png")

# Read the image as bytes
with open("celtic_knot_transparent.png", "rb") as f:
    image_data = f.read()

# Create hex output
hex_output = "unsigned char iconData[" + str(len(image_data)) + "] = {\n"

# Convert to hex with proper formatting
for i, byte in enumerate(image_data):
    if i % 16 == 0:
        hex_output += "\t"
    hex_output += f"0x{byte:02X}"
    
    if i < len(image_data) - 1:
        hex_output += ", "
    
    if (i + 1) % 16 == 0 and i < len(image_data) - 1:
        hex_output += "\n"

hex_output += "\n};"

# Save to file
with open("icon_hex_transparent.h", "w") as f:
    f.write(hex_output)

print(f"✓ Conversion complete!")
print(f"✓ File size: {len(image_data)} bytes")
print(f"✓ Saved to: icon_hex_transparent.h")
