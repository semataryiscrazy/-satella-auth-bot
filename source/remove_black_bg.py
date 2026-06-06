from PIL import Image
import os

# Load the image
img_path = "../img/download.png"
img = Image.open(img_path)

# Convert to RGBA if not already
if img.mode != 'RGBA':
    img = img.convert('RGBA')

# Get image data
data = img.getdata()

# Create new image data with transparent background
new_data = []
for item in data:
    # If pixel is black or very dark (close to black), make it transparent
    # Check if R, G, B are all less than 30 (very dark)
    if item[0] < 30 and item[1] < 30 and item[2] < 30:
        # Make it transparent
        new_data.append((255, 255, 255, 0))
    else:
        # Keep the pixel as is
        new_data.append(item)

# Update image data
img.putdata(new_data)

# Save the result
output_path = "celtic_knot_transparent.png"
img.save(output_path)

print(f"✓ Image processed successfully!")
print(f"✓ Saved to: {output_path}")
print(f"✓ Image size: {img.size}")
print(f"✓ Mode: {img.mode}")
