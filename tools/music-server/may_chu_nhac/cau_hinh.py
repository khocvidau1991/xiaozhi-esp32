"""Cấu hình đọc từ biến môi trường."""

import os
from dataclasses import dataclass, field
from pathlib import Path

DINH_DANG_HO_TRO = (".mp3", ".flac", ".wav", ".ogg", ".m4a")


def _doc_int(ten: str, mac_dinh: int) -> int:
    try:
        return int(os.environ.get(ten, mac_dinh))
    except ValueError:
        return mac_dinh


def _doc_bool(ten: str, mac_dinh: bool) -> bool:
    gia_tri = os.environ.get(ten)
    if gia_tri is None:
        return mac_dinh
    return gia_tri.strip().lower() in ("1", "true", "yes", "co", "on")


@dataclass
class CauHinh:
    thu_muc_nhac: Path = field(default_factory=lambda: Path(os.environ.get("MUSIC_DIR", "./music")))
    thu_muc_du_lieu: Path = field(default_factory=lambda: Path(os.environ.get("DATA_DIR", "./du_lieu")))
    api_key: str = field(default_factory=lambda: os.environ.get("API_KEY", ""))
    cong: int = field(default_factory=lambda: _doc_int("PORT", 8765))
    dia_chi_nghe: str = field(default_factory=lambda: os.environ.get("HOST", "0.0.0.0"))
    chi_lan: bool = field(default_factory=lambda: _doc_bool("LAN_ONLY", True))
    gioi_han_moi_phut: int = field(default_factory=lambda: _doc_int("RATE_LIMIT_PER_MINUTE", 600))
    bat_mdns: bool = field(default_factory=lambda: _doc_bool("ENABLE_MDNS", True))
    ip_quang_ba: str = field(default_factory=lambda: os.environ.get("ADVERTISE_IP", ""))
    tu_dong_quet: bool = field(default_factory=lambda: _doc_bool("WATCH_MUSIC_DIR", True))
