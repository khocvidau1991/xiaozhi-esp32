# Máy chủ nhạc cục bộ XiaoZhi

Máy chủ nhạc (Python 3, FastAPI) chạy trên máy tính/NAS/Raspberry Pi trong cùng mạng LAN với thiết bị XiaoZhi. Xem phần thiết bị tại [`docs/local-music.md`](../../docs/local-music.md).

## Cài đặt

```sh
cd tools/music-server
pip install -r requirements.txt
sudo apt install ffmpeg          # cần để chuyển mã cho thiết bị
MUSIC_DIR=/duong/dan/nhac python -m may_chu_nhac
```

Giao diện web: `http://<ip-may>:8765/`.

## Docker

```sh
MUSIC_HOST_DIR=/duong/dan/nhac API_KEY=khoa-bi-mat docker compose up -d --build
```

`docker-compose.yml` dùng `network_mode: host` để quảng bá mDNS. Nếu dùng mạng bridge, bỏ dòng đó, mở cổng 8765 và đặt `ADVERTISE_IP` là IP của máy chủ.

## Thư mục nhạc

Đặt tệp `.mp3`, `.flac`, `.wav`, `.ogg`, `.m4a` trong `MUSIC_DIR` (có thể có thư mục con). Tag (tiêu đề, nghệ sĩ, album, thời lượng) đọc bằng `mutagen`; thiếu tag thì dùng tên tệp. Thư mục được quét lại tự động khi có thay đổi (watchdog) hoặc qua `POST /api/quet`.

## Biến môi trường

| Biến | Mặc định | Ý nghĩa |
|------|----------|---------|
| `MUSIC_DIR` | `./music` | Thư mục nhạc |
| `DATA_DIR` | `./du_lieu` | Nơi lưu danh sách phát (JSON) |
| `PORT` / `HOST` | `8765` / `0.0.0.0` | Cổng / địa chỉ lắng nghe |
| `API_KEY` | rỗng | Nếu đặt, mọi API (trừ `/health`) cần header `X-Api-Key` (hoặc `?khoa_api=`) |
| `LAN_ONLY` | `true` | Chỉ chấp nhận yêu cầu từ địa chỉ mạng nội bộ |
| `RATE_LIMIT_PER_MINUTE` | `600` | Giới hạn yêu cầu mỗi IP mỗi phút (0 = tắt) |
| `ENABLE_MDNS` | `true` | Quảng bá `_xiaozhi-music._tcp` / `xiaozhi-music.local` |
| `ADVERTISE_IP` | tự đoán | IP quảng bá qua mDNS |
| `WATCH_MUSIC_DIR` | `true` | Tự quét lại khi thư mục thay đổi |

## Bảo mật

Máy chủ chỉ dành cho LAN: đừng mở cổng ra Internet. Nên đặt `API_KEY`; thiết bị gửi khóa trong URL luồng (`khoa_api`), nên dùng khóa riêng cho máy chủ nhạc. Khóa API không bao giờ được ghi vào log phía thiết bị.

## API (lỗi trả về `{"loi": "..."}` bằng tiếng Việt)

| Phương thức | Đường dẫn | Mô tả |
|-------------|-----------|-------|
| GET | `/api/bai-hat?tu_khoa=&nghe_si=&album=&trang=&so_luong=` | Tìm kiếm có dấu/không dấu |
| GET | `/api/bai-hat/{id}` | Chi tiết bài hát |
| GET | `/api/nghe-si`, `/api/album` | Danh sách nghệ sĩ, album |
| GET | `/api/danh-sach-phat`, `/api/danh-sach-phat/{ten}` | Danh sách phát |
| POST | `/api/danh-sach-phat/{ten}` | Lưu danh sách phát, thân `{"bai_hat": [id, ...]}` |
| DELETE | `/api/danh-sach-phat/{ten}` | Xóa danh sách phát |
| POST | `/api/quet` | Quét lại thư viện |
| GET | `/stream/{id}` | Phát trực tiếp, hỗ trợ `Range` |
| GET | `/api/trang-thai`, `/health` | Trạng thái |

`/stream/{id}` nhận thêm `dinh_dang=mp3|opus`, `toc_do_bit=128k`, `toc_do_mau=16000|24000`, `bat_dau=<giây>` để chuyển mã bằng ffmpeg (đơn kênh). Thiết bị dùng `dinh_dang=opus` (Ogg/Opus). Không có ffmpeg thì trả tệp gốc (không phù hợp cho thiết bị).

## Kiểm thử

```sh
python -m pytest tests -q
```
