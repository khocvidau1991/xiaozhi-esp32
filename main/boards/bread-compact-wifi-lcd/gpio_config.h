#ifndef BREAD_COMPACT_WIFI_LCD_GPIO_CONFIG_H
#define BREAD_COMPACT_WIFI_LCD_GPIO_CONFIG_H

#include "config.h"

#include <driver/gpio.h>

struct CauHinhGpio {
    gpio_num_t man_hinh_mosi = DISPLAY_MOSI_PIN;
    gpio_num_t man_hinh_clk = DISPLAY_CLK_PIN;
    gpio_num_t man_hinh_dc = DISPLAY_DC_PIN;
    gpio_num_t man_hinh_rst = DISPLAY_RST_PIN;
    gpio_num_t man_hinh_cs = DISPLAY_CS_PIN;
    gpio_num_t den_nen = DISPLAY_BACKLIGHT_PIN;
    gpio_num_t den_led = BUILTIN_LED_GPIO;
    gpio_num_t nut_khoi_dong = BOOT_BUTTON_GPIO;
    gpio_num_t nut_cam_ung = TOUCH_BUTTON_GPIO;
    gpio_num_t nut_tang_am_luong = VOLUME_UP_BUTTON_GPIO;
    gpio_num_t nut_giam_am_luong = VOLUME_DOWN_BUTTON_GPIO;
    gpio_num_t den_lamp = LAMP_GPIO;
#ifdef AUDIO_I2S_METHOD_SIMPLEX
    gpio_num_t mic_ws = AUDIO_I2S_MIC_GPIO_WS;
    gpio_num_t mic_sck = AUDIO_I2S_MIC_GPIO_SCK;
    gpio_num_t mic_din = AUDIO_I2S_MIC_GPIO_DIN;
    gpio_num_t loa_dout = AUDIO_I2S_SPK_GPIO_DOUT;
    gpio_num_t loa_bclk = AUDIO_I2S_SPK_GPIO_BCLK;
    gpio_num_t loa_lrck = AUDIO_I2S_SPK_GPIO_LRCK;
#else
    gpio_num_t i2s_ws = AUDIO_I2S_GPIO_WS;
    gpio_num_t i2s_bclk = AUDIO_I2S_GPIO_BCLK;
    gpio_num_t i2s_din = AUDIO_I2S_GPIO_DIN;
    gpio_num_t i2s_dout = AUDIO_I2S_GPIO_DOUT;
#endif

    static CauHinhGpio Tai();
    bool KiemTraHopLe() const;
    void Luu() const;
    static void KhoiPhucMacDinh();
};

#endif
