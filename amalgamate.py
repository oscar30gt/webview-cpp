import os
import re

ROOT_DIR = os.path.dirname(os.path.abspath(__file__))
SRC_DIR = os.path.join(ROOT_DIR, "src")
OUTPUT_DIR = os.path.join(ROOT_DIR, "dist")

# Archivo de entrada para la interfaz publica (.h)
HEADER_ENTRY = "webviewWindow.h"

# Lista de archivos de implementación para concatenar (.cc)
SOURCES_TO_PROCESS = [
    "webviewWindow.cc",
    "macos/core.cc",
    "win32/core.cc",
    "win32/styles.cc",
    "win32/bindings.cc",
    "win32/builder.cc",
    "win32/events.cc",
    "lib/json/json.cc",
]

# Regex para detectar inclusiones locales: #include "filename"
INCLUDE_QUOTE_REGEX = re.compile(r'^\s*#include\s+"([^"]+)"')


def process_header(file_path, included_files, include_stack=None):
    """
    Procesa recursivamente la interfaz (.h), resolviendo dependencias internas (#include "...").
    """
    if include_stack is None:
        include_stack = []

    abs_path = os.path.abspath(file_path)

    if abs_path in include_stack:
        return f"\n// [WARN] Circular inclusion detected: {os.path.basename(file_path)}\n"

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

    for line in lines:
        if "amalgamate(skip)" in line:
            content += f"{line.strip()} [OMITTED]\n"
            continue

        match = INCLUDE_QUOTE_REGEX.match(line)
        if match:
            inc_filename = match.group(1)
            current_dir = os.path.dirname(abs_path)
            target_path = os.path.normpath(os.path.join(current_dir, inc_filename))

            content += f"\n// --- BEGIN INCLUDED FROM: {inc_filename} ---\n"
            included_content = process_header(target_path, included_files, include_stack)
            content += included_content

            if content and not content.endswith("\n"):
                content += "\n"

            content += f"// --- END INCLUDED FROM: {inc_filename} ---\n"
        else:
            if "#pragma once" in line:
                continue
            content += line

    if content and not content.endswith("\n"):
        content += "\n"

    include_stack.pop()
    return content


def process_source(file_path):
    """
    Procesa un archivo .cc omitiendo las inclusiones por comillas (#include "...").
    """
    abs_path = os.path.abspath(file_path)

    if not os.path.exists(abs_path):
        return f"\n// [ERROR] Source file not found: {os.path.basename(file_path)}\n"

    with open(abs_path, "r", encoding="utf-8") as f:
        lines = f.readlines()

    content = f"\n// ============================================================================\n"
    content += f"// MODULE: {os.path.relpath(abs_path, SRC_DIR).replace(os.sep, '/')}\n"
    content += f"// ============================================================================\n\n"

    for line in lines:
        if "amalgamate(skip)" in line:
            content += f"{line.strip()} [OMITTED]\n"
            continue

        # Eliminar cualquier #include "..." local en las implementaciones
        if INCLUDE_QUOTE_REGEX.match(line):
            continue

        content += line

    if content and not content.endswith("\n"):
        content += "\n"

    return content


def main():
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    # 1. Generar webview-cpp.h
    print(f"Generating Header Amalgamation from {HEADER_ENTRY}...")
    included_files = set()
    header_content = "#pragma once\n\n"
    header_full_path = os.path.join(SRC_DIR, HEADER_ENTRY)
    header_content += process_header(header_full_path, included_files)

    header_output_path = os.path.join(OUTPUT_DIR, "webview-cpp.h")
    with open(header_output_path, "w", encoding="utf-8") as f:
        f.write(header_content)

    print(f"  -> Generated: {header_output_path}")

    # 2. Generar webview-cpp.cc
    print("\nGenerating Source Amalgamation (.cc)...")
    source_content = '#include "webview-cpp.h"\n\n'

    for rel_path in SOURCES_TO_PROCESS:
        display_name = rel_path.replace(os.sep, "/")
        print(f"  Appending {display_name}...")

        full_path = os.path.join(SRC_DIR, rel_path)
        source_content += process_source(full_path)

    source_output_path = os.path.join(OUTPUT_DIR, "webview-cpp.cc")
    with open(source_output_path, "w", encoding="utf-8") as f:
        f.write(source_content)

    print(f"  -> Generated: {source_output_path}")
    print("\nProcess finished successfully!")


if __name__ == "__main__":
    main()