import struct
import argparse
import os
import urllib.request
from io import BytesIO
from errors import error_invalid_syntax, error_type_mismatch, error_missing_section, FSALUnknownKeyError

TYPE_IDS = {
    "int": 1,
    "float": 2,
    "bool": 3,
    "str": 4,
    "table": 5,
}

VALID_TYPES = list(TYPE_IDS.keys())

# ---------------- PARSER ----------------

def parse_aap(path):
    data = {}
    links = []
    section = None

    with open(path, "r", encoding="utf-8") as f:
        lines = [l.rstrip() for l in f]

    i = 0
    while i < len(lines):
        line = lines[i].strip()
        i += 1

        if not line or line.startswith("#"):
            continue

        if line.startswith("@"):
            links.append(line[1:])
            continue

        if line.startswith("[") and line.endswith("]"):
            section = line[1:-1]
            data[section] = {}
            continue

        if line.endswith("=[") or line.endswith("= ["):
            if section is None:
                raise ValueError("Table must be inside a section")

            name = line.split("=", 1)[0].strip()
            table = {}

            while i < len(lines):
                l = lines[i].strip()
                i += 1
                if l == "]":
                    break

                if "=" not in l or ":" not in l:
                    raise ValueError(f"Invalid table line: {l}")

                k, rest = l.split("=", 1)
                v, t = rest.rsplit(":", 1)
                table[k.strip()] = (v.strip(), t.strip())

            data[section][name] = table
            continue

        if "=" not in line or ":" not in line:
            error_invalid_syntax(f"Invalid line format (expected 'key = value : type')", i, line)
            raise ValueError(f"Invalid line: {line}")

        if section is None:
            error_missing_section(f"Key/value outside section at line {i}")
            raise ValueError(f"Key/value outside section: {line}")

        k, rest = line.split("=", 1)
        v, t = rest.rsplit(":", 1)
        
        t_stripped = t.strip()
        if t_stripped not in VALID_TYPES:
            err = FSALUnknownKeyError(t_stripped, VALID_TYPES, i)
            print(err.format_error())
            print()
            raise ValueError(f"Unknown type: {t_stripped}")
        
        data[section][k.strip()] = (v.strip(), t_stripped)

    return data, links

# ---------------- ENCODER ----------------

def encode_value(val, typ):
    if typ == "int":
        try:
            return struct.pack(">i", int(val))
        except ValueError:
            error_type_mismatch("int", val, 0)
            raise
    if typ == "float":
        try:
            return struct.pack(">f", float(val))
        except ValueError:
            error_type_mismatch("float", val, 0)
            raise
    if typ == "bool":
        return b"\x01" if val.lower() == "true" else b"\x00"
    if typ == "str":
        return val.encode()
    
    err = FSALUnknownKeyError(typ, VALID_TYPES)
    print(err.format_error())
    print()
    raise ValueError(f"Unknown type {typ}")

# ---------------- LINK EXECUTOR ----------------

def execute_link(link):
    """Execute a @link immediately using exec.
    
    WARNING: This executes arbitrary code from remote URLs without validation.
    This is a significant security risk and should only be used with trusted sources.
    Consider implementing:
    - URL whitelist validation
    - Code sandboxing (e.g., RestrictedPython)
    - Digital signature verification
    - User confirmation prompts
    """
    link = link.strip()
    if link.startswith("@"):
        url = link[1:].strip()
        print(f"[ATFF] WARNING: Executing remote code from {url}")
        print(f"[ATFF] This is a security risk. Only use trusted sources.")
        try:
            code = urllib.request.urlopen(url).read().decode()
            exec(code, {"__name__": "__atff_link__", "__builtins__": __builtins__})
        except Exception as e:
            print(f"[ATFF] Error executing {url}: {e}")

# ---------------- TABLE WRITER ----------------

def write_table(f, table):
    f.write(struct.pack(">I", len(table)))
    for k, (v, t) in table.items():
        kb = k.encode()
        vb = encode_value(v, t)

        f.write(struct.pack(">H", len(kb)))
        f.write(kb)
        f.write(struct.pack(">B", TYPE_IDS[t]))
        f.write(struct.pack(">I", len(vb)))
        f.write(vb)

# ---------------- ATFF WRITER ----------------

def write_atff(data, links, out):
    with open(out, "wb") as f:
        f.write(b"ATFF")

        # ---------------- LINKS ----------------
        f.write(struct.pack(">I", len(links)))
        for l in links:
            execute_link(l)  # <-- execute immediately
            lb = l.encode()
            f.write(struct.pack(">H", len(lb)))
            f.write(lb)

        # ---------------- SECTIONS ----------------
        f.write(struct.pack(">I", len(data)))
        for sec, items in data.items():
            sb = sec.encode()
            f.write(struct.pack(">H", len(sb)))
            f.write(sb)

            f.write(struct.pack(">I", len(items)))
            for k, v in items.items():
                kb = k.encode()
                f.write(struct.pack(">H", len(kb)))
                f.write(kb)

                if isinstance(v, dict):
                    bio = BytesIO()
                    write_table(bio, v)
                    raw = bio.getvalue()

                    f.write(b"\x05")
                    f.write(struct.pack(">I", len(raw)))
                    f.write(raw)
                else:
                    val, typ = v
                    raw = encode_value(val, typ)

                    f.write(struct.pack(">B", TYPE_IDS[typ]))
                    f.write(struct.pack(">I", len(raw)))
                    f.write(raw)

# ---------------- CLI ----------------

if __name__ == "__main__":
    import argparse, os

    ap = argparse.ArgumentParser()
    ap.add_argument("input")
    ap.add_argument("--into")
    args = ap.parse_args()

    out = args.into or os.path.splitext(args.input)[0] + ".atff"
    data, links = parse_aap(args.input)
    write_atff(data, links, out)

    print("Written at path ", out)
