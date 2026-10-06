"""Quét thư mục nhạc, đọc tag và quản lý danh sách phát."""

import json
import re
import threading
import time
import zlib
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Dict, List, Optional

from .cau_hinh import DINH_DANG_HO_TRO
from .tim_kiem import bo_dau, khop_tu_khoa

_TEN_DANH_SACH_HOP_LE = re.compile(r"^[\w \-]{1,64}$")


@dataclass
class BaiHat:
    id: int
    tieu_de: str
    nghe_si: str
    album: str
    thoi_luong: int
    dinh_dang: str
    duong_dan: str  # tương đối so với thư mục nhạc
    chuan_hoa: str = ""

    def thong_tin(self) -> dict:
        du_lieu = asdict(self)
        du_lieu.pop("duong_dan")
        du_lieu.pop("chuan_hoa")
        return du_lieu


def _doc_tag(duong_dan: Path) -> dict:
    """Đọc tag bằng mutagen; lỗi thì dùng tên tệp."""
    ket_qua = {"tieu_de": duong_dan.stem, "nghe_si": "", "album": "", "thoi_luong": 0}
    try:
        import mutagen

        tep = mutagen.File(str(duong_dan), easy=True)
        if tep is None:
            return ket_qua
        if tep.info is not None and getattr(tep.info, "length", None):
            ket_qua["thoi_luong"] = int(tep.info.length)
        tags = tep.tags or {}
        for khoa_tag, khoa in (("title", "tieu_de"), ("artist", "nghe_si"), ("album", "album")):
            gia_tri = tags.get(khoa_tag)
            if gia_tri:
                ket_qua[khoa] = str(gia_tri[0]).strip() or ket_qua[khoa]
    except Exception:
        pass
    return ket_qua


class ThuVien:
    def __init__(self, thu_muc_nhac: Path, thu_muc_du_lieu: Path):
        self.thu_muc_nhac = Path(thu_muc_nhac)
        self.thu_muc_danh_sach = Path(thu_muc_du_lieu) / "danh_sach_phat"
        self._khoa = threading.RLock()
        self._bai_hat: Dict[int, BaiHat] = {}
        self.lan_quet_cuoi: Optional[float] = None

    def quet(self) -> int:
        """Quét lại toàn bộ thư mục nhạc, trả về số bài hát."""
        moi: Dict[int, BaiHat] = {}
        goc = self.thu_muc_nhac
        if goc.is_dir():
            for duong_dan in sorted(goc.rglob("*")):
                if not duong_dan.is_file() or duong_dan.suffix.lower() not in DINH_DANG_HO_TRO:
                    continue
                tuong_doi = duong_dan.relative_to(goc).as_posix()
                ma = zlib.crc32(tuong_doi.encode("utf-8")) & 0x7FFFFFFF
                while ma in moi:  # tránh trùng mã
                    ma = (ma + 1) & 0x7FFFFFFF
                tag = _doc_tag(duong_dan)
                bai = BaiHat(
                    id=ma,
                    tieu_de=tag["tieu_de"],
                    nghe_si=tag["nghe_si"],
                    album=tag["album"],
                    thoi_luong=tag["thoi_luong"],
                    dinh_dang=duong_dan.suffix.lower().lstrip("."),
                    duong_dan=tuong_doi,
                )
                bai.chuan_hoa = bo_dau(f"{bai.tieu_de} {bai.nghe_si} {bai.album} {duong_dan.stem}")
                moi[ma] = bai
        with self._khoa:
            self._bai_hat = moi
            self.lan_quet_cuoi = time.time()
        return len(moi)

    def so_bai(self) -> int:
        with self._khoa:
            return len(self._bai_hat)

    def lay_bai(self, ma: int) -> Optional[BaiHat]:
        with self._khoa:
            return self._bai_hat.get(ma)

    def duong_dan_tep(self, bai: BaiHat) -> Path:
        return self.thu_muc_nhac / bai.duong_dan

    def tim_kiem(self, tu_khoa: str = "", nghe_si: str = "", album: str = "") -> List[BaiHat]:
        nghe_si_chuan = bo_dau(nghe_si)
        album_chuan = bo_dau(album)
        with self._khoa:
            tat_ca = list(self._bai_hat.values())
        ket_qua = []
        for bai in tat_ca:
            if tu_khoa and not khop_tu_khoa(bai.chuan_hoa, tu_khoa):
                continue
            if nghe_si_chuan and nghe_si_chuan not in bo_dau(bai.nghe_si):
                continue
            if album_chuan and album_chuan not in bo_dau(bai.album):
                continue
            ket_qua.append(bai)
        ket_qua.sort(key=lambda b: (bo_dau(b.nghe_si), bo_dau(b.album), bo_dau(b.tieu_de)))
        return ket_qua

    def _dem_theo(self, truong: str) -> List[dict]:
        dem: Dict[str, int] = {}
        with self._khoa:
            for bai in self._bai_hat.values():
                ten = getattr(bai, truong)
                if ten:
                    dem[ten] = dem.get(ten, 0) + 1
        return [{"ten": ten, "so_bai": n} for ten, n in sorted(dem.items(), key=lambda x: bo_dau(x[0]))]

    def danh_sach_nghe_si(self) -> List[dict]:
        return self._dem_theo("nghe_si")

    def danh_sach_album(self) -> List[dict]:
        return self._dem_theo("album")

    # ---- Danh sách phát (lưu tệp JSON) ----
    @staticmethod
    def ten_hop_le(ten: str) -> bool:
        return bool(_TEN_DANH_SACH_HOP_LE.match(ten or "")) and ten.strip() == ten

    def _tep_danh_sach(self, ten: str) -> Path:
        return self.thu_muc_danh_sach / f"{ten}.json"

    def ds_phat_tat_ca(self) -> List[dict]:
        ket_qua = []
        if self.thu_muc_danh_sach.is_dir():
            for tep in sorted(self.thu_muc_danh_sach.glob("*.json")):
                ma = self._doc_ds_phat(tep.stem)
                if ma is not None:
                    ket_qua.append({"ten": tep.stem, "so_bai": len(ma), "bai_hat": ma})
        return ket_qua

    def _doc_ds_phat(self, ten: str) -> Optional[List[int]]:
        tep = self._tep_danh_sach(ten)
        try:
            du_lieu = json.loads(tep.read_text(encoding="utf-8"))
            return [int(x) for x in du_lieu.get("bai_hat", [])]
        except (OSError, ValueError, AttributeError, TypeError):
            return None

    def lay_ds_phat(self, ten: str) -> Optional[List[BaiHat]]:
        if not self.ten_hop_le(ten):
            return None
        cac_ma = self._doc_ds_phat(ten)
        if cac_ma is None:
            return None
        with self._khoa:
            return [self._bai_hat[m] for m in cac_ma if m in self._bai_hat]

    def luu_ds_phat(self, ten: str, cac_ma: List[int]) -> None:
        self.thu_muc_danh_sach.mkdir(parents=True, exist_ok=True)
        noi_dung = json.dumps({"ten": ten, "bai_hat": cac_ma}, ensure_ascii=False)
        self._tep_danh_sach(ten).write_text(noi_dung, encoding="utf-8")

    def xoa_ds_phat(self, ten: str) -> bool:
        if not self.ten_hop_le(ten):
            return False
        tep = self._tep_danh_sach(ten)
        if not tep.is_file():
            return False
        tep.unlink()
        return True
