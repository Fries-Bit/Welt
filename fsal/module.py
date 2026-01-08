# aat.py
import struct
import io
import types
import urllib.request

# ---------------- CORE ----------------

def _read_exact(f, n, ctx=""):
    b = f.read(n)
    if len(b) != n:
        raise ValueError(f"Corrupt ATFF ({ctx}): expected {n} bytes, got {len(b)}")
    return b

# ---------------- DEPENDENCY EXECUTION ----------------

def _run_link(link):
    """Execute a @link immediately."""
    original_link = link
    link = link.strip()

    # Accept both "@url" and plain "url" for convenience (but @ is preferred)
    if link.startswith("@"):
        url = link[1:].strip()
    elif link.startswith("http://") or link.startswith("https://"):
        url = link
    else:
        print(f"[ATFF] Skipping invalid link (must be @url or http(s)://): {original_link}")
        return

    try:
        code = urllib.request.urlopen(url).read().decode('utf-8')
        exec(code, {"__name__": f"__atff_dep_{url.split('/')[-1]}"})
    except Exception as e:
        print(f"[ATFF] FAILED to execute {url}: {e}")
        raise  # Optionally re-raise or just warn

# ---------------- TABLE READER ----------------

def _read_table(raw):
    f = io.BytesIO(raw)
    count = struct.unpack(">I", _read_exact(f, 4, "table count"))[0]
    ns = types.SimpleNamespace()

    for i in range(count):
        n = struct.unpack(">H", _read_exact(f, 2, f"table[{i}] key length"))[0]
        key = _read_exact(f, n, f"table[{i}] key").decode()

        tid = _read_exact(f, 1, f"table[{i}] type")[0]
        size = struct.unpack(">I", _read_exact(f, 4, f"table[{i}] size"))[0]
        raw_val = _read_exact(f, size, f"table[{i}] value")

        if tid == 1:
            val = struct.unpack(">i", raw_val)[0]
        elif tid == 2:
            val = struct.unpack(">f", raw_val)[0]
        elif tid == 3:
            val = raw_val != b"\x00"
        elif tid == 4:
            val = raw_val.decode()
        else:
            raise ValueError(f"Bad table type id {tid}")

        setattr(ns, key, val)

    return ns

# ---------------- LOADER ----------------

def load(file_path):
    """Load ATFF file and execute any @link dependencies immediately."""
    with open(file_path, "rb") as f:
        # check magic
        if _read_exact(f, 4, "magic") != b"ATFF":
            raise ValueError("Not an ATFF file")

        # ---------------- LINKS ----------------
        link_count = struct.unpack(">I", _read_exact(f, 4, "link count"))[0]
        links = []

        for i in range(link_count):
            n = struct.unpack(">H", _read_exact(f, 2, f"link[{i}] length"))[0]
            link = _read_exact(f, n, f"link[{i}] value").decode()
            links.append(link)

        # Execute all links (now more robust)
        for link in links:
            _run_link(link)

        # ---------------- SECTIONS ----------------
        section_count = struct.unpack(">I", _read_exact(f, 4, "section count"))[0]

        data = {}  # You might want to return structured data

        for si in range(section_count):
            n = struct.unpack(">H", _read_exact(f, 2, f"section[{si}] name length"))[0]
            name = _read_exact(f, n, f"section[{si}] name").decode()

            entry_count = struct.unpack(">I", _read_exact(f, 4, f"{name} entry count"))[0]
            section_data = {}

            for ei in range(entry_count):
                klen = struct.unpack(">H", _read_exact(f, 2, f"{name}[{ei}] key length"))[0]
                key = _read_exact(f, klen, f"{name}[{ei}] key").decode()

                tid = _read_exact(f, 1, f"{name}.{key} type")[0]
                size = struct.unpack(">I", _read_exact(f, 4, f"{name}.{key} size"))[0]
                raw = _read_exact(f, size, f"{name}.{key} value")

                if tid == 1:
                    val = struct.unpack(">i", raw)[0]
                elif tid == 2:
                    val = struct.unpack(">f", raw)[0]
                elif tid == 3:
                    val = raw != b"\x00"
                elif tid == 4:
                    val = raw.decode()
                elif tid == 5:
                    val = _read_table(raw)
                else:
                    raise ValueError(f"Unknown type id {tid} for {name}.{key}")

                section_data[key] = val

            data[name] = section_data

        return data