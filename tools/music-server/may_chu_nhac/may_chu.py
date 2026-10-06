"""Ứng dụng FastAPI: API tìm kiếm, phát trực tiếp và quản lý danh sách phát."""

import hmac
import ipaddress
import logging
import threading
import time
from collections import defaultdict, deque
from contextlib import asynccontextmanager
from pathlib import Path
from typing import Deque, Dict, List, Optional

from fastapi import FastAPI, Header, HTTPException, Query, Request
from fastapi.exceptions import RequestValidationError
from fastapi.responses import FileResponse, JSONResponse, StreamingResponse
from pydantic import BaseModel

from . import PHIEN_BAN, phat
from .cau_hinh import CauHinh
from .mdns import QuangBaMdns
from .thu_vien import ThuVien

LOGGER = logging.getLogger("xiaozhi-music")
THU_MUC_TINH = Path(__file__).parent / "static"
CAC_BUOC_TOC_DO_MAU = (16000, 24000)


class NoiDungDanhSach(BaseModel):
    bai_hat: List[int]


def _la_mang_noi_bo(dia_chi: Optional[str]) -> bool:
    if not dia_chi:
        return False
    try:
        ip = ipaddress.ip_address(dia_chi)
    except ValueError:
        return dia_chi == "testclient"
    return ip.is_private or ip.is_loopback or ip.is_link_local


def _bat_theo_doi(thu_vien: ThuVien):
    """Tự quét lại khi thư mục nhạc thay đổi (gộp sự kiện trong 2 giây)."""
    try:
        from watchdog.events import FileSystemEventHandler
        from watchdog.observers import Observer
    except ImportError:
        return None

    class XuLy(FileSystemEventHandler):
        def __init__(self):
            self._hen_gio: Optional[threading.Timer] = None
            self._khoa = threading.Lock()

        def on_any_event(self, event):
            with self._khoa:
                if self._hen_gio:
                    self._hen_gio.cancel()
                self._hen_gio = threading.Timer(2.0, thu_vien.quet)
                self._hen_gio.daemon = True
                self._hen_gio.start()

    if not thu_vien.thu_muc_nhac.is_dir():
        return None
    quan_sat = Observer()
    quan_sat.schedule(XuLy(), str(thu_vien.thu_muc_nhac), recursive=True)
    quan_sat.daemon = True
    quan_sat.start()
    return quan_sat


def tao_ung_dung(cau_hinh: Optional[CauHinh] = None) -> FastAPI:
    cau_hinh = cau_hinh or CauHinh()
    thu_vien = ThuVien(cau_hinh.thu_muc_nhac, cau_hinh.thu_muc_du_lieu)
    lich_su_yeu_cau: Dict[str, Deque[float]] = defaultdict(deque)

    @asynccontextmanager
    async def vong_doi(_: FastAPI):
        so_bai = thu_vien.quet()
        LOGGER.info("Đã quét %d bài hát trong %s", so_bai, cau_hinh.thu_muc_nhac)
        quan_sat = _bat_theo_doi(thu_vien) if cau_hinh.tu_dong_quet else None
        mdns = QuangBaMdns(cau_hinh.cong, cau_hinh.ip_quang_ba) if cau_hinh.bat_mdns else None
        if mdns:
            mdns.bat_dau()
        yield
        if mdns:
            mdns.dung()
        if quan_sat:
            quan_sat.stop()

    ung_dung = FastAPI(title="Máy chủ nhạc XiaoZhi", version=PHIEN_BAN, lifespan=vong_doi)
    ung_dung.state.thu_vien = thu_vien
    ung_dung.state.cau_hinh = cau_hinh

    def loi_json(ma: int, thong_bao: str, tieu_de: Optional[dict] = None) -> JSONResponse:
        return JSONResponse({"loi": thong_bao}, status_code=ma, headers=tieu_de)

    @ung_dung.exception_handler(HTTPException)
    async def xu_ly_http(_: Request, loi: HTTPException):
        return loi_json(loi.status_code, str(loi.detail), getattr(loi, "headers", None))

    @ung_dung.exception_handler(RequestValidationError)
    async def xu_ly_tham_so(_: Request, loi: RequestValidationError):
        return loi_json(422, "Tham số không hợp lệ")

    @ung_dung.middleware("http")
    async def bao_ve(request: Request, call_next):
        ip = request.client.host if request.client else None
        if cau_hinh.chi_lan and not _la_mang_noi_bo(ip):
            return loi_json(403, "Chỉ cho phép truy cập từ mạng nội bộ")
        if request.url.path != "/health" and cau_hinh.api_key:
            khoa = request.headers.get("x-api-key") or request.query_params.get("khoa_api") or ""
            dang_nhap_web = request.url.path in ("/", "/static/index.html")
            if not dang_nhap_web and not hmac.compare_digest(khoa.encode(), cau_hinh.api_key.encode()):
                return loi_json(401, "Khóa API không hợp lệ hoặc bị thiếu")
        if cau_hinh.gioi_han_moi_phut > 0 and request.url.path != "/health":
            bay_gio = time.monotonic()
            hang = lich_su_yeu_cau[ip or "?"]
            while hang and bay_gio - hang[0] > 60:
                hang.popleft()
            if len(hang) >= cau_hinh.gioi_han_moi_phut:
                return loi_json(429, "Bạn gửi yêu cầu quá nhanh, vui lòng thử lại sau", {"Retry-After": "10"})
            hang.append(bay_gio)
        return await call_next(request)

    def lay_bai_hoac_loi(ma: int):
        bai = thu_vien.lay_bai(ma)
        if bai is None:
            raise HTTPException(404, "Không tìm thấy bài hát")
        return bai

    @ung_dung.get("/")
    def trang_chu():
        return FileResponse(THU_MUC_TINH / "index.html")

    @ung_dung.get("/health")
    def suc_khoe():
        return {"trang_thai": "tot"}

    @ung_dung.get("/api/trang-thai")
    def trang_thai():
        return {
            "phien_ban": PHIEN_BAN,
            "so_bai_hat": thu_vien.so_bai(),
            "lan_quet_cuoi": thu_vien.lan_quet_cuoi,
            "co_ffmpeg": phat.co_ffmpeg(),
            "can_khoa_api": bool(cau_hinh.api_key),
        }

    @ung_dung.post("/api/quet")
    def quet_lai():
        return {"so_bai_hat": thu_vien.quet()}

    @ung_dung.get("/api/bai-hat")
    def tim_bai_hat(
        tu_khoa: str = "",
        nghe_si: str = "",
        album: str = "",
        trang: int = Query(1, ge=1),
        so_luong: int = Query(20, ge=1, le=100),
    ):
        ket_qua = thu_vien.tim_kiem(tu_khoa, nghe_si, album)
        dau = (trang - 1) * so_luong
        return {
            "tong": len(ket_qua),
            "trang": trang,
            "so_luong": so_luong,
            "bai_hat": [b.thong_tin() for b in ket_qua[dau:dau + so_luong]],
        }

    @ung_dung.get("/api/bai-hat/{ma}")
    def chi_tiet_bai_hat(ma: int):
        return lay_bai_hoac_loi(ma).thong_tin()

    @ung_dung.get("/api/nghe-si")
    def ds_nghe_si():
        return {"nghe_si": thu_vien.danh_sach_nghe_si()}

    @ung_dung.get("/api/album")
    def ds_album():
        return {"album": thu_vien.danh_sach_album()}

    @ung_dung.get("/api/danh-sach-phat")
    def ds_phat():
        return {"danh_sach_phat": thu_vien.ds_phat_tat_ca()}

    @ung_dung.get("/api/danh-sach-phat/{ten}")
    def chi_tiet_ds_phat(ten: str):
        cac_bai = thu_vien.lay_ds_phat(ten)
        if cac_bai is None:
            raise HTTPException(404, "Không tìm thấy danh sách phát")
        return {"ten": ten, "so_bai": len(cac_bai), "bai_hat": [b.thong_tin() for b in cac_bai]}

    @ung_dung.post("/api/danh-sach-phat/{ten}")
    def luu_ds_phat(ten: str, noi_dung: NoiDungDanhSach):
        if not thu_vien.ten_hop_le(ten):
            raise HTTPException(400, "Tên danh sách phát không hợp lệ (tối đa 64 ký tự chữ, số, khoảng trắng, gạch)")
        thieu = [m for m in noi_dung.bai_hat if thu_vien.lay_bai(m) is None]
        if thieu:
            raise HTTPException(400, f"Không tìm thấy bài hát có mã {thieu[0]}")
        thu_vien.luu_ds_phat(ten, noi_dung.bai_hat)
        return {"ten": ten, "so_bai": len(noi_dung.bai_hat)}

    @ung_dung.delete("/api/danh-sach-phat/{ten}")
    def xoa_ds_phat(ten: str):
        if not thu_vien.xoa_ds_phat(ten):
            raise HTTPException(404, "Không tìm thấy danh sách phát")
        return {"da_xoa": ten}

    @ung_dung.get("/stream/{ma}")
    def phat_truc_tiep(
        ma: int,
        range_: Optional[str] = Header(None, alias="Range"),
        dinh_dang: str = "",
        toc_do_bit: str = "128k",
        toc_do_mau: int = 16000,
        bat_dau: float = Query(0, ge=0),
    ):
        bai = lay_bai_hoac_loi(ma)
        duong_dan = thu_vien.duong_dan_tep(bai)
        if not duong_dan.is_file():
            raise HTTPException(404, "Tệp nhạc không còn tồn tại")

        if dinh_dang:
            if dinh_dang not in ("mp3", "opus"):
                raise HTTPException(400, "Định dạng chuyển mã chỉ hỗ trợ mp3 hoặc opus")
            if not phat.kiem_tra_toc_do_bit(toc_do_bit):
                raise HTTPException(400, "Tốc độ bit không hợp lệ (ví dụ 64k, 128k)")
            if toc_do_mau not in CAC_BUOC_TOC_DO_MAU:
                raise HTTPException(400, "Tốc độ lấy mẫu chỉ hỗ trợ 16000 hoặc 24000")
            if phat.co_ffmpeg():
                lenh = phat.lenh_chuyen_ma(duong_dan, dinh_dang, toc_do_bit, toc_do_mau, bat_dau)
                return StreamingResponse(
                    phat.chuyen_ma(lenh),
                    media_type=phat.LOAI_NOI_DUNG[dinh_dang],
                    headers={"X-Chuyen-Ma": "co", "Cache-Control": "no-store"},
                )

        kich_thuoc = duong_dan.stat().st_size
        loai = phat.LOAI_NOI_DUNG.get(bai.dinh_dang, "application/octet-stream")
        tieu_de = {"Accept-Ranges": "bytes", "X-Chuyen-Ma": "khong"}
        try:
            khoang = phat.phan_tich_range(range_, kich_thuoc)
        except ValueError:
            return loi_json(416, "Phạm vi yêu cầu không hợp lệ", {"Content-Range": f"bytes */{kich_thuoc}"})
        if khoang is None:
            tieu_de["Content-Length"] = str(kich_thuoc)
            return StreamingResponse(phat.doc_tep(duong_dan, 0, kich_thuoc - 1), media_type=loai, headers=tieu_de)
        dau, cuoi = khoang
        tieu_de["Content-Range"] = f"bytes {dau}-{cuoi}/{kich_thuoc}"
        tieu_de["Content-Length"] = str(cuoi - dau + 1)
        return StreamingResponse(phat.doc_tep(duong_dan, dau, cuoi), status_code=206, media_type=loai, headers=tieu_de)

    return ung_dung
