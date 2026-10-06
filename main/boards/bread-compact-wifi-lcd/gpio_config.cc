#include "gpio_config.h"

#include "config.h"
#include "settings.h"

#include <esp_log.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <soc/soc_caps.h>

#include <array>

#define TAG "CauHinhGpio"

namespace {
constexpr char NAMESPACE_GPIO[] = "gpio";
constexpr char KHOA_SO_LAN_LOI[] = "failures";

void XoaSoLanLoiSauKhiKhoiDongOnDinh(void*) {
    vTaskDelay(pdMS_TO_TICKS(60000));
    {
        Settings cai_dat(NAMESPACE_GPIO, true);
        cai_dat.SetInt(KHOA_SO_LAN_LOI, 0);
    }
    vTaskDelete(nullptr);
}

bool LaLoiKhoiDong(esp_reset_reason_t ly_do) {
    return ly_do == ESP_RST_PANIC || ly_do == ESP_RST_INT_WDT ||
           ly_do == ESP_RST_TASK_WDT || ly_do == ESP_RST_WDT ||
           ly_do == ESP_RST_BROWNOUT;
}
}

CauHinhGpio CauHinhGpio::Tai() {
    CauHinhGpio cau_hinh;
    Settings cai_dat(NAMESPACE_GPIO, true);
    cau_hinh.man_hinh_mosi = static_cast<gpio_num_t>(cai_dat.GetInt("mosi", cau_hinh.man_hinh_mosi));
    cau_hinh.man_hinh_clk = static_cast<gpio_num_t>(cai_dat.GetInt("clk", cau_hinh.man_hinh_clk));
    cau_hinh.man_hinh_dc = static_cast<gpio_num_t>(cai_dat.GetInt("dc", cau_hinh.man_hinh_dc));
    cau_hinh.man_hinh_rst = static_cast<gpio_num_t>(cai_dat.GetInt("rst", cau_hinh.man_hinh_rst));
    cau_hinh.man_hinh_cs = static_cast<gpio_num_t>(cai_dat.GetInt("cs", cau_hinh.man_hinh_cs));
    cau_hinh.den_nen = static_cast<gpio_num_t>(cai_dat.GetInt("backlight", cau_hinh.den_nen));
    cau_hinh.den_led = static_cast<gpio_num_t>(cai_dat.GetInt("led", cau_hinh.den_led));
    cau_hinh.nut_khoi_dong = static_cast<gpio_num_t>(cai_dat.GetInt("boot", cau_hinh.nut_khoi_dong));
    cau_hinh.nut_cam_ung = static_cast<gpio_num_t>(cai_dat.GetInt("touch", cau_hinh.nut_cam_ung));
    cau_hinh.nut_tang_am_luong = static_cast<gpio_num_t>(cai_dat.GetInt("volume_up", cau_hinh.nut_tang_am_luong));
    cau_hinh.nut_giam_am_luong = static_cast<gpio_num_t>(cai_dat.GetInt("volume_down", cau_hinh.nut_giam_am_luong));
    cau_hinh.den_lamp = static_cast<gpio_num_t>(cai_dat.GetInt("lamp", cau_hinh.den_lamp));
#ifdef AUDIO_I2S_METHOD_SIMPLEX
    cau_hinh.mic_ws = static_cast<gpio_num_t>(cai_dat.GetInt("mic_ws", cau_hinh.mic_ws));
    cau_hinh.mic_sck = static_cast<gpio_num_t>(cai_dat.GetInt("mic_sck", cau_hinh.mic_sck));
    cau_hinh.mic_din = static_cast<gpio_num_t>(cai_dat.GetInt("mic_din", cau_hinh.mic_din));
    cau_hinh.loa_dout = static_cast<gpio_num_t>(cai_dat.GetInt("spk_dout", cau_hinh.loa_dout));
    cau_hinh.loa_bclk = static_cast<gpio_num_t>(cai_dat.GetInt("spk_bclk", cau_hinh.loa_bclk));
    cau_hinh.loa_lrck = static_cast<gpio_num_t>(cai_dat.GetInt("spk_lrck", cau_hinh.loa_lrck));
#else
    cau_hinh.i2s_ws = static_cast<gpio_num_t>(cai_dat.GetInt("i2s_ws", cau_hinh.i2s_ws));
    cau_hinh.i2s_bclk = static_cast<gpio_num_t>(cai_dat.GetInt("i2s_bclk", cau_hinh.i2s_bclk));
    cau_hinh.i2s_din = static_cast<gpio_num_t>(cai_dat.GetInt("i2s_din", cau_hinh.i2s_din));
    cau_hinh.i2s_dout = static_cast<gpio_num_t>(cai_dat.GetInt("i2s_dout", cau_hinh.i2s_dout));
#endif

    int so_lan_loi = cai_dat.GetInt(KHOA_SO_LAN_LOI, 0);
    if (LaLoiKhoiDong(esp_reset_reason())) {
        ++so_lan_loi;
        cai_dat.SetInt(KHOA_SO_LAN_LOI, so_lan_loi);
    }
    if (so_lan_loi > 3) {
        ESP_LOGE(TAG, "Khởi động lỗi nhiều lần; khôi phục cấu hình GPIO mặc định");
        cau_hinh = CauHinhGpio{};
        cau_hinh.Luu();
    } else if (!cau_hinh.KiemTraHopLe()) {
        ESP_LOGE(TAG, "Cấu hình GPIO không hợp lệ; sử dụng chân mặc định");
        cau_hinh = CauHinhGpio{};
        cau_hinh.Luu();
    }

    if (xTaskCreate(XoaSoLanLoiSauKhiKhoiDongOnDinh, "gpio_boot_ok", 2048, nullptr, 1, nullptr) != pdPASS) {
        ESP_LOGW(TAG, "Không thể tạo tác vụ theo dõi khởi động GPIO");
    }
    return cau_hinh;
}

bool CauHinhGpio::KiemTraHopLe() const {
    struct Chan {
        gpio_num_t so;
        bool dau_ra;
        bool tuy_chon;
        const char* ten;
    };
    std::array<Chan, 19> cac_chan = {{
        {man_hinh_mosi, true, false, "MOSI"},
        {man_hinh_clk, true, false, "CLK"},
        {man_hinh_dc, true, false, "DC"},
        {man_hinh_rst, true, false, "RST"},
        {man_hinh_cs, true, false, "CS"},
        {den_nen, true, true, "đèn nền"},
        {den_led, true, true, "LED"},
        {nut_khoi_dong, false, false, "nút khởi động"},
        {nut_cam_ung, false, true, "nút cảm ứng"},
        {nut_tang_am_luong, false, true, "nút tăng âm lượng"},
        {nut_giam_am_luong, false, true, "nút giảm âm lượng"},
        {den_lamp, true, true, "đèn"},
#ifdef AUDIO_I2S_METHOD_SIMPLEX
        {mic_ws, true, false, "mic WS"},
        {mic_sck, true, false, "mic SCK"},
        {mic_din, false, false, "mic DIN"},
        {loa_dout, true, false, "loa DOUT"},
        {loa_bclk, true, false, "loa BCLK"},
        {loa_lrck, true, false, "loa LRCK"},
        {GPIO_NUM_NC, false, true, ""},
#else
        {i2s_ws, true, false, "I2S WS"},
        {i2s_bclk, true, false, "I2S BCLK"},
        {i2s_din, false, false, "I2S DIN"},
        {i2s_dout, true, false, "I2S DOUT"},
        {GPIO_NUM_NC, false, true, ""},
        {GPIO_NUM_NC, false, true, ""},
        {GPIO_NUM_NC, false, true, ""},
#endif
    }};

    for (size_t i = 0; i < cac_chan.size(); ++i) {
        const auto& chan = cac_chan[i];
        if (chan.so == GPIO_NUM_NC) {
            if (chan.tuy_chon) continue;
            return false;
        }
        if (chan.so < GPIO_NUM_0 || chan.so >= SOC_GPIO_PIN_COUNT ||
            !GPIO_IS_VALID_GPIO(chan.so) ||
            (chan.dau_ra && !GPIO_IS_VALID_OUTPUT_GPIO(chan.so))) {
            ESP_LOGW(TAG, "GPIO %s không hợp lệ: %d", chan.ten, chan.so);
            return false;
        }
#if CONFIG_IDF_TARGET_ESP32S3
        if (chan.so >= 26 && chan.so <= 37) {
            ESP_LOGW(TAG, "GPIO %s thuộc vùng flash/PSRAM của ESP32-S3: %d", chan.ten, chan.so);
            return false;
        }
        if (chan.so == 0 || chan.so == 3 || chan.so == 45 || chan.so == 46) {
            ESP_LOGW(TAG, "GPIO %s là chân strapping của ESP32-S3: %d", chan.ten, chan.so);
        }
#endif
        for (size_t j = 0; j < i; ++j) {
            if (chan.so == cac_chan[j].so) {
                ESP_LOGW(TAG, "GPIO bị trùng giữa %s và %s", chan.ten, cac_chan[j].ten);
                return false;
            }
        }
    }
    return true;
}

void CauHinhGpio::Luu() const {
    Settings cai_dat(NAMESPACE_GPIO, true);
    cai_dat.SetInt("mosi", man_hinh_mosi);
    cai_dat.SetInt("clk", man_hinh_clk);
    cai_dat.SetInt("dc", man_hinh_dc);
    cai_dat.SetInt("rst", man_hinh_rst);
    cai_dat.SetInt("cs", man_hinh_cs);
    cai_dat.SetInt("backlight", den_nen);
    cai_dat.SetInt("led", den_led);
    cai_dat.SetInt("boot", nut_khoi_dong);
    cai_dat.SetInt("touch", nut_cam_ung);
    cai_dat.SetInt("volume_up", nut_tang_am_luong);
    cai_dat.SetInt("volume_down", nut_giam_am_luong);
    cai_dat.SetInt("lamp", den_lamp);
#ifdef AUDIO_I2S_METHOD_SIMPLEX
    cai_dat.SetInt("mic_ws", mic_ws);
    cai_dat.SetInt("mic_sck", mic_sck);
    cai_dat.SetInt("mic_din", mic_din);
    cai_dat.SetInt("spk_dout", loa_dout);
    cai_dat.SetInt("spk_bclk", loa_bclk);
    cai_dat.SetInt("spk_lrck", loa_lrck);
#else
    cai_dat.SetInt("i2s_ws", i2s_ws);
    cai_dat.SetInt("i2s_bclk", i2s_bclk);
    cai_dat.SetInt("i2s_din", i2s_din);
    cai_dat.SetInt("i2s_dout", i2s_dout);
#endif
    cai_dat.SetInt(KHOA_SO_LAN_LOI, 0);
}

void CauHinhGpio::KhoiPhucMacDinh() {
    CauHinhGpio{}.Luu();
}
