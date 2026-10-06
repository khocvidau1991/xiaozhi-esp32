# Giao diện LCD hiện đại (`CONFIG_LCD_MODERN_UI`)

Tùy chọn `CONFIG_LCD_MODERN_UI` bật giao diện LCD hiện đại: bảng màu mới, bong bóng chat bo góc lớn, đổ bóng nhẹ, thanh trạng thái có đường kẻ và phụ đề dạng viên thuốc. Mặc định tắt, giao diện của mọi board giữ nguyên như cũ.

Lưu ý: đổ bóng và gradient tốn RAM/CPU, nên thử trên màn hình 240x320 trước.

## Bật bằng menuconfig

```sh
idf.py menuconfig
```

Vào mục giao diện (kiểu tin nhắn) và chọn `Giao diện LCD hiện đại`.

## Bật cho một board qua `config.json`

Thêm vào `sdkconfig_append` của board:

```json
"sdkconfig_append": [
    "CONFIG_LCD_MODERN_UI=y"
]
```

Hiện tại tùy chọn được bật cho board `bread-compact-wifi-lcd`.
