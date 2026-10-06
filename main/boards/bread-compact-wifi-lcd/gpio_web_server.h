#ifndef BREAD_COMPACT_WIFI_LCD_GPIO_WEB_SERVER_H
#define BREAD_COMPACT_WIFI_LCD_GPIO_WEB_SERVER_H

#include "gpio_config.h"

#ifdef CONFIG_ENABLE_GPIO_WEB_CONFIG
#include <esp_http_server.h>

class MayChuWebCauHinhGpio {
public:
    explicit MayChuWebCauHinhGpio(CauHinhGpio& cau_hinh);
    ~MayChuWebCauHinhGpio();

    void BatDau(uint16_t cong);
    void Dung();

private:
    CauHinhGpio& cau_hinh_;
    httpd_handle_t may_chu_ = nullptr;
    uint16_t cong_ = 0;

    static esp_err_t HienTrang(httpd_req_t* yeu_cau);
    static esp_err_t LayCauHinh(httpd_req_t* yeu_cau);
    static esp_err_t LuuCauHinh(httpd_req_t* yeu_cau);
    static esp_err_t KhoiPhucCauHinh(httpd_req_t* yeu_cau);
    static esp_err_t KhoiDongLai(httpd_req_t* yeu_cau);
};
#endif

#endif
