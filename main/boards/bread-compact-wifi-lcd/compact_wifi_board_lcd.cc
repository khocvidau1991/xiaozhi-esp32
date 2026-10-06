#include "wifi_board.h"
#include "codecs/no_audio_codec.h"
#include "display/lcd_display.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "mcp_server.h"
#include "lamp_controller.h"
#include "led/single_led.h"
#include "gpio_config.h"
#ifdef CONFIG_ENABLE_GPIO_WEB_CONFIG
#include "gpio_web_server.h"
#include <wifi_manager.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

#include <esp_log.h>
#include <esp_system.h>
#include <driver/i2c_master.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <driver/spi_common.h>

#if defined(LCD_TYPE_ILI9341_SERIAL)
#include "esp_lcd_ili9341.h"
#endif

#if defined(LCD_TYPE_GC9A01_SERIAL)
#include "esp_lcd_gc9a01.h"
static const gc9a01_lcd_init_cmd_t gc9107_lcd_init_cmds[] = {
    //  {cmd, { data }, data_size, delay_ms}
    {0xfe, (uint8_t[]){0x00}, 0, 0},
    {0xef, (uint8_t[]){0x00}, 0, 0},
    {0xb0, (uint8_t[]){0xc0}, 1, 0},
    {0xb1, (uint8_t[]){0x80}, 1, 0},
    {0xb2, (uint8_t[]){0x27}, 1, 0},
    {0xb3, (uint8_t[]){0x13}, 1, 0},
    {0xb6, (uint8_t[]){0x19}, 1, 0},
    {0xb7, (uint8_t[]){0x05}, 1, 0},
    {0xac, (uint8_t[]){0xc8}, 1, 0},
    {0xab, (uint8_t[]){0x0f}, 1, 0},
    {0x3a, (uint8_t[]){0x05}, 1, 0},
    {0xb4, (uint8_t[]){0x04}, 1, 0},
    {0xa8, (uint8_t[]){0x08}, 1, 0},
    {0xb8, (uint8_t[]){0x08}, 1, 0},
    {0xea, (uint8_t[]){0x02}, 1, 0},
    {0xe8, (uint8_t[]){0x2A}, 1, 0},
    {0xe9, (uint8_t[]){0x47}, 1, 0},
    {0xe7, (uint8_t[]){0x5f}, 1, 0},
    {0xc6, (uint8_t[]){0x21}, 1, 0},
    {0xc7, (uint8_t[]){0x15}, 1, 0},
    {0xf0,
    (uint8_t[]){0x1D, 0x38, 0x09, 0x4D, 0x92, 0x2F, 0x35, 0x52, 0x1E, 0x0C,
                0x04, 0x12, 0x14, 0x1f},
    14, 0},
    {0xf1,
    (uint8_t[]){0x16, 0x40, 0x1C, 0x54, 0xA9, 0x2D, 0x2E, 0x56, 0x10, 0x0D,
                0x0C, 0x1A, 0x14, 0x1E},
    14, 0},
    {0xf4, (uint8_t[]){0x00, 0x00, 0xFF}, 3, 0},
    {0xba, (uint8_t[]){0xFF, 0xFF}, 2, 0},
};
#endif
 
#define TAG "CompactWifiBoardLCD"

class CompactWifiBoardLCD : public WifiBoard {
private:
    CauHinhGpio cau_hinh_gpio_;
    Button boot_button_;
    LcdDisplay* display_;
#ifdef CONFIG_ENABLE_GPIO_WEB_CONFIG
    MayChuWebCauHinhGpio may_chu_gpio_;

    static void TheoDoiMang(void* arg) {
        auto* board = static_cast<CompactWifiBoardLCD*>(arg);
        while (true) {
            auto& wifi = WifiManager::GetInstance();
            if (wifi.IsConfigMode()) {
                board->may_chu_gpio_.BatDau(8080);
            } else if (wifi.IsConnected()) {
                board->may_chu_gpio_.BatDau(80);
            } else {
                board->may_chu_gpio_.Dung();
            }
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
#endif

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = cau_hinh_gpio_.man_hinh_mosi;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = cau_hinh_gpio_.man_hinh_clk;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeLcdDisplay() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;
        // Khởi tạo các chân điều khiển màn hình LCD
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = cau_hinh_gpio_.man_hinh_cs;
        io_config.dc_gpio_num = cau_hinh_gpio_.man_hinh_dc;
        io_config.spi_mode = DISPLAY_SPI_MODE;
        io_config.pclk_hz = 40 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));

        // Khởi tạo bộ điều khiển màn hình LCD
        ESP_LOGD(TAG, "Install LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = cau_hinh_gpio_.man_hinh_rst;
        panel_config.rgb_ele_order = DISPLAY_RGB_ORDER;
        panel_config.bits_per_pixel = 16;
#if defined(LCD_TYPE_GC9A01_SERIAL)
        gc9a01_vendor_config_t gc9107_vendor_config = {
            .init_cmds = gc9107_lcd_init_cmds,
            .init_cmds_size = sizeof(gc9107_lcd_init_cmds) / sizeof(gc9a01_lcd_init_cmd_t),
        };
        panel_config.vendor_config = &gc9107_vendor_config;
#endif
#if defined(LCD_TYPE_ILI9341_SERIAL)
        ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(panel_io, &panel_config, &panel));
#elif defined(LCD_TYPE_GC9A01_SERIAL)
        ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(panel_io, &panel_config, &panel));
#else
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));
#endif
        
        esp_lcd_panel_reset(panel);

        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, DISPLAY_INVERT_COLOR);
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        display_ = new SpiLcdDisplay(panel_io, panel,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            if (display_->IsMenuVisible()) {
                display_->MoveMenuSelection();
                return;
            }
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });
        boot_button_.OnDoubleClick([this]() {
            if (Application::GetInstance().GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            if (!display_->IsMenuVisible()) {
                display_->ShowMenu();
                return;
            }

            switch (display_->SelectMenuItem()) {
                case 0:
                    Application::GetInstance().ToggleChatState();
                    break;
                case 1:
                    EnterWifiConfigMode();
                    break;
                case 2: {
                    auto codec = GetAudioCodec();
                    int volume = codec->output_volume() + 10;
                    if (volume > 100) {
                        volume = 20;
                    }
                    codec->SetOutputVolume(volume);
                    display_->ShowNotification("Âm lượng: " + std::to_string(volume) + "%");
                    break;
                }
                case 3:
#ifdef CONFIG_ENABLE_GPIO_WEB_CONFIG
                    display_->ShowNotification("Mở /gpio tại địa chỉ IP thiết bị; AP dùng cổng 8080.");
#else
                    display_->ShowNotification("Cấu hình GPIO web chưa được bật.");
#endif
                    break;
                case 4:
                    display_->ShowNotification("ESP32-S3 • LCD 240 × 240");
                    break;
                case 5:
                    esp_restart();
                    break;
                default:
                    break;
            }
        });
        boot_button_.OnLongPress([this]() {
            if (Application::GetInstance().GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
            } else if (display_->IsMenuVisible()) {
                display_->HideMenu();
            } else {
                display_->ShowMenu();
            }
        });
    }

    // Khởi tạo thiết bị điều khiển đèn để AI có thể sử dụng
    void InitializeTools() {
        if (cau_hinh_gpio_.den_lamp != GPIO_NUM_NC) {
            static LampController lamp(cau_hinh_gpio_.den_lamp);
        }
    }

public:
    CompactWifiBoardLCD() :
        cau_hinh_gpio_(CauHinhGpio::Tai()),
        boot_button_(cau_hinh_gpio_.nut_khoi_dong)
#ifdef CONFIG_ENABLE_GPIO_WEB_CONFIG
        , may_chu_gpio_(cau_hinh_gpio_)
#endif
    {
        InitializeSpi();
        InitializeLcdDisplay();
        InitializeButtons();
        InitializeTools();
        if (cau_hinh_gpio_.den_nen != GPIO_NUM_NC) {
            GetBacklight()->RestoreBrightness();
        }
#ifdef CONFIG_ENABLE_GPIO_WEB_CONFIG
        xTaskCreate(TheoDoiMang, "gpio_web", 3072, this, 2, nullptr);
#endif
    }

    virtual Led* GetLed() override {
        if (cau_hinh_gpio_.den_led == GPIO_NUM_NC) {
            return nullptr;
        }
        static SingleLed led(cau_hinh_gpio_.den_led);
        return &led;
    }

    virtual AudioCodec* GetAudioCodec() override {
#ifdef AUDIO_I2S_METHOD_SIMPLEX
        static NoAudioCodecSimplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            cau_hinh_gpio_.loa_bclk, cau_hinh_gpio_.loa_lrck, cau_hinh_gpio_.loa_dout, cau_hinh_gpio_.mic_sck, cau_hinh_gpio_.mic_ws, cau_hinh_gpio_.mic_din);
#else
        static NoAudioCodecDuplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            cau_hinh_gpio_.i2s_bclk, cau_hinh_gpio_.i2s_ws, cau_hinh_gpio_.i2s_dout, cau_hinh_gpio_.i2s_din);
#endif
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual Backlight* GetBacklight() override {
        if (cau_hinh_gpio_.den_nen != GPIO_NUM_NC) {
            static PwmBacklight backlight(cau_hinh_gpio_.den_nen, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
            return &backlight;
        }
        return nullptr;
    }
};

DECLARE_BOARD(CompactWifiBoardLCD);
