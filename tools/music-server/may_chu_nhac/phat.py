"""Phát trực tiếp tệp nhạc (HTTP Range) và chuyển mã bằng ffmpeg."""

import re
import shutil
import subprocess
from pathlib import Path
from typing import Iterator, Optional, Tuple

KICH_THUOC_KHOI = 64 * 1024

LOAI_NOI_DUNG = {
    "mp3": "audio/mpeg",
    "flac": "audio/flac",
    "wav": "audio/wav",
    "ogg": "audio/ogg",
    "m4a": "audio/mp4",
    "opus": "audio/ogg",
}

_RANGE = re.compile(r"^bytes=(\d*)-(\d*)$")
_TOC_DO_BIT = re.compile(r"^\d{2,3}k$")


def co_ffmpeg() -> bool:
    return shutil.which("ffmpeg") is not None


def phan_tich_range(tieu_de: Optional[str], kich_thuoc: int) -> Optional[Tuple[int, int]]:
    """Trả về (bắt đầu, kết thúc) bao gồm cả hai đầu; None nếu không có Range.

    Ném ValueError nếu Range không hợp lệ hoặc nằm ngoài tệp (416).
    """
    if not tieu_de:
        return None
    khop = _RANGE.match(tieu_de.strip())
    if not khop or (not khop.group(1) and not khop.group(2)):
        raise ValueError("Range không hợp lệ")
    dau, cuoi = khop.group(1), khop.group(2)
    if not dau:  # bytes=-N : N byte cuối
        so_byte = int(cuoi)
        if so_byte == 0:
            raise ValueError("Range không hợp lệ")
        return max(kich_thuoc - so_byte, 0), kich_thuoc - 1
    bat_dau = int(dau)
    ket_thuc = int(cuoi) if cuoi else kich_thuoc - 1
    if bat_dau >= kich_thuoc or ket_thuc < bat_dau:
        raise ValueError("Range nằm ngoài tệp")
    return bat_dau, min(ket_thuc, kich_thuoc - 1)


def doc_tep(duong_dan: Path, bat_dau: int, ket_thuc: int) -> Iterator[bytes]:
    con_lai = ket_thuc - bat_dau + 1
    with open(duong_dan, "rb") as tep:
        tep.seek(bat_dau)
        while con_lai > 0:
            khoi = tep.read(min(KICH_THUOC_KHOI, con_lai))
            if not khoi:
                break
            con_lai -= len(khoi)
            yield khoi


def kiem_tra_toc_do_bit(gia_tri: str) -> bool:
    return bool(_TOC_DO_BIT.match(gia_tri))


def lenh_chuyen_ma(duong_dan: Path, dinh_dang: str, toc_do_bit: str, toc_do_mau: int, bat_dau: float) -> list:
    """Tạo dòng lệnh ffmpeg: MP3 hoặc Ogg/Opus, đơn kênh."""
    lenh = ["ffmpeg", "-nostdin", "-loglevel", "error"]
    if bat_dau > 0:
        lenh += ["-ss", f"{bat_dau:.3f}"]
    lenh += ["-i", str(duong_dan), "-vn", "-map_metadata", "-1", "-ac", "1", "-ar", str(toc_do_mau)]
    if dinh_dang == "mp3":
        lenh += ["-c:a", "libmp3lame", "-b:a", toc_do_bit, "-f", "mp3"]
    else:
        lenh += ["-c:a", "libopus", "-b:a", toc_do_bit, "-application", "audio", "-frame_duration", "60", "-f", "ogg"]
    lenh.append("pipe:1")
    return lenh


def chuyen_ma(lenh: list) -> Iterator[bytes]:
    tien_trinh = subprocess.Popen(lenh, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
    try:
        while True:
            khoi = tien_trinh.stdout.read(KICH_THUOC_KHOI)
            if not khoi:
                break
            yield khoi
    finally:
        if tien_trinh.poll() is None:
            tien_trinh.kill()
        tien_trinh.stdout.close()
        tien_trinh.wait()
