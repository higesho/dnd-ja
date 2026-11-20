"""patch_dlls.py のパッチ一覧を、日本語化ローダー（wsock32.dll）用の C ヘッダに変換する。

ファイルオフセットを RVA に直し、元のバイト列（照合用）と新しいバイト列を埋め込む。
4 バイトのアドレス（push 即値の付け替え）は is_va=1 とし、ローダー側で実際の読み込み位置に合わせて補正する。

使い方: python gen_loader.py   → jp_mod/loader/patches.h
"""
import os
import sys

import pefile

sys.path.insert(0, os.path.dirname(__file__))
import patch_dlls  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..")


def off2rva(pe, off):
    for s in pe.sections:
        if s.PointerToRawData <= off < s.PointerToRawData + s.SizeOfRawData:
            return off - s.PointerToRawData + s.VirtualAddress
    raise ValueError(hex(off))


def carr(b):
    return "{" + ",".join("0x%02x" % x for x in b) + "}"


def main():
    out = ["/* 自動生成（gen_loader.py）。手で編集しない。 */", "#pragma once", ""]
    tables = []
    for mod, patches in (("sneg.dll", patch_dlls.SNEG_PATCHES), ("ddraw.dll", patch_dlls.DDRAW_PATCHES)):
        pe = pefile.PE(os.path.join(ROOT, "backup", mod), fast_load=True)
        base = pe.OPTIONAL_HEADER.ImageBase
        name = mod.split(".")[0]
        rows = []
        for k, (off, old, new, desc) in enumerate(patches):
            rva = off2rva(pe, off)
            is_va = 1 if desc.startswith("ref ") else 0
            out.append("static const unsigned char %s_old_%d[] = %s;" % (name, k, carr(old)))
            out.append("static const unsigned char %s_new_%d[] = %s;" % (name, k, carr(new)))
            rows.append("  {0x%x, %d, %s_old_%d, %s_new_%d, %d}," % (rva, len(old), name, k, name, k, is_va))
        tables.append((name, base, rows))
    out.append("")
    out.append("typedef struct { unsigned rva; unsigned len; const unsigned char *old_; const unsigned char *new_; int is_va; } JaPatch;")
    for name, base, rows in tables:
        out.append("#define %s_PREFERRED_BASE 0x%xu" % (name.upper(), base))
        out.append("static const JaPatch %s_patches[] = {" % name)
        out += rows
        out.append("};")
    path = os.path.join(ROOT, "loader", "patches.h")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    open(path, "w", encoding="utf-8").write("\n".join(out) + "\n")
    print("sneg %d 件 / ddraw %d 件 → %s" % (len(tables[0][2]), len(tables[1][2]), path))


if __name__ == "__main__":
    main()
