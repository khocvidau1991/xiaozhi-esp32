#include "music_player.h"

#include <cJSON.h>
#include <esp_log.h>
#include <esp_random.h>

#include <algorithm>

#include "board.h"
#include "display.h"

#define TAG "TrinhPhatNhac"

namespace {
constexpr int kSoLanThuToiDa = 40;          // 40 x 500 ms = 20 giây chờ thiết bị rảnh
constexpr int kLanThuNgungNghe = 6;         // sau 3 giây vẫn đang nghe thì dừng nghe để phát nhạc
constexpr int kLoiLienTiepToiDa = 3;
constexpr uint32_t kNguongBaiTruocMs = 3000;
}  // namespace

TrinhPhatNhac& TrinhPhatNhac::LayThucThe() {
    static TrinhPhatNhac thuc_the;
    return thuc_the;
}

const char* TrinhPhatNhac::TenCheDoLap(CheDoLap che_do) {
    switch (che_do) {
        case CheDoLap::kMotBai: return "one";
        case CheDoLap::kTatCa: return "all";
        default: return "none";
    }
}

const BaiHat* TrinhPhatNhac::BaiHienTaiLocked() const {
    if (vi_tri_ >= thu_tu_.size()) {
        return nullptr;
    }
    return &hang_doi_[thu_tu_[vi_tri_]];
}

void TrinhPhatNhac::TaoThuTu(bool giu_bai_hien_tai) {
    size_t hien_tai = 0;
    if (giu_bai_hien_tai && vi_tri_ < thu_tu_.size()) {
        hien_tai = thu_tu_[vi_tri_];
    }
    thu_tu_.resize(hang_doi_.size());
    for (size_t i = 0; i < thu_tu_.size(); ++i) {
        thu_tu_[i] = i;
    }
    if (ngau_nhien_ && thu_tu_.size() > 1) {
        for (size_t i = thu_tu_.size() - 1; i > 0; --i) {
            std::swap(thu_tu_[i], thu_tu_[esp_random() % (i + 1)]);
        }
        if (giu_bai_hien_tai) {
            auto it = std::find(thu_tu_.begin(), thu_tu_.end(), hien_tai);
            std::iter_swap(thu_tu_.begin(), it);
        }
    } else if (giu_bai_hien_tai) {
        vi_tri_ = hien_tai;
        return;
    }
    vi_tri_ = 0;
}

void TrinhPhatNhac::PhatDanhSach(std::vector<BaiHat> danh_sach, size_t vi_tri) {
    if (danh_sach.empty()) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        hang_doi_ = std::move(danh_sach);
        // Giữ bài được chọn ở đầu thứ tự khi bật ngẫu nhiên
        thu_tu_.resize(hang_doi_.size());
        for (size_t i = 0; i < thu_tu_.size(); ++i) thu_tu_[i] = i;
        vi_tri_ = std::min(vi_tri, hang_doi_.size() - 1);
        TaoThuTu(true);
        loi_lien_tiep_ = 0;
        goc_ms_ = 0;
        vi_tri_ms_ = 0;
    }
    BatDauPhat(0);
}

void TrinhPhatNhac::ThemVaoHangDoi(std::vector<BaiHat> danh_sach) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& bai : danh_sach) {
        hang_doi_.push_back(std::move(bai));
        thu_tu_.push_back(hang_doi_.size() - 1);
    }
}

bool TrinhPhatNhac::DangHoatDong() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return trang_thai_ != TrangThaiNhac::kDung;
}

void TrinhPhatNhac::XoaHangDoi() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (trang_thai_ == TrangThaiNhac::kDung) {
        hang_doi_.clear();
        thu_tu_.clear();
        vi_tri_ = 0;
        return;
    }
    // Giữ lại bài đang phát, xóa các bài còn lại
    if (auto* bai = BaiHienTaiLocked()) {
        BaiHat hien_tai = *bai;
        hang_doi_.assign(1, hien_tai);
        thu_tu_.assign(1, 0);
        vi_tri_ = 0;
    }
}

void TrinhPhatNhac::BatDauPhat(int bat_dau_giay) {
    uint32_t the;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (BaiHienTaiLocked() == nullptr) {
            return;
        }
        the = ++the_phat_;
        trang_thai_ = TrangThaiNhac::kDangTai;
        goc_ms_ = static_cast<uint32_t>(bat_dau_giay) * 1000;
        vi_tri_ms_ = goc_ms_;
    }
    Application::GetInstance().Schedule([this, the]() { ThuBatDau(the, 0); });
}

void TrinhPhatNhac::HenGioThuLai(uint32_t the, int lan_thu) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        the_hen_gio_ = the;
        lan_thu_hen_gio_ = lan_thu;
        if (hen_gio_ == nullptr) {
            esp_timer_create_args_t tham_so = {};
            tham_so.callback = [](void* arg) {
                auto* tp = static_cast<TrinhPhatNhac*>(arg);
                uint32_t the_moi;
                int lan;
                {
                    std::lock_guard<std::mutex> lock(tp->mutex_);
                    the_moi = tp->the_hen_gio_;
                    lan = tp->lan_thu_hen_gio_;
                }
                Application::GetInstance().Schedule(
                    [tp, the_moi, lan]() { tp->ThuBatDau(the_moi, lan); });
            };
            tham_so.arg = this;
            tham_so.name = "nhac_thu_lai";
            if (esp_timer_create(&tham_so, &hen_gio_) != ESP_OK) {
                hen_gio_ = nullptr;
                return;
            }
        }
        esp_timer_stop(hen_gio_);
        esp_timer_start_once(hen_gio_, 500 * 1000);
    }
}

void TrinhPhatNhac::ThuBatDau(uint32_t the, int lan_thu) {
    BaiHat bai;
    int bat_dau_giay = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (the != the_phat_ || BaiHienTaiLocked() == nullptr) {
            return;
        }
        bai = *BaiHienTaiLocked();
        bat_dau_giay = static_cast<int>(goc_ms_ / 1000);
    }

    auto& app = Application::GetInstance();
    if (app.DangPhatNhac()) {
        app.DungPhatNhac();
    }
    if (!app.CoTheBatDauNhac()) {
        if (lan_thu >= kSoLanThuToiDa) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (the == the_phat_) {
                trang_thai_ = TrangThaiNhac::kDung;
            }
            ESP_LOGW(TAG, "Thiết bị bận, bỏ qua yêu cầu phát nhạc");
            return;
        }
        if (lan_thu == kLanThuNgungNghe) {
            app.StopListening();  // người dùng đã yêu cầu phát nhạc, thoát chế độ nghe liên tục
        }
        HenGioThuLai(the, lan_thu + 1);
        return;
    }

    auto url = MayChuNhac::LayThucThe().TaoUrlPhat(bai.id, bat_dau_giay);
    if (!url) {
        BaoLoiHienThi(url.error());
        std::lock_guard<std::mutex> lock(mutex_);
        if (the == the_phat_) trang_thai_ = TrangThaiNhac::kDung;
        return;
    }

    bool ok = app.BatDauPhatNhac(
        *url, [this, the](Application::KetThucNhac kt) { XuLyKetThuc(the, kt); },
        [this, the](uint32_t ms) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (the == the_phat_) {
                vi_tri_ms_ = goc_ms_ + ms;
            }
        });
    std::lock_guard<std::mutex> lock(mutex_);
    if (the != the_phat_) {
        return;
    }
    if (ok) {
        trang_thai_ = TrangThaiNhac::kDangPhat;
        loi_lien_tiep_ = 0;
        auto bai_sao = bai;
        Application::GetInstance().Schedule([this, bai_sao]() { HienThi(bai_sao); });
    } else {
        trang_thai_ = TrangThaiNhac::kDung;
        ESP_LOGE(TAG, "Không bắt đầu được luồng nhạc");
    }
}

void TrinhPhatNhac::HienThi(const BaiHat& bai) {
    auto display = Board::GetInstance().GetDisplay();
    if (display == nullptr) {
        return;
    }
    std::string noi_dung = "♪ " + bai.tieu_de;
    if (!bai.nghe_si.empty()) {
        noi_dung += " - " + bai.nghe_si;
    }
    display->SetChatMessage("assistant", noi_dung.c_str());
}

void TrinhPhatNhac::BaoLoiHienThi(const std::string& noi_dung) {
    ESP_LOGW(TAG, "%s", noi_dung.c_str());
    Application::GetInstance().Schedule([noi_dung]() {
        auto display = Board::GetInstance().GetDisplay();
        if (display) {
            display->SetChatMessage("system", noi_dung.c_str());
        }
    });
}

bool TrinhPhatNhac::ChuyenBai(int huong, bool tu_dong) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (thu_tu_.empty()) {
        return false;
    }
    if (tu_dong && lap_ == CheDoLap::kMotBai) {
        return true;
    }
    long moi = static_cast<long>(vi_tri_) + huong;
    if (moi >= static_cast<long>(thu_tu_.size())) {
        if (lap_ != CheDoLap::kTatCa) {
            return false;
        }
        TaoThuTu(false);  // xáo lại khi bắt đầu vòng mới
        moi = 0;
    } else if (moi < 0) {
        moi = lap_ == CheDoLap::kTatCa ? static_cast<long>(thu_tu_.size()) - 1 : 0;
    }
    vi_tri_ = static_cast<size_t>(moi);
    return true;
}

void TrinhPhatNhac::XuLyKetThuc(uint32_t the, Application::KetThucNhac ket_thuc) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (the != the_phat_) {
            return;
        }
        if (ket_thuc == Application::KetThucNhac::kBiNgat) {
            // Bị ngắt bởi từ khóa đánh thức hoặc nút bấm: giữ vị trí để có thể tiếp tục
            trang_thai_ = TrangThaiNhac::kTamDung;
            return;
        }
        if (ket_thuc == Application::KetThucNhac::kLoi &&
            ++loi_lien_tiep_ >= kLoiLienTiepToiDa) {
            trang_thai_ = TrangThaiNhac::kDung;
            return;
        }
    }
    if (ChuyenBai(+1, ket_thuc == Application::KetThucNhac::kHoanThanh)) {
        BatDauPhat(0);
    } else {
        std::lock_guard<std::mutex> lock(mutex_);
        trang_thai_ = TrangThaiNhac::kDung;
        vi_tri_ms_ = 0;
        goc_ms_ = 0;
        vi_tri_ = 0;
    }
}

std::string TrinhPhatNhac::TamDung() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (trang_thai_ != TrangThaiNhac::kDangPhat && trang_thai_ != TrangThaiNhac::kDangTai) {
            return "Hiện không có bài nào đang phát";
        }
        ++the_phat_;
        trang_thai_ = TrangThaiNhac::kTamDung;
    }
    Application::GetInstance().Schedule([]() { Application::GetInstance().DungPhatNhac(); });
    return "";
}

std::string TrinhPhatNhac::TiepTuc() {
    int giay;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (trang_thai_ != TrangThaiNhac::kTamDung) {
            return "Hiện không có bài nào đang tạm dừng";
        }
        giay = static_cast<int>(vi_tri_ms_ / 1000);
    }
    BatDauPhat(giay);
    return "";
}

void TrinhPhatNhac::Dung() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ++the_phat_;
        trang_thai_ = TrangThaiNhac::kDung;
        vi_tri_ms_ = 0;
        goc_ms_ = 0;
    }
    Application::GetInstance().Schedule([]() { Application::GetInstance().DungPhatNhac(); });
}

std::string TrinhPhatNhac::BaiTiepTheo() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (thu_tu_.empty()) {
            return "Hàng đợi nhạc đang trống";
        }
    }
    if (!ChuyenBai(+1, false)) {
        return "Đây đã là bài cuối cùng trong hàng đợi";
    }
    BatDauPhat(0);
    return "";
}

std::string TrinhPhatNhac::BaiTruoc() {
    bool phat_lai_bai_nay;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (thu_tu_.empty()) {
            return "Hàng đợi nhạc đang trống";
        }
        phat_lai_bai_nay = vi_tri_ms_ > kNguongBaiTruocMs;
    }
    if (!phat_lai_bai_nay) {
        ChuyenBai(-1, false);
    }
    BatDauPhat(0);
    return "";
}

std::string TrinhPhatNhac::TuaDen(int giay) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto* bai = BaiHienTaiLocked();
        if (bai == nullptr || trang_thai_ == TrangThaiNhac::kDung) {
            return "Hiện không có bài nào để tua";
        }
        if (giay < 0 || (bai->thoi_luong > 0 && giay >= bai->thoi_luong)) {
            return "Vị trí tua nằm ngoài thời lượng bài hát";
        }
    }
    BatDauPhat(giay);
    return "";
}

void TrinhPhatNhac::DatCheDoLap(CheDoLap che_do) {
    std::lock_guard<std::mutex> lock(mutex_);
    lap_ = che_do;
}

void TrinhPhatNhac::DatNgauNhien(bool bat) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (ngau_nhien_ == bat) {
        return;
    }
    ngau_nhien_ = bat;
    TaoThuTu(true);
}

cJSON* TrinhPhatNhac::TrangThaiJson() {
    std::lock_guard<std::mutex> lock(mutex_);
    cJSON* json = cJSON_CreateObject();
    const char* ten = "stopped";
    switch (trang_thai_) {
        case TrangThaiNhac::kDangTai: ten = "loading"; break;
        case TrangThaiNhac::kDangPhat: ten = "playing"; break;
        case TrangThaiNhac::kTamDung: ten = "paused"; break;
        default: break;
    }
    cJSON_AddStringToObject(json, "state", ten);
    if (auto* bai = BaiHienTaiLocked(); bai && trang_thai_ != TrangThaiNhac::kDung) {
        cJSON* hien_tai = cJSON_AddObjectToObject(json, "track");
        cJSON_AddNumberToObject(hien_tai, "id", bai->id);
        cJSON_AddStringToObject(hien_tai, "title", bai->tieu_de.c_str());
        cJSON_AddStringToObject(hien_tai, "artist", bai->nghe_si.c_str());
        cJSON_AddStringToObject(hien_tai, "album", bai->album.c_str());
        cJSON_AddNumberToObject(json, "position_seconds", vi_tri_ms_ / 1000);
        cJSON_AddNumberToObject(json, "duration_seconds", bai->thoi_luong);
    }
    cJSON_AddStringToObject(json, "repeat", TenCheDoLap(lap_));
    cJSON_AddBoolToObject(json, "shuffle", ngau_nhien_);
    cJSON_AddNumberToObject(json, "queue_size", static_cast<double>(hang_doi_.size()));
    return json;
}

cJSON* TrinhPhatNhac::HangDoiJson() {
    std::lock_guard<std::mutex> lock(mutex_);
    cJSON* mang = cJSON_CreateArray();
    for (size_t i = 0; i < thu_tu_.size(); ++i) {
        const auto& bai = hang_doi_[thu_tu_[i]];
        cJSON* muc = cJSON_CreateObject();
        cJSON_AddNumberToObject(muc, "id", bai.id);
        cJSON_AddStringToObject(muc, "title", bai.tieu_de.c_str());
        cJSON_AddStringToObject(muc, "artist", bai.nghe_si.c_str());
        cJSON_AddBoolToObject(muc, "current", i == vi_tri_ && trang_thai_ != TrangThaiNhac::kDung);
        cJSON_AddItemToArray(mang, muc);
    }
    return mang;
}
