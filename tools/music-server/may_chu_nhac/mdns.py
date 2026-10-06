"""Quảng bá dịch vụ qua mDNS/Zeroconf để thiết bị tự tìm thấy máy chủ."""

import logging
import socket
from typing import Optional

LOGGER = logging.getLogger("xiaozhi-music")
LOAI_DICH_VU = "_xiaozhi-music._tcp.local."
TEN_MAY = "xiaozhi-music.local."


def lay_ip_lan() -> str:
    """Đoán địa chỉ IP trong mạng LAN (không gửi dữ liệu ra ngoài)."""
    may_do = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        may_do.connect(("10.255.255.255", 1))
        return may_do.getsockname()[0]
    except OSError:
        return "127.0.0.1"
    finally:
        may_do.close()


class QuangBaMdns:
    def __init__(self, cong: int, ip: str = ""):
        self.cong = cong
        self.ip = ip or lay_ip_lan()
        self._zc = None
        self._thong_tin = None

    def bat_dau(self) -> bool:
        try:
            from zeroconf import ServiceInfo, Zeroconf

            self._thong_tin = ServiceInfo(
                LOAI_DICH_VU,
                f"xiaozhi-music.{LOAI_DICH_VU}",
                addresses=[socket.inet_aton(self.ip)],
                port=self.cong,
                properties={"phien_ban": "1"},
                server=TEN_MAY,
            )
            self._zc = Zeroconf()
            self._zc.register_service(self._thong_tin)
            LOGGER.info("Đã quảng bá mDNS %s tại %s:%d", TEN_MAY, self.ip, self.cong)
            return True
        except Exception as loi:  # noqa: BLE001
            LOGGER.warning("Không thể bật mDNS: %s", loi)
            return False

    def dung(self) -> None:
        if self._zc is not None:
            try:
                self._zc.unregister_service(self._thong_tin)
                self._zc.close()
            except Exception:  # noqa: BLE001
                pass
            self._zc = None
