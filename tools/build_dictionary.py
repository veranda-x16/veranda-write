#!/usr/bin/env python3
"""build_dictionary.py -- Veranda Write's spelling dictionaries (<LANG>.DIC).

    python3 tools/build_dictionary.py ID OUT.DIC SOURCE... [--licence=TEXT]
                                      [--apostrophe] [--latin1] [--max=N]
                                      [--freq=FREQ.txt] [--morfologik]

Taken over from Writer (the user's own earlier X16 word processor,
~/Projects/Writer, tools/build_dictionary.py): the same file format
(version 2: words front-coded in fixed 1 KB blocks, a two-level index of
the blocks' first words; Writer's docs/dictionary-format.md, Write's
docs/SPELLING.md), the same reading back of the output the way the X16
reads it (verify()). Changed for Write: no language pack -- Write's text
is always the Veranda set, ISO-8859-16 (DECISIONS W38), so a word is
encoded with Python's iso8859_16 codec; --apostrophe keeps words with an
apostrophe inside ("don't", "o'clock": English); --latin1 reads sources
in ISO-8859-1 (SCOWL's lists); several sources are merged.

SOURCE: a word list, one word per line (or a Morfologik dump with
--morfologik: "form<TAB>lemma<TAB>tags"). With --freq ("word count" per
line) only words in both are kept, most frequent first, up to --max.
Forms are normalised (NFC, lower case); forms that are not letters (but
an inner apostrophe with --apostrophe), all-capital abbreviations and
forms over 31 bytes are dropped, as are forms ISO-8859-16 cannot hold.
--licence copies a licence text next to the output as <OUT>.txt.
"""
import shutil
import struct
import sys
import unicodedata

MAGIC = b"WRDC"
VERSION = 2
BLOCK = 1024             # data block, key page and file unit
KEY = 16
KEYS_PER_PAGE = BLOCK // KEY
HEADER = 64
MAXWORD = 31
TOP_MAX = 2048 // KEY    # key pages Write holds in its index (2 KB: 128 pages, 8,192 blocks)


def clean(form, apostrophe=False):
    """Normalise one source form; None if it is not a dictionary word."""
    w = unicodedata.normalize("NFC", form.strip()).replace("’", "'")
    if not w:
        return None
    core = w.replace("'", "") if apostrophe else w
    if not core or not core.isalpha() or w[0] == "'" or w[-1] == "'" or "''" in w:
        return None
    if len(core) > 1 and core.isupper():
        return None                         # an abbreviation, AAP, PKO
    return w.lower()


def encode_words(words):
    """Encode normalised words in ISO-8859-16; return (sorted unique byte
    strings, number dropped: not in the set or too long)."""
    out, dropped = set(), 0
    for w in words:
        try:
            b = w.encode("iso8859_16")
        except UnicodeEncodeError:
            dropped += 1
            continue
        if len(b) <= MAXWORD:
            out.add(b)
        else:
            dropped += 1
    return sorted(out), dropped


def build(words):
    """Front-code sorted words into BLOCK-sized blocks. Entry: shared
    prefix length (1 byte), suffix length (1 byte), suffix. A block's
    first entry has prefix 0; 0xFF ends a block early."""
    blocks, keys = [], []
    cur, prev = bytearray(), b""
    for w in words:
        p = 0
        if cur:
            m = min(len(w), len(prev))
            while p < m and w[p] == prev[p]:
                p += 1
        entry = bytes([p, len(w) - p]) + w[p:]
        if cur and len(cur) + len(entry) > BLOCK - 1:
            blocks.append(bytes(cur) + b"\xff" * (BLOCK - len(cur)))
            cur = bytearray()
        if not cur:
            entry = bytes([0, len(w)]) + w
            keys.append(w[:KEY].ljust(KEY, b"\0"))
        cur += entry
        prev = w
    if cur:
        blocks.append(bytes(cur) + b"\xff" * (BLOCK - len(cur)))
    return blocks, keys


def layout(nblocks):
    """(key pages, unit of the first key page, unit of block 0)."""
    npages = (nblocks + KEYS_PER_PAGE - 1) // KEYS_PER_PAGE
    keys_unit = (HEADER + npages * KEY + BLOCK - 1) // BLOCK
    return npages, keys_unit, keys_unit + npages


def header(data):
    """The header fields the X16 reads."""
    (ver, key, nblk, nwords, npages, keys_unit,
     data_unit) = struct.unpack_from("<BBHIHHH", data, 4)
    return dict(ver=ver, key=key, nblocks=nblk, nwords=nwords, npages=npages,
                keys_unit=keys_unit, data_unit=data_unit)


def decode_block(blk):
    words, prev = [], b""
    i = 0
    while i < len(blk) and blk[i] != 0xFF:
        p, n = blk[i], blk[i + 1]
        w = prev[:p] + blk[i + 2:i + 2 + n]
        words.append(w)
        prev = w
        i += 2 + n
    return words


def read_words(data):
    """Every word in a .DIC, in order."""
    h = header(data)
    out = []
    for b in range(h["nblocks"]):
        at = (h["data_unit"] + b) * BLOCK
        out += decode_block(data[at:at + BLOCK])
    return out


def block_key(data, h, b):
    at = (h["keys_unit"] + b // KEYS_PER_PAGE) * BLOCK + (b % KEYS_PER_PAGE) * KEY
    return data[at:at + KEY]


def lookup(data, w):
    """The X16's lookup (src/spell/dict.c), step for step."""
    h = header(data)
    wk = w[:KEY].ljust(KEY, b"\0")
    top = [data[HEADER + i * KEY:HEADER + (i + 1) * KEY] for i in range(h["npages"])]
    # last page whose first key < w (or page 0) ...
    lo, hi = 0, h["npages"]
    while hi - lo > 1:
        mid = (lo + hi) // 2
        if top[mid] < wk:
            lo = mid
        else:
            hi = mid
    # ... then the last block in it whose key < w (or its first)
    base = lo * KEYS_PER_PAGE
    lo, hi = 0, min(KEYS_PER_PAGE, h["nblocks"] - base)
    while hi - lo > 1:
        mid = (lo + hi) // 2
        if block_key(data, h, base + mid) < wk:
            lo = mid
        else:
            hi = mid
    b = base + lo
    while True:
        at = (h["data_unit"] + b) * BLOCK
        ws = decode_block(data[at:at + BLOCK])
        if w in ws:
            return True
        if w < ws[-1]:
            return False
        # keys are truncated: the word can be in the next block when its
        # key equals the word's first KEY bytes
        b += 1
        if b >= h["nblocks"] or block_key(data, h, b) != wk:
            return False


def verify(data, words):
    """Read the file back the way the X16 does and check it."""
    h = header(data)
    assert data[:4] == MAGIC and h["ver"] == VERSION and h["key"] == KEY
    assert h["nwords"] == len(words)
    assert h["npages"] <= TOP_MAX, "top level larger than a bank"
    assert (HEADER + h["npages"] * KEY) <= h["keys_unit"] * BLOCK
    keys = [block_key(data, h, b) for b in range(h["nblocks"])]
    assert keys == sorted(keys), "keys not sorted"
    for p in range(h["npages"]):
        assert data[HEADER + p * KEY:HEADER + (p + 1) * KEY] == keys[p * KEYS_PER_PAGE]
    got = []
    for b in range(h["nblocks"]):
        at = (h["data_unit"] + b) * BLOCK
        ws = decode_block(data[at:at + BLOCK])
        assert ws, f"block {b} empty"
        assert data[at + BLOCK - 1] == 0xFF, f"block {b} has no end mark"
        assert ws[0][:KEY].ljust(KEY, b"\0") == keys[b], f"block {b} key"
        got += ws
    assert got == words, "decoded words differ"
    # no lookup may read the last unit: HostFS and CMDR-DOS end the channel
    assert len(data) == (h["data_unit"] + h["nblocks"] + 1) * BLOCK
    # the lookup itself, on a sample: block edges, long words, and
    # neighbours that are not words
    known = set(words)
    sample = set(words[::max(1, len(words) // 3000)])
    for b in range(0, h["nblocks"], max(1, h["nblocks"] // 500)):
        at = (h["data_unit"] + b) * BLOCK
        ws = decode_block(data[at:at + BLOCK])
        sample.update((ws[0], ws[-1]))
    sample.update(w for w in words if len(w) > KEY)
    for w in list(sample)[:20000]:
        assert lookup(data, w), f"lookup misses {w!r}"
        for v in (w + b"a", w[:-1], b"a" + w):
            if v and len(v) <= MAXWORD:
                assert lookup(data, v) == (v in known), f"lookup wrong on {v!r}"


def write(words, lang_id):
    blocks, keys = build(words)
    npages, keys_unit, data_unit = layout(len(blocks))
    if npages > TOP_MAX:
        sys.exit(f"build_dictionary: {npages} key pages; the reader holds {TOP_MAX}")
    hdr = MAGIC + struct.pack("<BBHIHHH", VERSION, KEY, len(blocks), len(words),
                              npages, keys_unit, data_unit)
    hdr += lang_id.encode("ascii").ljust(4, b"\0")
    data = bytearray(hdr.ljust(HEADER, b"\0"))
    data += b"".join(keys[p * KEYS_PER_PAGE] for p in range(npages))
    data += b"\0" * (keys_unit * BLOCK - len(data))
    for p in range(npages):
        page = b"".join(keys[p * KEYS_PER_PAGE:(p + 1) * KEYS_PER_PAGE])
        data += page.ljust(BLOCK, b"\0")
    data += b"".join(blocks)
    # One unit of padding at the end: HostFS closes a channel when its
    # last byte is read, and CMDR-DOS allows nothing after EOF, so no
    # lookup may ever read to the end of the file.
    data += b"\xff" * BLOCK
    return bytes(data), blocks, npages


def source_forms(path, morfologik, latin1):
    with open(path, encoding="iso-8859-1" if latin1 else "utf-8") as f:
        for line in f:
            yield line.split("\t", 1)[0] if morfologik else line


def main(argv):
    args = [a for a in argv[1:] if not a.startswith("--")]
    flags = {a[2:] for a in argv[1:] if a.startswith("--") and "=" not in a}
    opts = dict(a[2:].split("=", 1) for a in argv[1:] if a.startswith("--") and "=" in a)
    if len(args) < 3:
        print(__doc__.strip().splitlines()[2].strip(), file=sys.stderr)
        return 2
    lang_id, out, sources = args[0], args[1], args[2:]
    seen, rejected = set(), 0
    for src in sources:
        for form in source_forms(src, "morfologik" in flags, "latin1" in flags):
            w = clean(form, "apostrophe" in flags)
            if w is None:
                rejected += 1
            else:
                seen.add(w)
    limit = int(opts.get("max", "0")) or None
    if "freq" in opts:
        chosen = []
        with open(opts["freq"], encoding="utf-8") as f:
            for line in f:
                p = line.split()
                if len(p) == 2 and p[0] in seen:
                    chosen.append(p[0])
                    if limit and len(chosen) >= limit:
                        break
    else:
        chosen = sorted(seen)
        if limit:
            chosen = chosen[:limit]
    words, dropped = encode_words(chosen)
    data, blocks, npages = write(words, lang_id)
    verify(data, words)
    with open(out, "wb") as f:
        f.write(data)
    if "licence" in opts:
        shutil.copyfile(opts["licence"], out + ".txt")
    fill = sum(len(b.rstrip(b"\xff")) for b in blocks) / len(blocks)
    print(f"{out}: {len(words)} words ({rejected} source forms not words, "
          f"{dropped} not encodable or too long), {len(blocks)} blocks "
          f"(average {fill:.0f} B used), {npages} key pages, "
          f"{len(data) / 1048576:.2f} MB, top index {npages * KEY} B")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
