#!/usr/bin/env python3
import argparse
import csv
from collections import Counter
from pathlib import Path


def parse_segments(payload_hex):
    payload_hex = payload_hex.strip()
    if not payload_hex:
        return 0
    if len(payload_hex) % 2 != 0:
        raise ValueError(f"payload hex does not match length: {payload_hex!r}")
    data = bytes.fromhex(payload_hex)
    i = 0
    segments = 0
    ad_types = set()
    while i < len(data):
        seg_len = data[i]
        i += 1
        if seg_len == 0:
            break
        if i + seg_len > len(data):
            print(f"error: segment length {seg_len} exceeds remaining {len(data) - i}")
            exit()
        if seg_len >= 1:
            ad_types.add(data[i])
        i += seg_len
        segments += 1
    return segments, ad_types


def read_counts(csv_path, payload_col):
    counts = []
    ad_type_sets = []
    bad_rows = []
    with csv_path.open(newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        if reader.fieldnames is None:
            raise ValueError("missing CSV header row")
        header_map = {name.strip(): name for name in reader.fieldnames}
        if payload_col not in header_map:
            raise ValueError(
                f"payload column {payload_col!r} not found; have {sorted(header_map)}"
            )
        payload_key = header_map[payload_col]
        for idx, row in enumerate(reader, start=2):
            payload = row.get(payload_key, "")
            try:
                seg_count, ad_types = parse_segments(payload)
                counts.append(seg_count)
                ad_type_sets.append(ad_types)
            except ValueError as exc:
                bad_rows.append((idx, str(exc), payload))
    return counts, ad_type_sets, bad_rows


def plot_histogram(counts, output_path):
    import matplotlib.pyplot as plt

    hist = Counter(counts)
    xs = sorted(hist)
    ys = [hist[x] for x in xs]

    fig, ax = plt.subplots(figsize=(8, 4.5))
    ax.bar(xs, ys, width=0.8, color="#2a6f6f")
    ax.set_xlabel("Number of segments per payload")
    ax.set_ylabel("Count")
    ax.set_title("BLE Payload Segment Count Histogram")
    ax.set_xticks(xs)
    fig.tight_layout()
    fig.savefig(output_path, dpi=150)


AD_TYPE_LABELS = {
    0x01: "Flags",
    0x02: "16-bit UUIDs (incomplete)",
    0x03: "16-bit UUIDs (complete)",
    0x04: "32-bit UUIDs (incomplete)",
    0x05: "32-bit UUIDs (complete)",
    0x06: "128-bit UUIDs (incomplete)",
    0x07: "128-bit UUIDs (complete)",
    0x08: "Shortened local name",
    0x09: "Complete local name",
    0x0A: "Tx power level",
    0x0D: "Class of device",
    0x0E: "Simple pairing hash",
    0x0F: "Simple pairing randomizer",
    0x10: "Device ID",
    0x14: "LE role",
    0x15: "16-bit SVC data",
    0x16: "16-bit SVC data",
    0x17: "Public target address",
    0x18: "Random target address",
    0x19: "Appearance",
    0x1A: "Advertising interval",
    0x1B: "LE device address",
    0x1C: "LE role",
    0x1D: "Simple pairing hash-192",
    0x1E: "Simple pairing randomizer-192",
    0x1F: "32-bit SVC data",
    0x20: "32-bit SVC data",
    0x21: "128-bit SVC data",
    0x22: "LE secure connections confirm",
    0x23: "LE secure connections random",
    0x24: "URI",
    0x25: "Indoor positioning",
    0x26: "Transport discovery data",
    0x27: "LE supported features",
    0x28: "Channel map update indication",
    0x29: "PB-ADV",
    0x2A: "Mesh message",
    0x2B: "Mesh beacon",
    0x2C: "BIGInfo",
    0x2D: "Broadcast code",
    0x2E: "Resolvable set identifier",
    0x2F: "Advertising interval long",
    0x30: "Broadcast name",
    0x31: "Encrypted advertising data",
    0x32: "Periodic adv response timing",
    0x34: "Electronic shelf label",
    0x35: "3D information data",
    0x36: "Manufacturer data",
}


def plot_ad_type_histogram(ad_type_sets, output_path):
    import matplotlib.pyplot as plt

    counts = Counter()
    for ad_types in ad_type_sets:
        for ad_type in ad_types:
            counts[ad_type] += 1

    xs = sorted(counts)
    ys = [counts[x] for x in xs]
    labels = [f"0x{x:02X} {AD_TYPE_LABELS.get(x, 'Unknown')}" for x in xs]

    fig, ax = plt.subplots(figsize=(10, 4.5))
    ax.bar(range(len(xs)), ys, width=0.8, color="#4b7b3f")
    ax.set_xlabel("AD type")
    ax.set_ylabel("Advertisements containing type (log 10)")
    ax.set_title("BLE AD Type Presence Histogram")
    ax.set_yscale("log")
    ax.set_xticks(range(len(xs)))
    ax.set_xticklabels(labels, rotation=45, ha="right")
    fig.tight_layout()
    fig.savefig(output_path, dpi=150)


def main():
    counts, ad_type_sets, bad_rows = read_counts(Path("unique2.csv"), "payload")
    plot_histogram(counts, "segments.png")
    plot_ad_type_histogram(ad_type_sets, "types.png")

    total = len(counts)
    if bad_rows:
        print(
            "Wrote segments.png and types.png "
            f"(parsed {total} rows, {len(bad_rows)} errors)."
        )
        for row_num, err, payload in bad_rows[:5]:
            print(f"Row {row_num}: {err} (payload={payload!r})")
        if len(bad_rows) > 5:
            print(f"... {len(bad_rows) - 5} more errors")
    else:
        print(f"Wrote segments.png and types.png (parsed {total} rows).")


if __name__ == "__main__":
    main()
