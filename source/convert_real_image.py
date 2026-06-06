#!/usr/bin/env python3
from PIL import Image
import os

# Caminho da imagem original
img_path = r"C:\Users\s\Downloads\void attt byy tecxas\void attt byy tecxas\void attt by tecxas\void attt by tecxas\img\download.png"

# Ler a imagem
img = Image.open(img_path)

# Converter para RGBA se necessário
if img.mode != 'RGBA':
    img = img.convert('RGBA')

# Salvar como PNG
img.save('celtic_knot.png')

# Ler o arquivo PNG
with open('celtic_knot.png', 'rb') as f:
    png_data = f.read()

print(f"Imagem carregada com {len(png_data)} bytes")

# Converter para hexadecimal
hex_content = f"unsigned char iconData[{len(png_data)}] = {{\n"

for i, byte in enumerate(png_data):
    if i % 12 == 0:
        hex_content += "\t"
    hex_content += f"0x{byte:02X}"
    if i < len(png_data) - 1:
        hex_content += ", "
    if (i + 1) % 12 == 0 and i < len(png_data) - 1:
        hex_content += "\n"

hex_content += "\n};\n"

# Salvar o resultado
with open('icon_hex_final.h', 'w') as f:
    f.write(hex_content)

print("✓ Conversão concluída!")
print(f"✓ Arquivo salvo em: icon_hex_final.h")

# Limpar arquivo temporário
os.remove('celtic_knot.png')
