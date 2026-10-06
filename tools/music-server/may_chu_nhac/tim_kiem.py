"""Chuẩn hóa văn bản để tìm kiếm có dấu và không dấu."""

import re
import unicodedata

_KHOANG_TRANG = re.compile(r"[^0-9a-z]+")


def bo_dau(van_ban: str) -> str:
    """Bỏ dấu tiếng Việt, chuyển chữ thường và gộp ký tự đặc biệt thành khoảng trắng."""
    if not van_ban:
        return ""
    van_ban = van_ban.replace("đ", "d").replace("Đ", "D")
    phan_ra = unicodedata.normalize("NFKD", van_ban)
    khong_dau = "".join(c for c in phan_ra if not unicodedata.combining(c))
    return _KHOANG_TRANG.sub(" ", khong_dau.lower()).strip()


def khop_tu_khoa(kho_chuan_hoa: str, tu_khoa: str) -> bool:
    """Mọi từ trong từ khóa phải xuất hiện trong chuỗi đã chuẩn hóa."""
    cac_tu = bo_dau(tu_khoa).split()
    return all(tu in kho_chuan_hoa for tu in cac_tu)
