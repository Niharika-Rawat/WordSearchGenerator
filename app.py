"""
Word Search Puzzle Generator - Streamlit App
--------------------------------------------
Generates a word search puzzle using a C++ backend solver, 
renders it as a downloadable image, and visualizes the solution.
"""

import streamlit as st
import subprocess, json, tempfile, os, io
from PIL import Image, ImageDraw, ImageFont

# ==============================
# Streamlit Configuration
# ==============================
st.set_page_config(page_title="Word Search Generator", layout="wide")
st.title("Word Search Generator")
st.markdown("Enter words (one per line). Prefix with `*` for must-include words.")

# ==============================
# Input Panels
# ==============================
input_col, settings_col = st.columns([2, 1])

with input_col:
    user_word_input = st.text_area(
        "Words (one per line)",
        height=250,
        placeholder="*HELLO\nWORLD\nSONGS"
    )
    cpp_timeout_ms = st.slider(
        "C++ solver time limit per run (ms)",
        200, 10000, 2000, step=100
    )

with settings_col:
    st.markdown("### Visual Settings")
    custom_font_upload = st.file_uploader("Upload .ttf font (optional)", type=["ttf"])
    use_default_font = st.checkbox("Use bundled font (Aaargh.ttf)")
    font_size_pt = st.number_input("Font size (pt)", min_value=4, value=72, step=1)
    font_color_hex = st.text_input("Letter color (hex)", value="#FFFFFF")
    background_color_hex = st.text_input("Background color (hex)", value="#000000")
    is_transparent_bg = st.checkbox("Transparent background")
    output_format = st.selectbox("Download format", ["PNG", "JPEG"])
    paper_size = st.selectbox("Page size", ["A5", "A4", "A3"])


# ==============================
# Helper Functions
# ==============================
def convert_hex_to_rgb(hex_color, fallback=(0, 0, 0)):
    """Convert a hex color string to an RGB tuple."""
    try:
        clean_hex = hex_color.strip().lstrip('#')
        if len(clean_hex) == 3:
            clean_hex = ''.join([c * 2 for c in clean_hex])
        return tuple(int(clean_hex[i:i + 2], 16) for i in (0, 2, 4))
    except Exception:
        return fallback


def run_cpp_solver(word_list, max_rows, max_cols, timeout_ms):
    """Execute the C++ backend solver and return parsed JSON output."""
    executable = "./wordsearch_solver" if os.name != 'nt' else "wordsearch_solver.exe"
    if not os.path.exists(executable):
        st.error(f"C++ executable not found: {executable}. Compile it before running.")
        return None

    args = [executable, f"--timems={int(timeout_ms)}"]
    if max_rows > 0 and max_cols > 0:
        args += [f"--rows={max_rows}", f"--cols={max_cols}"]

    input_data = "\n".join(word_list) + "\n"

    try:
        result = subprocess.run(
            args,
            input=input_data,
            text=True,
            capture_output=True,
            timeout=(timeout_ms / 1000.0) + 5
        )
        if result.returncode != 0:
            st.error("C++ solver error:\n" + result.stderr)
            return None
        return json.loads(result.stdout)
    except subprocess.TimeoutExpired:
        st.error("C++ process timed out.")
    except Exception as e:
        st.error(f"Error parsing C++ output: {e}")
        st.write("Raw output:", result.stdout)
    return None


def generate_puzzle_image(grid, word_positions, page_size, font_rgb, bg_rgb,
                          transparent_bg, font_file, file_format,
                          font_pt_size=72, use_default_font=False):
    """Render the puzzle grid as an image."""
    # Define page dimensions (300 DPI)
    page_dimensions = {"A5": (1748, 2480), "A4": (2480, 3508), "A3": (3508, 4961)}
    width, height = page_dimensions.get(page_size, page_dimensions["A4"])
    rows, cols = len(grid), len(grid[0]) if grid else (0, 0)
    margin = int(min(width, height) * 0.06)
    usable_w, usable_h = width - 2 * margin, height - 2 * margin
    cell_size = min(usable_w // cols, usable_h // rows)
    start_x, start_y = (width - cell_size * cols) // 2, (height - cell_size * rows) // 2

    mode = "RGBA" if transparent_bg else "RGB"
    background = (255, 255, 255, 0) if transparent_bg else bg_rgb
    image = Image.new(mode, (width, height), background)
    draw = ImageDraw.Draw(image)

    # Load font
    try:
        if font_file and os.path.exists(font_file):
            font = ImageFont.truetype(font_file, font_pt_size)
        elif use_default_font and os.path.exists("Aaargh.ttf"):
            font = ImageFont.truetype("Aaargh.ttf", font_pt_size)
        else:
            font = ImageFont.load_default()
    except Exception:
        font = ImageFont.load_default()

    # Optional gradient background
    if not transparent_bg:
        def blend(a, b, t): return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3))
        for i in range(height):
            t = i / max(1, height - 1)
            col = blend(bg_rgb, (240, 240, 240), t)
            draw.line([(0, i), (width, i)], fill=col)

    # Draw grid letters
    for r in range(rows):
        for c in range(cols):
            letter = grid[r][c]
            x, y = start_x + c * cell_size, start_y + r * cell_size
            w, h = draw.textbbox((0, 0), letter, font=font)[2:]
            draw.text((x + (cell_size - w) / 2, y + (cell_size - h) / 2),
                      letter, fill=font_rgb, font=font)

    return image


# ==============================
# Generate Puzzle Action
# ==============================
if st.button("Generate Puzzle"):
    word_list = [line.strip() for line in user_word_input.splitlines() if line.strip()]
    if not word_list:
        st.error("Please enter at least one word.")
    else:
        puzzle_data = run_cpp_solver(word_list, 0, 0, cpp_timeout_ms)
        if puzzle_data:
            grid = puzzle_data["grid"]
            placements = puzzle_data["placements"]
            placed_words = puzzle_data["placed_words"]
            unplaced_words = puzzle_data["unplaced_words"]

            st.success(f"Placed {len(placed_words)} words; {len(unplaced_words)} unplaced.")
            st.write("✅ Placed:", placed_words)
            if unplaced_words:
                st.warning("⚠️ Unplaced: " + ", ".join(unplaced_words))

            # Save uploaded font temporarily
            temp_font_path = None
            if custom_font_upload:
                with tempfile.NamedTemporaryFile(delete=False, suffix=".ttf") as temp_font:
                    temp_font.write(custom_font_upload.read())
                    temp_font_path = temp_font.name

            # Generate puzzle image
            font_rgb = convert_hex_to_rgb(font_color_hex)
            bg_rgb = convert_hex_to_rgb(background_color_hex, (255, 255, 255))
            puzzle_image = generate_puzzle_image(
                grid, placements, paper_size, font_rgb, bg_rgb,
                is_transparent_bg, temp_font_path, output_format,
                font_size_pt, use_default_font
            )
            st.image(puzzle_image, width=min(puzzle_image.width, 1200))

            # Download puzzle
            puzzle_buffer = io.BytesIO()
            puzzle_image.save(puzzle_buffer, format=output_format)
            puzzle_buffer.seek(0)
            st.download_button("⬇️ Download Puzzle", puzzle_buffer,
                               file_name=f"wordsearch.{output_format.lower()}")

            # Generate and show solution
            solution_image = puzzle_image.copy()
            draw = ImageDraw.Draw(solution_image)
            page_dimensions = {"A5": (1748, 2480), "A4": (2480, 3508), "A3": (3508, 4961)}
            width, height = page_dimensions.get(paper_size, page_dimensions["A4"])
            margin = int(min(width, height) * 0.06)
            usable_w, usable_h = width - 2 * margin, height - 2 * margin
            rows, cols = len(grid), len(grid[0])
            cell_size = min(usable_w // cols, usable_h // rows)
            start_x, start_y = (width - cell_size * cols) // 2, (height - cell_size * rows) // 2

            for word_data in placements:
                for k in range(len(word_data["word"])):
                    x = start_x + (word_data["col"] + k * word_data["dc"]) * cell_size
                    y = start_y + (word_data["row"] + k * word_data["dr"]) * cell_size
                    draw.rectangle([x + 2, y + 2, x + cell_size - 3, y + cell_size - 3],
                                   outline=(255, 0, 0), width=3)

            solution_buffer = io.BytesIO()
            solution_image.save(solution_buffer, format=output_format)
            solution_buffer.seek(0)
            st.image(solution_image, caption="Solution (highlighted)",
                     width=min(puzzle_image.width, 1200))
            st.download_button("⬇️ Download Solution", solution_buffer,
                               file_name=f"wordsearch_solution.{output_format.lower()}")

            # Cleanup
            if temp_font_path:
                try:
                    os.remove(temp_font_path)
                except Exception:
                    pass
