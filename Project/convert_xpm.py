from PIL import Image
import sys

def convert_to_xpm(input_path, output_path, new_width=800, new_height=600):
    img = Image.open(input_path).convert('RGB')
    img = img.resize((new_width, new_height), Image.Resampling.NEAREST)
    img = img.quantize(colors=64, method=Image.Quantize.FASTOCTREE)
    img = img.convert('RGB')
    
    colors = {}
    char_list = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@#$%^&*()_+~`|}{[]:;?><,./-="
    char_idx = 0
    
    width, height = img.size
    pixels = img.load()
    
    for y in range(height):
        for x in range(width):
            r, g, b = pixels[x, y]
            hex_col = "#{:02x}{:02x}{:02x}".format(r, g, b)
            if hex_col not in colors:
                colors[hex_col] = char_list[char_idx % len(char_list)] + char_list[(char_idx // len(char_list)) % len(char_list)]
                char_idx += 1

    var_name = sys.argv[3] if len(sys.argv) > 3 else "mission_failed_xpm"
    with open(output_path, 'w') as f:
        f.write("/* XPM */\n")
        f.write(f"static char *{var_name}[] = {{\n")
        f.write(f'"{width} {height} {len(colors)} 2",\n')
        
        for hex_col, char_code in colors.items():
            f.write(f'"{char_code} c {hex_col}",\n')
            
        for y in range(height):
            line = ""
            for x in range(width):
                r, g, b = pixels[x, y]
                hex_col = "#{:02x}{:02x}{:02x}".format(r, g, b)
                line += colors[hex_col]
            if y == height - 1:
                f.write(f'"{line}"\n')
            else:
                f.write(f'"{line}",\n')
        f.write("};\n")

if __name__ == "__main__":
    convert_to_xpm(sys.argv[1], sys.argv[2])
