"""Generate the repository's dependency-free multi-size Windows icon."""

from pathlib import Path
import struct


SIZES = (16, 32, 48, 256)


def draw_icon(size: int) -> bytes:
    pixels = bytearray(size * size * 4)

    def pixel(x: int, y: int, color: tuple[int, int, int, int]) -> None:
        if 0 <= x < size and 0 <= y < size:
            offset = (y * size + x) * 4
            red, green, blue, alpha = color
            pixels[offset : offset + 4] = bytes((blue, green, red, alpha))

    def rectangle(left: int, top: int, right: int, bottom: int,
                  color: tuple[int, int, int, int]) -> None:
        for y in range(top, bottom):
            for x in range(left, right):
                pixel(x, y, color)

    margin = max(1, round(size / 16))
    border = max(1, round(size / 32))
    rectangle(margin, margin, size - margin, size - margin, (255, 96, 0, 255))
    rectangle(margin + border, margin + border, size - margin - border,
              size - margin - border, (16, 23, 34, 255))

    baseline = size - margin - border - max(2, round(size / 8))
    bar_width = max(1, round(size / 8))
    gap = max(1, round(size / 10))
    start = (size - (bar_width * 3 + gap * 2)) // 2
    for index, height_ratio in enumerate((0.34, 0.58, 0.82)):
        height = max(1, round((size - 2 * margin - 2 * border) * height_ratio))
        left = start + index * (bar_width + gap)
        rectangle(left, baseline - height, left + bar_width, baseline, (76, 220, 154, 255))

    # ICO DIBs are stored bottom-up and include an all-clear 1-bit alpha mask.
    xor = bytearray()
    for y in range(size - 1, -1, -1):
        start_offset = y * size * 4
        xor.extend(pixels[start_offset : start_offset + size * 4])
    and_stride = ((size + 31) // 32) * 4
    and_mask = bytes(and_stride * size)
    header = struct.pack("<IIIHHIIIIII", 40, size, size * 2, 1, 32, 0,
                         len(xor), 0, 0, 0, 0)
    return header + xor + and_mask


def main() -> None:
    frames = [(size, draw_icon(size)) for size in SIZES]
    offset = 6 + len(frames) * 16
    entries = []
    payload = []
    for size, frame in frames:
        entries.append(struct.pack("<BBBBHHII", 0 if size == 256 else size,
                                   0 if size == 256 else size, 0, 0, 1, 32,
                                   len(frame), offset))
        payload.append(frame)
        offset += len(frame)

    output = Path(__file__).resolve().parents[1] / "assets" / "sysglance.ico"
    output.write_bytes(struct.pack("<HHH", 0, 1, len(frames)) + b"".join(entries) +
                       b"".join(payload))


if __name__ == "__main__":
    main()
