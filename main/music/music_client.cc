#include "music_client.h"

#include <cJSON.h>
#include <esp_log.h>
#include <esp_netif_ip_addr.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <mdns.h>
#include <sdkconfig.h>

#include <cctype>
#include <cstdio>
#include <functional>
#include <memory>

#include "board.h"
#include "cjson_utils.h"
#include "http.h"
#include "settings.h"

#define TAG "MayChuNhac"

namespace {
constexpr int kHttpTimeoutMs = 5000;
constexpr int kSoLanThu = 2;
constexpr size_t kToiDaPhanHoi = CONFIG_MUSIC_PLAYER_BUFFER_KB * 1024;
}  // namespace

MayChuNhac& MayChuNhac::LayThucThe() {
    static MayChuNhac thuc_the;
    return thuc_the;
}

std::string MayChuNhac::MaHoaUrl(const std::string& van_ban) {
    static const char* kHex = "0123456789ABCDEF";
    std::string kq;
    for (unsigned char c : van_ban) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            kq += static_cast<char>(c);
        } else {
            kq += '%';
            kq += kHex[c >> 4];
            kq += kHex[c & 0x0F];
        }
    }
    return kq;
}

void MayChuNhac::DatMayChu(const std::string& host, int port, const std::string& api_key) {
    Settings settings("music", true);
    settings.SetString("host", host);
    settings.SetInt("port", port);
    settings.SetString("api_key", api_key);
    std::lock_guard<std::mutex> lock(mutex_);
    host_cache_.clear();
    port_cache_ = 0;
}

std::string MayChuNhac::MoTaMayChu() {
    auto dia_chi = LayDiaChi();
    if (!dia_chi) {
        return dia_chi.error();
    }
    return dia_chi->host + ":" + std::to_string(dia_chi->port);
}

bool MayChuNhac::TimBangMdns(std::string& host, int& port) {
    static std::mutex mutex_mdns;
    static bool da_khoi_tao = false;
    std::lock_guard<std::mutex> khoa_mdns(mutex_mdns);
    if (!da_khoi_tao) {
        esp_err_t loi = mdns_init();
        if (loi != ESP_OK && loi != ESP_ERR_INVALID_STATE) {
            ESP_LOGW(TAG, "Không khởi tạo được mDNS: %s", esp_err_to_name(loi));
            return false;
        }
        da_khoi_tao = true;
    }
    mdns_result_t* ket_qua = nullptr;
    esp_err_t loi = mdns_query_ptr("_xiaozhi-music", "_tcp", 3000, 4, &ket_qua);
    if (loi != ESP_OK || ket_qua == nullptr) {
        return false;
    }
    bool tim_thay = false;
    for (auto* r = ket_qua; r != nullptr && !tim_thay; r = r->next) {
        for (auto* a = r->addr; a != nullptr; a = a->next) {
            if (a->addr.type == ESP_IPADDR_TYPE_V4) {
                char bo_dem[16];
                esp_ip4addr_ntoa(&a->addr.u_addr.ip4, bo_dem, sizeof(bo_dem));
                host = bo_dem;
                port = r->port;
                tim_thay = true;
                break;
            }
        }
    }
    mdns_query_results_free(ket_qua);
    return tim_thay;
}

MayChuNhac::KetQua<MayChuNhac::DiaChi> MayChuNhac::LayDiaChi() {
    DiaChi dia_chi;
    {
        Settings settings("music", false);
        dia_chi.host = settings.GetString("host", CONFIG_MUSIC_SERVER_DEFAULT_HOST);
        dia_chi.port = settings.GetInt("port", CONFIG_MUSIC_SERVER_DEFAULT_PORT);
        dia_chi.api_key = settings.GetString("api_key", "");
    }
    if (dia_chi.port <= 0 || dia_chi.port > 65535) {
        dia_chi.port = CONFIG_MUSIC_SERVER_DEFAULT_PORT;
    }
    if (!dia_chi.host.empty()) {
        return dia_chi;
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!host_cache_.empty()) {
            dia_chi.host = host_cache_;
            dia_chi.port = port_cache_;
            return dia_chi;
        }
    }
    std::string host;
    int port = 0;
    if (!TimBangMdns(host, port)) {
        return std::unexpected(
            "Không tìm thấy máy chủ nhạc trong mạng. Hãy bật máy chủ nhạc hoặc cấu hình địa chỉ "
            "bằng công cụ self.music.set_server");
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        host_cache_ = host;
        port_cache_ = port;
    }
    dia_chi.host = host;
    dia_chi.port = port;
    return dia_chi;
}

MayChuNhac::KetQua<std::string> MayChuNhac::GoiMotLan(const std::string& phuong_thuc,
                                                      const std::string& duong_dan) {
    auto dia_chi = LayDiaChi();
    if (!dia_chi) {
        return std::unexpected(dia_chi.error());
    }
    auto mang = Board::GetInstance().GetNetwork();
    auto http = mang->CreateHttp(0);
    if (!http) {
        return std::unexpected("Không tạo được kết nối HTTP");
    }
    http->SetTimeout(kHttpTimeoutMs);
    http->SetHeader("Accept", "application/json");
    if (!dia_chi->api_key.empty()) {
        http->SetHeader("X-Api-Key", dia_chi->api_key);
    }
    if (phuong_thuc != "GET") {
        http->SetHeader("Content-Type", "application/json");
        http->SetContent("{}");
    }
    std::string url = "http://" + dia_chi->host + ":" + std::to_string(dia_chi->port) + duong_dan;
    auto mo = http->Open(phuong_thuc, url);
    if (!mo) {
        return std::unexpected("Không kết nối được máy chủ nhạc: " + mo.error().ToString());
    }
    auto trang_thai = http->GetStatusCode();
    if (!trang_thai) {
        http->Close();
        return std::unexpected("Máy chủ nhạc không phản hồi: " + trang_thai.error().ToString());
    }
    std::string noi_dung = http->ReadAll();
    http->Close();
    if (*trang_thai == 401) {
        return std::unexpected("Khóa API của máy chủ nhạc không đúng");
    }
    if (*trang_thai < 200 || *trang_thai >= 300) {
        std::string loi = "Máy chủ nhạc trả về lỗi " + std::to_string(*trang_thai);
        CJsonUniquePtr json(cJSON_Parse(noi_dung.c_str()));
        if (json) {
            auto* thong_bao = cJSON_GetObjectItem(json.get(), "loi");
            if (cJSON_IsString(thong_bao)) {
                loi += ": ";
                loi += thong_bao->valuestring;
            }
        }
        return std::unexpected(loi);
    }
    if (noi_dung.size() > kToiDaPhanHoi) {
        return std::unexpected("Phản hồi của máy chủ nhạc quá lớn");
    }
    return noi_dung;
}

MayChuNhac::KetQua<std::string> MayChuNhac::Goi(const std::string& phuong_thuc,
                                                const std::string& duong_dan) {
    std::string loi_cuoi;
    for (int lan = 0; lan < kSoLanThu; ++lan) {
        auto kq = GoiMotLan(phuong_thuc, duong_dan);
        if (kq) {
            return kq;
        }
        loi_cuoi = kq.error();
        // Lỗi nghiệp vụ (4xx) thì không thử lại
        if (loi_cuoi.find("Máy chủ nhạc trả về lỗi 4") != std::string::npos ||
            loi_cuoi.find("Khóa API") != std::string::npos) {
            break;
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            host_cache_.clear();  // tìm lại bằng mDNS ở lần sau
        }
        vTaskDelay(pdMS_TO_TICKS(300));
    }
    ESP_LOGW(TAG, "Gọi %s thất bại: %s", phuong_thuc.c_str(), loi_cuoi.c_str());
    return std::unexpected(loi_cuoi);
}

bool MayChuNhac::PhanTichBaiHat(const void* doi_tuong, BaiHat& bai) {
    auto* json = static_cast<const cJSON*>(doi_tuong);
    auto* id = cJSON_GetObjectItem(json, "id");
    if (!cJSON_IsNumber(id)) {
        return false;
    }
    bai.id = id->valueint;
    auto lay_chuoi = [json](const char* khoa) -> std::string {
        auto* m = cJSON_GetObjectItem(json, khoa);
        return cJSON_IsString(m) && m->valuestring ? m->valuestring : "";
    };
    bai.tieu_de = lay_chuoi("tieu_de");
    bai.nghe_si = lay_chuoi("nghe_si");
    bai.album = lay_chuoi("album");
    auto* thoi_luong = cJSON_GetObjectItem(json, "thoi_luong");
    bai.thoi_luong = cJSON_IsNumber(thoi_luong) ? thoi_luong->valueint : 0;
    return true;
}

static std::vector<BaiHat> DocMangBaiHat(const cJSON* mang, std::function<bool(const void*, BaiHat&)> doc) {
    std::vector<BaiHat> ds;
    if (!cJSON_IsArray(mang)) {
        return ds;
    }
    cJSON* phan_tu = nullptr;
    cJSON_ArrayForEach(phan_tu, mang) {
        BaiHat bai;
        if (doc(phan_tu, bai)) {
            ds.push_back(std::move(bai));
        }
    }
    return ds;
}

MayChuNhac::KetQua<std::vector<BaiHat>> MayChuNhac::TimBaiHat(const std::string& tu_khoa,
                                                              const std::string& nghe_si,
                                                              const std::string& album,
                                                              int so_luong) {
    std::string duong_dan = "/api/bai-hat?so_luong=" + std::to_string(so_luong);
    if (!tu_khoa.empty()) duong_dan += "&tu_khoa=" + MaHoaUrl(tu_khoa);
    if (!nghe_si.empty()) duong_dan += "&nghe_si=" + MaHoaUrl(nghe_si);
    if (!album.empty()) duong_dan += "&album=" + MaHoaUrl(album);
    auto kq = Goi("GET", duong_dan);
    if (!kq) {
        return std::unexpected(kq.error());
    }
    CJsonUniquePtr json(cJSON_Parse(kq->c_str()));
    if (!json) {
        return std::unexpected("Phản hồi của máy chủ nhạc không hợp lệ");
    }
    return DocMangBaiHat(cJSON_GetObjectItem(json.get(), "bai_hat"), PhanTichBaiHat);
}

MayChuNhac::KetQua<BaiHat> MayChuNhac::LayBaiHat(int id) {
    auto kq = Goi("GET", "/api/bai-hat/" + std::to_string(id));
    if (!kq) {
        return std::unexpected(kq.error());
    }
    CJsonUniquePtr json(cJSON_Parse(kq->c_str()));
    BaiHat bai;
    if (!json || !PhanTichBaiHat(json.get(), bai)) {
        return std::unexpected("Phản hồi của máy chủ nhạc không hợp lệ");
    }
    return bai;
}

MayChuNhac::KetQua<std::vector<BaiHat>> MayChuNhac::LayDanhSachPhat(const std::string& ten) {
    auto kq = Goi("GET", "/api/danh-sach-phat/" + MaHoaUrl(ten));
    if (!kq) {
        return std::unexpected(kq.error());
    }
    CJsonUniquePtr json(cJSON_Parse(kq->c_str()));
    if (!json) {
        return std::unexpected("Phản hồi của máy chủ nhạc không hợp lệ");
    }
    return DocMangBaiHat(cJSON_GetObjectItem(json.get(), "bai_hat"), PhanTichBaiHat);
}

MayChuNhac::KetQua<std::vector<std::string>> MayChuNhac::LayTenDanhSachPhat() {
    auto kq = Goi("GET", "/api/danh-sach-phat");
    if (!kq) {
        return std::unexpected(kq.error());
    }
    CJsonUniquePtr json(cJSON_Parse(kq->c_str()));
    if (!json) {
        return std::unexpected("Phản hồi của máy chủ nhạc không hợp lệ");
    }
    std::vector<std::string> ten_ds;
    auto* mang = cJSON_GetObjectItem(json.get(), "danh_sach_phat");
    cJSON* phan_tu = nullptr;
    cJSON_ArrayForEach(phan_tu, mang) {
        auto* ten = cJSON_GetObjectItem(phan_tu, "ten");
        if (cJSON_IsString(ten) && ten->valuestring) {
            ten_ds.emplace_back(ten->valuestring);
        }
    }
    return ten_ds;
}

MayChuNhac::KetQua<int> MayChuNhac::QuetLai() {
    auto kq = Goi("POST", "/api/quet");
    if (!kq) {
        return std::unexpected(kq.error());
    }
    CJsonUniquePtr json(cJSON_Parse(kq->c_str()));
    auto* so_bai = json ? cJSON_GetObjectItem(json.get(), "so_bai_hat") : nullptr;
    return cJSON_IsNumber(so_bai) ? so_bai->valueint : 0;
}

MayChuNhac::KetQua<std::string> MayChuNhac::TaoUrlPhat(int id, int bat_dau_giay) {
    auto dia_chi = LayDiaChi();
    if (!dia_chi) {
        return std::unexpected(dia_chi.error());
    }
    std::string url = "http://" + dia_chi->host + ":" + std::to_string(dia_chi->port) +
                      "/stream/" + std::to_string(id);
#ifdef CONFIG_MUSIC_PREFER_TRANSCODE
    url += "?dinh_dang=opus&toc_do_bit=32k&toc_do_mau=16000";
#else
    url += "?dinh_dang=opus&toc_do_bit=48k&toc_do_mau=24000";
#endif
    if (bat_dau_giay > 0) {
        url += "&bat_dau=" + std::to_string(bat_dau_giay);
    }
    if (!dia_chi->api_key.empty()) {
        url += "&khoa_api=" + MaHoaUrl(dia_chi->api_key);
    }
    return url;
}
