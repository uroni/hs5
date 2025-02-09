from dataclasses import dataclass
import sys
import os
from pathlib import Path
import gzip
import hashlib

@dataclass
class Sourcefile:
    source: str
    header: str
    fn: Path
    var_name: str
    etag: str
    uncomp_len: int

def xxd_i(filename: Path, empty: bool) -> Sourcefile:
    with open(filename, 'rb') as f:
        raw_data = b"" if empty else f.read()

    etag = hashlib.md5(raw_data).hexdigest()
    data = gzip.compress(raw_data)

    var_name = filename.name.replace('.', '_').replace('-', '_')
    source = f"unsigned char {var_name}[] = {{\n"
    header = f"extern unsigned char {var_name}[];\nextern unsigned int {var_name}_len;\n"

    for i in range(0, len(data), 12):
        line = ', '.join(f'0x{byte:02x}' for byte in data[i:i+12])
        source += f"  {line},\n"

    source += f"}};\nunsigned int {var_name}_len = {len(data)};\n"
    return Sourcefile(source, header, filename, var_name, etag, len(raw_data))

def get_content_type(fn: Path) -> str:
    if fn.suffix == ".html":
        return "text/html"
    elif fn.suffix == ".css":
        return "text/css"
    elif fn.suffix == ".js":
        return "application/javascript"
    elif fn.suffix == ".png":
        return "image/png"
    elif fn.suffix == ".jpg":
        return "image/jpeg"
    elif fn.suffix == ".jpeg":
        return "image/jpeg"
    elif fn.suffix == ".gif":
        return "image/gif"
    elif fn.suffix == ".svg":
        return "image/svg+xml"
    elif fn.suffix == ".ico":
        return "image/x-icon"
    elif fn.suffix == ".json":
        return "application/json"
    elif fn.suffix == ".xml":
        return "application/xml"
    elif fn.suffix == ".pdf":
        return "application/pdf"
    elif fn.suffix == ".zip":
        return "application/zip"
    
    return "application/octet-stream"


def merge_sources(sources: list[Sourcefile], base_dir: Path) -> Sourcefile:
    source = ''
    header = ''
    srcs = "struct WwwFile\n{\n\tconst unsigned char* data;\n\tunsigned int len;\n\tstd::string etag;\n\tbool immutable;\n\tstd::string_view contentType;\n\tunsigned int uncompLen;\n};\n"
    srcs += "static std::map<std::string_view, WwwFile> www_files = {\n"
    for s in sources:
        source += '\n' + s.source
        header += '\n' + s.header

        rel_fn = str(s.fn)[len(str(base_dir))+1:]

        immutable = True if rel_fn.startswith("assets-") else False
        imm_str = "true" if immutable else "false"

        srcs += f'  {{ "{rel_fn}", WwwFile{{ {s.var_name}, {s.var_name}_len, "\\"{s.etag}\\"", {imm_str}, "{get_content_type(s.fn)}", {s.uncomp_len} }} }},\n'

    srcs += "};\n"

    return Sourcefile(source, "#pragma once\n#include <map>\n#include <string>\n" + header+"\n"+srcs, Path(), "", "", 0)


def list_files(dir: Path) -> list[Path]:
    return [f for f in dir.rglob('*') if f.is_file()]

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python wwwgen.py <directory> <empty or not-empty>")
        sys.exit(1)

    dir = Path(sys.argv[1])
    empty = sys.argv[2] == "empty"

    files = list_files(dir)
    srcs = list[Sourcefile]()

    for file in files:
        srcs.append(xxd_i(file, empty))

    res = merge_sources(srcs, dir)
    
    with open("www_files.h", 'w') as f:
        f.write(res.header)
    
    with open("www_files.cpp", 'w') as f:
        f.write(res.source)
