#include "gpio_web_server.h"

#ifdef CONFIG_ENABLE_GPIO_WEB_CONFIG
#include <cJSON.h>
#include <esp_log.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <array>
#include <cmath>
#include <string>

#define TAG "MayChuCauHinhGpio"

namespace {
constexpr char TRANG_HTML[] = R"HTML(<!doctype html>
<html lang="vi">
<head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="color-scheme" content="light"><title>Cấu hình phần cứng</title>
<style>
:root{font-family:system-ui,-apple-system,"Segoe UI",sans-serif;color:#18212f;background:#f3f6fb}
*{box-sizing:border-box}body{margin:0;padding:24px 14px}.card{max-width:680px;margin:auto;padding:24px;background:white;border:1px solid #e2e8f0;border-radius:18px;box-shadow:0 12px 35px #1e293b12}
h1{font-size:1.55rem;margin:0 0 8px}p{line-height:1.5;color:#526074}.warn{padding:12px 14px;border-radius:12px;background:#fff7e6;color:#805500}
form{display:grid;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:14px 16px;margin:22px 0}
label{font-size:.9rem;font-weight:650}input{display:block;width:100%;margin-top:6px;padding:10px 12px;border:1px solid #cbd5e1;border-radius:10px;font:inherit}
.actions{display:flex;flex-wrap:wrap;gap:10px}button{border:0;border-radius:10px;padding:11px 16px;font:inherit;font-weight:650;cursor:pointer;background:#4f8bff;color:white}
button.secondary{background:#e8edf5;color:#263448}button.danger{background:#b42318}#status{min-height:1.5em;margin-top:14px;color:#263448}
</style></head>
<body><main class="card"><h1>Cấu hình phần cứng</h1>
<p>Điều chỉnh chân GPIO cho bo mạch Bread Compact Wi-Fi LCD. Giá trị -1 chỉ dùng để tắt đèn nền, LED hoặc đèn.</p>
<p class="warn"><strong>Cảnh báo:</strong> Chọn sai chân có thể khiến thiết bị không khởi động. Nút khôi phục mặc định luôn có sẵn.</p>
<form id="gpio"></form><div class="actions"><button id="save">Lưu</button><button class="secondary" id="reload" type="button">Tải lại</button><button class="danger" id="reset" type="button">Khôi phục mặc định</button></div>
<div id="status" role="status" aria-live="polite"></div></main>
<script>
const tenTruong={mosi:"Màn hình MOSI",clk:"Màn hình CLK",dc:"Màn hình DC",rst:"Màn hình RST",cs:"Màn hình CS",backlight:"Đèn nền (-1: tắt)",led:"LED tích hợp (-1: tắt)",boot:"Nút khởi động",touch:"Nút cảm ứng (-1: không dùng)",volume_up:"Nút tăng âm lượng (-1: không dùng)",volume_down:"Nút giảm âm lượng (-1: không dùng)",lamp:"Đèn điều khiển (-1: không dùng)",mic_ws:"Mic WS",mic_sck:"Mic SCK",mic_din:"Mic DIN",spk_dout:"Loa DOUT",spk_bclk:"Loa BCLK",spk_lrck:"Loa LRCK",i2s_ws:"I2S WS",i2s_bclk:"I2S BCLK",i2s_din:"I2S DIN",i2s_dout:"I2S DOUT"};
const bieuMau=document.querySelector("#gpio"), trangThai=document.querySelector("#status");
function hienThongBao(chuoi){trangThai.textContent=chuoi}
async function nap(){const phanHoi=await fetch("/api/gpio");if(!phanHoi.ok)throw Error("Không tải được cấu hình GPIO");const duLieu=await phanHoi.json();bieuMau.innerHTML="";for(const [ma,giaTri] of Object.entries(duLieu)){const nhan=document.createElement("label");nhan.textContent=tenTruong[ma]||ma;const o=document.createElement("input");o.type="number";o.name=ma;o.min="-1";o.max="48";o.required=true;o.value=giaTri;nhan.append(o);bieuMau.append(nhan)}}
document.querySelector("#save").addEventListener("click",async suKien=>{suKien.preventDefault();if(!bieuMau.reportValidity())return;const duLieu=Object.fromEntries(new FormData(bieuMau));for(const ma in duLieu)duLieu[ma]=Number(duLieu[ma]);try{let phanHoi=await fetch("/api/gpio",{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify(duLieu)});let ketQua=await phanHoi.json();if(!phanHoi.ok)throw Error(ketQua.error||"Không thể lưu cấu hình");hienThongBao(ketQua.message);setTimeout(()=>fetch("/api/reboot",{method:"POST"}),700)}catch(loi){hienThongBao(loi.message)}});
document.querySelector("#reload").addEventListener("click",()=>nap().then(()=>hienThongBao("Đã tải cấu hình GPIO.")).catch(loi=>hienThongBao(loi.message)));
document.querySelector("#reset").addEventListener("click",async()=>{if(!confirm("Khôi phục toàn bộ chân GPIO mặc định và khởi động lại?"))return;try{const phanHoi=await fetch("/api/gpio/reset",{method:"POST"});const ketQua=await phanHoi.json();if(!phanHoi.ok)throw Error(ketQua.error||"Không thể khôi phục");hienThongBao(ketQua.message);setTimeout(()=>fetch("/api/reboot",{method:"POST"}),700)}catch(loi){hienThongBao(loi.message)}});
nap().catch(loi=>hienThongBao(loi.message));
</script></body></html>)HTML";

struct TruongJson {
    const char* ten;
    gpio_num_t CauHinhGpio::*con_truong;
};

#ifdef AUDIO_I2S_METHOD_SIMPLEX
const std::array<TruongJson, 18> TRUONG_I2S_DON = {{
    {"mosi", &CauHinhGpio::man_hinh_mosi},
    {"clk", &CauHinhGpio::man_hinh_clk},
    {"dc", &CauHinhGpio::man_hinh_dc},
    {"rst", &CauHinhGpio::man_hinh_rst},
    {"cs", &CauHinhGpio::man_hinh_cs},
    {"backlight", &CauHinhGpio::den_nen},
    {"led", &CauHinhGpio::den_led},
    {"boot", &CauHinhGpio::nut_khoi_dong},
    {"touch", &CauHinhGpio::nut_cam_ung},
    {"volume_up", &CauHinhGpio::nut_tang_am_luong},
    {"volume_down", &CauHinhGpio::nut_giam_am_luong},
    {"lamp", &CauHinhGpio::den_lamp},
    {"mic_ws", &CauHinhGpio::mic_ws},
    {"mic_sck", &CauHinhGpio::mic_sck},
    {"mic_din", &CauHinhGpio::mic_din},
    {"spk_dout", &CauHinhGpio::loa_dout},
    {"spk_bclk", &CauHinhGpio::loa_bclk},
    {"spk_lrck", &CauHinhGpio::loa_lrck},
}};
#endif

const std::array<TruongJson, 12> TRUONG_CHUNG = {{
    {"mosi", &CauHinhGpio::man_hinh_mosi},
    {"clk", &CauHinhGpio::man_hinh_clk},
    {"dc", &CauHinhGpio::man_hinh_dc},
    {"rst", &CauHinhGpio::man_hinh_rst},
    {"cs", &CauHinhGpio::man_hinh_cs},
    {"backlight", &CauHinhGpio::den_nen},
    {"led", &CauHinhGpio::den_led},
    {"boot", &CauHinhGpio::nut_khoi_dong},
    {"touch", &CauHinhGpio::nut_cam_ung},
    {"volume_up", &CauHinhGpio::nut_tang_am_luong},
    {"volume_down", &CauHinhGpio::nut_giam_am_luong},
    {"lamp", &CauHinhGpio::den_lamp},
}};

bool DocSoChan(cJSON* doi_tuong, const char* ten, gpio_num_t& so_chan) {
    cJSON* gia_tri = cJSON_GetObjectItemCaseSensitive(doi_tuong, ten);
    if (!cJSON_IsNumber(gia_tri) || !std::isfinite(gia_tri->valuedouble) ||
        gia_tri->valuedouble < -1 || gia_tri->valuedouble > 48 ||
        std::floor(gia_tri->valuedouble) != gia_tri->valuedouble) {
        return false;
    }
    so_chan = static_cast<gpio_num_t>(gia_tri->valueint);
    return true;
}

bool NhanJson(httpd_req_t* yeu_cau, std::string& noi_dung) {
    if (yeu_cau->content_len == 0 || yeu_cau->content_len > 1024) {
        return false;
    }
    noi_dung.resize(yeu_cau->content_len);
    size_t da_nhan = 0;
    while (da_nhan < noi_dung.size()) {
        int ket_qua = httpd_req_recv(yeu_cau, noi_dung.data() + da_nhan, noi_dung.size() - da_nhan);
        if (ket_qua <= 0) {
            return false;
        }
        da_nhan += ket_qua;
    }
    return true;
}

esp_err_t GuiJson(httpd_req_t* yeu_cau, cJSON* doi_tuong) {
    char* chuoi_json = cJSON_PrintUnformatted(doi_tuong);
    if (chuoi_json == nullptr) {
        cJSON_Delete(doi_tuong);
        return ESP_FAIL;
    }
    httpd_resp_set_type(yeu_cau, "application/json; charset=utf-8");
    esp_err_t ket_qua = httpd_resp_sendstr(yeu_cau, chuoi_json);
    cJSON_free(chuoi_json);
    cJSON_Delete(doi_tuong);
    return ket_qua;
}

esp_err_t GuiThongBao(httpd_req_t* yeu_cau, const char* truong, const char* thong_bao,
                      int ma_loi = 400) {
    cJSON* doi_tuong = cJSON_CreateObject();
    if (doi_tuong == nullptr) return ESP_FAIL;
    cJSON_AddStringToObject(doi_tuong, truong, thong_bao);
    if (ma_loi != 0) httpd_resp_set_status(yeu_cau, "400 Bad Request");
    return GuiJson(yeu_cau, doi_tuong);
}
}  // namespace

MayChuWebCauHinhGpio::MayChuWebCauHinhGpio(CauHinhGpio& cau_hinh) : cau_hinh_(cau_hinh) {}

MayChuWebCauHinhGpio::~MayChuWebCauHinhGpio() { Dung(); }

void MayChuWebCauHinhGpio::BatDau(uint16_t cong) {
    if (may_chu_ != nullptr && cong_ == cong) return;
    Dung();

    httpd_config_t cau_hinh = HTTPD_DEFAULT_CONFIG();
    cau_hinh.server_port = cong;
    cau_hinh.max_uri_handlers = 8;
    cau_hinh.stack_size = 6144;
    cau_hinh.lru_purge_enable = true;
    if (httpd_start(&may_chu_, &cau_hinh) != ESP_OK) {
        may_chu_ = nullptr;
        ESP_LOGW(TAG, "Không thể khởi động máy chủ cấu hình GPIO trên cổng %u", cong);
        return;
    }
    cong_ = cong;

    httpd_uri_t cac_uri[] = {
        {.uri = "/gpio", .method = HTTP_GET, .handler = HienTrang, .user_ctx = this},
        {.uri = "/api/gpio", .method = HTTP_GET, .handler = LayCauHinh, .user_ctx = this},
        {.uri = "/api/gpio", .method = HTTP_POST, .handler = LuuCauHinh, .user_ctx = this},
        {.uri = "/api/gpio/reset",
         .method = HTTP_POST,
         .handler = KhoiPhucCauHinh,
         .user_ctx = this},
        {.uri = "/api/reboot", .method = HTTP_POST, .handler = KhoiDongLai, .user_ctx = this},
    };
    for (auto& uri : cac_uri) {
        if (httpd_register_uri_handler(may_chu_, &uri) != ESP_OK) {
            ESP_LOGE(TAG, "Không thể đăng ký URI %s", uri.uri);
            Dung();
            return;
        }
    }
    ESP_LOGI(TAG, "Máy chủ cấu hình GPIO đang chạy trên cổng %u", cong);
}

void MayChuWebCauHinhGpio::Dung() {
    if (may_chu_ != nullptr) {
        httpd_stop(may_chu_);
        may_chu_ = nullptr;
        cong_ = 0;
    }
}

esp_err_t MayChuWebCauHinhGpio::HienTrang(httpd_req_t* yeu_cau) {
    httpd_resp_set_type(yeu_cau, "text/html; charset=utf-8");
    httpd_resp_set_hdr(yeu_cau, "Cache-Control", "no-store");
    return httpd_resp_sendstr(yeu_cau, TRANG_HTML);
}

esp_err_t MayChuWebCauHinhGpio::LayCauHinh(httpd_req_t* yeu_cau) {
    auto* may_chu = static_cast<MayChuWebCauHinhGpio*>(yeu_cau->user_ctx);
    cJSON* doi_tuong = cJSON_CreateObject();
    if (doi_tuong == nullptr) return ESP_FAIL;
#ifdef AUDIO_I2S_METHOD_SIMPLEX
    for (const auto& truong : TRUONG_I2S_DON) {
        cJSON_AddNumberToObject(doi_tuong, truong.ten, (may_chu->cau_hinh_.*(truong.con_truong)));
    }
#else
    constexpr std::array<const char*, 4> ten_i2s = {"i2s_ws", "i2s_bclk", "i2s_din", "i2s_dout"};
    const std::array<gpio_num_t, 4> chan_i2s = {
        may_chu->cau_hinh_.i2s_ws, may_chu->cau_hinh_.i2s_bclk, may_chu->cau_hinh_.i2s_din,
        may_chu->cau_hinh_.i2s_dout};
    for (size_t i = 0; i < ten_i2s.size(); ++i)
        cJSON_AddNumberToObject(doi_tuong, ten_i2s[i], chan_i2s[i]);
#endif
#ifndef AUDIO_I2S_METHOD_SIMPLEX
    for (const auto& truong : TRUONG_CHUNG) {
        cJSON_AddNumberToObject(doi_tuong, truong.ten, (may_chu->cau_hinh_.*(truong.con_truong)));
    }
#endif
    return GuiJson(yeu_cau, doi_tuong);
}

esp_err_t MayChuWebCauHinhGpio::LuuCauHinh(httpd_req_t* yeu_cau) {
    auto* may_chu = static_cast<MayChuWebCauHinhGpio*>(yeu_cau->user_ctx);
    std::string noi_dung;
    if (!NhanJson(yeu_cau, noi_dung))
        return GuiThongBao(yeu_cau, "error", "Dữ liệu gửi lên không hợp lệ.");
    cJSON* doi_tuong = cJSON_Parse(noi_dung.c_str());
    if (!cJSON_IsObject(doi_tuong)) {
        cJSON_Delete(doi_tuong);
        return GuiThongBao(yeu_cau, "error", "Không thể đọc dữ liệu cấu hình.");
    }

    CauHinhGpio cau_hinh = may_chu->cau_hinh_;
    bool hop_le = true;
#define DOC_CHAN(ten_json, ten_truong) \
    hop_le = hop_le && DocSoChan(doi_tuong, ten_json, cau_hinh.ten_truong)
    DOC_CHAN("mosi", man_hinh_mosi);
    DOC_CHAN("clk", man_hinh_clk);
    DOC_CHAN("dc", man_hinh_dc);
    DOC_CHAN("rst", man_hinh_rst);
    DOC_CHAN("cs", man_hinh_cs);
    DOC_CHAN("backlight", den_nen);
    DOC_CHAN("led", den_led);
    DOC_CHAN("boot", nut_khoi_dong);
    DOC_CHAN("touch", nut_cam_ung);
    DOC_CHAN("volume_up", nut_tang_am_luong);
    DOC_CHAN("volume_down", nut_giam_am_luong);
    DOC_CHAN("lamp", den_lamp);
#ifdef AUDIO_I2S_METHOD_SIMPLEX
    DOC_CHAN("mic_ws", mic_ws);
    DOC_CHAN("mic_sck", mic_sck);
    DOC_CHAN("mic_din", mic_din);
    DOC_CHAN("spk_dout", loa_dout);
    DOC_CHAN("spk_bclk", loa_bclk);
    DOC_CHAN("spk_lrck", loa_lrck);
#else
    DOC_CHAN("i2s_ws", i2s_ws);
    DOC_CHAN("i2s_bclk", i2s_bclk);
    DOC_CHAN("i2s_din", i2s_din);
    DOC_CHAN("i2s_dout", i2s_dout);
#endif
#undef DOC_CHAN
    cJSON_Delete(doi_tuong);
    if (!hop_le || !cau_hinh.KiemTraHopLe()) {
        return GuiThongBao(yeu_cau, "error",
                           "Chân GPIO không hợp lệ, bị trùng hoặc dành riêng cho flash/PSRAM.");
    }
    may_chu->cau_hinh_ = cau_hinh;
    may_chu->cau_hinh_.Luu();
    return GuiThongBao(yeu_cau, "message", "Đã lưu cấu hình. Thiết bị sẽ khởi động lại để áp dụng.",
                       0);
}

esp_err_t MayChuWebCauHinhGpio::KhoiPhucCauHinh(httpd_req_t* yeu_cau) {
    auto* may_chu = static_cast<MayChuWebCauHinhGpio*>(yeu_cau->user_ctx);
    may_chu->cau_hinh_ = CauHinhGpio{};
    may_chu->cau_hinh_.Luu();
    return GuiThongBao(yeu_cau, "message",
                       "Đã khôi phục chân GPIO mặc định. Thiết bị sẽ khởi động lại.", 0);
}

esp_err_t MayChuWebCauHinhGpio::KhoiDongLai(httpd_req_t* yeu_cau) {
    esp_err_t ket_qua = GuiThongBao(yeu_cau, "message", "Thiết bị đang khởi động lại.", 0);
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
    return ket_qua;
}
#endif
