# Nhạc cục bộ (Local Music)

Thiết bị phát nhạc từ **máy chủ nhạc cục bộ** chạy trong cùng mạng WiFi/LAN (máy tính, NAS, Raspberry Pi). Không phụ thuộc dịch vụ đám mây. AI điều khiển nhạc bằng các công cụ MCP `self.music.*`.

## Bật tính năng

Tùy chọn mặc định **tắt**; khi tắt, không có mã nào được biên dịch thêm.

```sh
idf.py menuconfig   # Xiaozhi Assistant -> Nhạc cục bộ (Local Music) -> ENABLE_LOCAL_MUSIC
```

Hoặc thêm vào `sdkconfig_append` của biến thể board trong `config.json`:

```json
"sdkconfig_append": ["CONFIG_ENABLE_LOCAL_MUSIC=y"]
```

| Kconfig | Mặc định | Ý nghĩa |
|---------|----------|---------|
| `ENABLE_LOCAL_MUSIC` | n | Bật module nhạc cục bộ |
| `MUSIC_SERVER_DEFAULT_HOST` | rỗng | Rỗng = tự tìm bằng mDNS (`_xiaozhi-music._tcp`) |
| `MUSIC_SERVER_DEFAULT_PORT` | 8765 | Cổng máy chủ |
| `MUSIC_PLAYER_BUFFER_KB` | 32 | Dung lượng tối đa của một phản hồi JSON |
| `MUSIC_PREFER_TRANSCODE` | y | Luồng nhẹ Ogg/Opus 16 kHz 32 kbps (tắt: 24 kHz 48 kbps) |

Phụ thuộc: component `espressif/mdns`.

## Cấu hình máy chủ

Mặc định thiết bị tự tìm máy chủ bằng mDNS. Có thể đặt địa chỉ thủ công (lưu trong NVS, namespace `music`) bằng công cụ user-only `self.music.set_server` (`host`, `port`, `api_key`). Khóa API không được ghi vào log.

Cài đặt và chạy máy chủ: xem [`tools/music-server/README.md`](../tools/music-server/README.md). Máy chủ cần có **ffmpeg**, vì thiết bị chỉ giải mã luồng Ogg/Opus đơn kênh (do máy chủ chuyển mã từ MP3/FLAC/...).

## Cách hoạt động trên thiết bị

- `main/music/music_client.*`: truy vấn REST bằng HTTP (thử lại 2 lần, timeout 5 giây).
- `main/music/music_player.*`: hàng đợi, lặp, ngẫu nhiên, tạm dừng/tiếp tục/tua (máy chủ bắt đầu luồng từ `bat_dau` giây).
- Phát luồng dùng lại `NotifyPlayer` và trạng thái `kDeviceStateNotifying`. Nói từ khóa đánh thức hoặc bấm nút sẽ dừng nhạc (giữ vị trí, có thể "tiếp tục") và vào chế độ nghe.
- Khi AI vừa gọi công cụ, nhạc bắt đầu sau khi thiết bị về trạng thái rảnh (tối đa 20 giây).
- Tên bài/nghệ sĩ hiển thị bằng `Display::SetChatMessage`.

Lưu ý: khi phát nhạc thiết bị ở trạng thái Notifying nên xử lý giọng nói bị tắt (chỉ còn từ khóa đánh thức); tính năng tự giảm âm lượng khi AI nói (ducking) chưa được hỗ trợ.

Kiểm chứng: máy chủ nhạc có kiểm thử `pytest`; phần firmware chưa được biên dịch bằng `idf.py` hay thử trên phần cứng trong môi trường này.

## Ví dụ câu nói

- "Mở bài Lạc Trôi của Sơn Tùng" → `self.music.play`
- "Chuyển bài" → `self.music.next`; "Quay lại bài trước" → `self.music.previous`
- "Phát ngẫu nhiên" → `self.music.set_shuffle`
- "Lặp lại bài này" → `self.music.set_repeat` (`one`)
- "Tạm dừng" / "Tiếp tục" / "Dừng nhạc"
- "Tua đến phút thứ hai" → `self.music.seek` (`seconds`: 120)

Danh sách đầy đủ công cụ: [`mcp-usage.md`](./mcp-usage.md).
