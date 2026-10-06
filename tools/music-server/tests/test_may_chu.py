import wave
from pathlib import Path

import pytest
from fastapi.testclient import TestClient

from may_chu_nhac.cau_hinh import CauHinh
from may_chu_nhac.may_chu import tao_ung_dung
from may_chu_nhac.phat import phan_tich_range
from may_chu_nhac.thu_vien import ThuVien
from may_chu_nhac.tim_kiem import bo_dau, khop_tu_khoa


def tao_wav(duong_dan: Path, giay: float = 0.2):
    duong_dan.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(duong_dan), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(8000)
        w.writeframes(b"\x00\x00" * int(8000 * giay))


@pytest.fixture()
def thu_muc(tmp_path):
    tao_wav(tmp_path / "nhac" / "Sơn Tùng - Lạc Trôi.wav")
    tao_wav(tmp_path / "nhac" / "album" / "Em Của Ngày Hôm Qua.wav")
    (tmp_path / "nhac" / "ghi_chu.txt").write_text("khong phai nhac")
    return tmp_path


def tao_khach(thu_muc, **kw):
    cau_hinh = CauHinh(thu_muc_nhac=thu_muc / "nhac", thu_muc_du_lieu=thu_muc / "dl",
                       bat_mdns=False, tu_dong_quet=False, **kw)
    return TestClient(tao_ung_dung(cau_hinh))


def test_bo_dau():
    assert bo_dau("Sơn Tùng M-TP") == "son tung m tp"
    assert bo_dau("Đào Đức") == "dao duc"
    assert khop_tu_khoa(bo_dau("Sơn Tùng - Lạc Trôi"), "son tung")
    assert khop_tu_khoa(bo_dau("Sơn Tùng - Lạc Trôi"), "Lạc trôi")
    assert not khop_tu_khoa(bo_dau("Sơn Tùng"), "my tam")


def test_quet_thu_muc(thu_muc):
    tv = ThuVien(thu_muc / "nhac", thu_muc / "dl")
    assert tv.quet() == 2
    ma_cu = {b.id for b in tv.tim_kiem()}
    tao_wav(thu_muc / "nhac" / "moi.wav")
    assert tv.quet() == 3
    assert ma_cu <= {b.id for b in tv.tim_kiem()}  # mã ổn định sau khi quét lại


def test_tim_kiem_khong_dau(thu_muc):
    with tao_khach(thu_muc) as khach:
        d = khach.get("/api/bai-hat", params={"tu_khoa": "son tung"}).json()
        assert d["tong"] == 1 and d["bai_hat"][0]["tieu_de"].startswith("Sơn Tùng")
        assert khach.get("/api/bai-hat", params={"tu_khoa": "khong co"}).json()["tong"] == 0
        assert khach.get("/api/bai-hat/999").status_code == 404
        assert "Không tìm thấy" in khach.get("/api/bai-hat/999").json()["loi"]


def test_range(thu_muc):
    with tao_khach(thu_muc) as khach:
        ma = khach.get("/api/bai-hat", params={"tu_khoa": "lac troi"}).json()["bai_hat"][0]["id"]
        day_du = khach.get(f"/stream/{ma}")
        assert day_du.status_code == 200 and day_du.headers["content-type"] == "audio/wav"
        assert day_du.headers["accept-ranges"] == "bytes"
        r = khach.get(f"/stream/{ma}", headers={"Range": "bytes=10-19"})
        assert r.status_code == 206 and r.content == day_du.content[10:20]
        assert r.headers["content-range"] == f"bytes 10-19/{len(day_du.content)}"
        r = khach.get(f"/stream/{ma}", headers={"Range": "bytes=-5"})
        assert r.content == day_du.content[-5:]
        assert khach.get(f"/stream/{ma}", headers={"Range": "bytes=99999999-"}).status_code == 416


def test_phan_tich_range():
    assert phan_tich_range(None, 100) is None
    assert phan_tich_range("bytes=0-", 100) == (0, 99)
    assert phan_tich_range("bytes=50-500", 100) == (50, 99)
    with pytest.raises(ValueError):
        phan_tich_range("bytes=abc", 100)


def test_danh_sach_phat(thu_muc):
    with tao_khach(thu_muc) as khach:
        ma = khach.get("/api/bai-hat").json()["bai_hat"][0]["id"]
        assert khach.post("/api/danh-sach-phat/Yêu thích", json={"bai_hat": [ma]}).status_code == 200
        assert khach.get("/api/danh-sach-phat").json()["danh_sach_phat"][0]["so_bai"] == 1
        assert len(khach.get("/api/danh-sach-phat/Yêu thích").json()["bai_hat"]) == 1
        assert khach.post("/api/danh-sach-phat/..%2Fx", json={"bai_hat": []}).status_code in (400, 404)
        assert khach.post("/api/danh-sach-phat/a", json={"bai_hat": [1]}).status_code == 400
        assert khach.delete("/api/danh-sach-phat/Yêu thích").status_code == 200
        assert khach.delete("/api/danh-sach-phat/Yêu thích").status_code == 404


def test_api_key(thu_muc):
    with tao_khach(thu_muc, api_key="bi-mat") as khach:
        assert khach.get("/health").status_code == 200
        assert khach.get("/api/bai-hat").status_code == 401
        assert khach.get("/api/bai-hat", headers={"X-Api-Key": "bi-mat"}).status_code == 200
        assert khach.get("/api/bai-hat", params={"khoa_api": "bi-mat"}).status_code == 200


def test_gioi_han_toc_do(thu_muc):
    with tao_khach(thu_muc, gioi_han_moi_phut=2) as khach:
        ma_tt = [khach.get("/api/trang-thai").status_code for _ in range(3)]
        assert ma_tt == [200, 200, 429]


def test_chuyen_ma_tham_so_sai(thu_muc):
    with tao_khach(thu_muc) as khach:
        ma = khach.get("/api/bai-hat").json()["bai_hat"][0]["id"]
        assert khach.get(f"/stream/{ma}", params={"dinh_dang": "wma"}).status_code == 400
        assert khach.get(f"/stream/{ma}", params={"dinh_dang": "mp3", "toc_do_bit": "x;rm"}).status_code == 400
