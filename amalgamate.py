import os
import re

ROOT_DIR = os.path.dirname(os.path.abspath(__file__))
SRC_DIR = os.path.join(ROOT_DIR, "src")
OUTPUT_DIR = os.path.join(ROOT_DIR, "dist")

# List of entry-point files to process, relative to SRC_DIR
FILES_TO_PROCESS = [
    "webviewWindow.h",
    "webviewWindow.cc",
    "platforms/win32_platform.cc",
    "platforms/macos_platform.cc",
    "lib/json/json.cc",
]

def process_file(file_path, included_files, include_stack=None):
    """
    Recursively processes a source file, resolving local header inclusions (#include "..."),
    preventing circular dependencies, stripping include guards, and handling skip directives.
    """
    if include_stack is None:
        include_stack = []

    abs_path = os.path.abspath(file_path)
    
    # Check for circular inclusion loops
    if abs_path in include_stack:
        return f"\n// [WARN] Circular inclusion detected: {os.path.basename(file_path)}\n"
    
    # Ensure each file is only included once (header guard / once-only inclusion logic)
    if abs_path in included_files:
        return ""

    included_files.add(abs_path)
    include_stack.append(abs_path)

    content = ""
    if not os.path.exists(abs_path):
        include_stack.pop()
        return f"\n// [ERROR] File not found: {os.path.basename(file_path)}\n"

    with open(abs_path, "r", encoding="utf-8") as f:
        lines = f.readlines()

    # Regex to capture local header inclusions: #include "filename"
    include_regex = re.compile(r'^\s*#include\s+"([^"]+)"')

    for line in lines:
        # If the line contains the skip directive, bypass standard inclusion and mark it
        if "amalgamate(skip)" in line:
            content += f"{line.strip()} [OMMITED]\n"
            continue

        match = include_regex.match(line)
        if match:
            inc_filename = match.group(1)
            current_dir = os.path.dirname(abs_path)
            target_path = os.path.normpath(os.path.join(current_dir, inc_filename))
            
            content += f"\n// --- BEGIN INCLUDED FROM: {inc_filename} ---\n"
            
            # Recursively retrieve and append the included file's content
            included_content = process_file(target_path, included_files, include_stack)
            content += included_content
            
            # Ensure a newline follows the inclusion block in case the file ended without one
            if content and not content.endswith("\n"):
                content += "\n"
                
            content += f"// --- END INCLUDED FROM: {inc_filename} ---\n"
        else:
            # Strip standard once-only include guards to avoid multiple definition issues
            if "#pragma once" in line:
                continue
            content += line

    # Ensure the file content ends with a newline to prevent concatenation bugs (e.g., '}#include')
    if content and not content.endswith("\n"):
        content += "\n"

    include_stack.pop()
    return content

def main():
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    print("Generating clean source amalgamation...")
    
    included_files = set()
    output_content = "#pragma once\n\n"
    
    for rel_path in FILES_TO_PROCESS:
        # Normalize path separators for clean and cross-platform terminal output
        display_name = rel_path.replace(os.sep, '/')
        print(f"Processing {display_name}...")
        
        full_path = os.path.join(SRC_DIR, rel_path)
        
        # Ensure proper spacing between top-level files
        if output_content and not output_content.endswith("\n"):
            output_content += "\n"
            
        output_content += process_file(full_path, included_files)

    output_file_path = os.path.join(OUTPUT_DIR, "webview-cpp.h")
    with open(output_file_path, "w", encoding="utf-8") as f:
        f.write(output_content)

    print(f"\nSuccess! Amalgamated file generated at: {output_file_path}")

if __name__ == "__main__":
    main()