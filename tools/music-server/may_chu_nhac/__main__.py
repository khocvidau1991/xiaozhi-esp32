"""Chạy: python -m may_chu_nhac"""

import logging

import uvicorn

from .cau_hinh import CauHinh
from .may_chu import tao_ung_dung

if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO)
    cau_hinh = CauHinh()
    uvicorn.run(tao_ung_dung(cau_hinh), host=cau_hinh.dia_chi_nghe, port=cau_hinh.cong)
