# Bread Compact Wi-Fi LCD với TFT 1,54 inch

Biến thể `bread-compact-wifi-lcd-154` dùng màn hình ST7789 vuông 240 × 240, đảo màu, thứ tự RGB, offset 0/0 và SPI mode 0. Biến thể này bật giao diện LCD hiện đại, ngôn ngữ mặc định tiếng Việt và cấu hình GPIO qua web.

```sh
python3 scripts/build.py bread-compact-wifi-lcd --name bread-compact-wifi-lcd-154
```

Các tùy chọn `CONFIG_LCD_MODERN_UI`, `CONFIG_LANGUAGE_VI_VN` và `CONFIG_ENABLE_GPIO_WEB_CONFIG` đã được bật trong cấu hình biến thể. Tùy chọn web có mặc định `n` và chỉ áp dụng cho board Bread Compact Wi-Fi LCD.

## Trang cấu hình GPIO

- Khi thiết bị đang phát điểm truy cập cấu hình Wi-Fi, mở `http://192.168.4.1:8080/gpio`. Máy chủ cấu hình Wi-Fi bên thứ ba dùng cổng 80 và không cung cấp hook thêm URI, vì vậy trang GPIO chạy riêng trên cổng 8080 ở chế độ AP.
- Sau khi thiết bị kết nối Wi-Fi, mở `http://<địa-chỉ-IP-thiết-bị>/gpio` trên cùng mạng nội bộ (cổng 80).
- `GET /api/gpio` đọc cấu hình JSON; `POST /api/gpio` lưu cấu hình. Nút **Lưu** sẽ khởi động lại thiết bị để áp dụng.
- Nút **Khôi phục mặc định** đặt lại toàn bộ chân GPIO và khởi động lại. Giá trị `-1` tắt các chân tùy chọn như đèn nền, LED, đèn và các nút không sử dụng.
- Khi bật cấu hình GPIO qua web, chọn sai chân có thể khiến thiết bị không khởi động. Sau hơn ba lần khởi động liên tiếp kết thúc do panic/watchdog, thiết bị tự khôi phục chân mặc định.

## Sơ đồ chân mặc định

| Chức năng | GPIO |
| --- | ---: |
| Màn hình MOSI / CLK / DC / RST / CS | 47 / 21 / 40 / 45 / 41 |
| Đèn nền | 42 |
| LED tích hợp | 48 |
| Nút khởi động | 0 |
| Mic WS / SCK / DIN | 4 / 5 / 6 |
| Loa DOUT / BCLK / LRCK | 7 / 15 / 16 |
| Đèn điều khiển (MCP) | 18 |
| Nút cảm ứng / tăng âm lượng / giảm âm lượng | Không dùng |

GPIO 0 và 45 là chân strapping; thay đổi chúng có thể ảnh hưởng quá trình khởi động. GPIO 26–37 được từ chối vì có thể dành cho flash/PSRAM trên ESP32-S3.
