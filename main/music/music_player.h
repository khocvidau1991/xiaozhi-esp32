#ifndef MUSIC_PLAYER_H_
#define MUSIC_PLAYER_H_

#include <esp_timer.h>

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "application.h"
#include "music_client.h"

struct cJSON;

enum class TrangThaiNhac { kDung, kDangTai, kDangPhat, kTamDung };
enum class CheDoLap { kKhong, kMotBai, kTatCa };

// Trình phát nhạc: quản lý hàng đợi, lặp, ngẫu nhiên và phát luồng qua Application.
// Mọi thao tác với Application đều được thực hiện trên luồng chính qua Schedule().
class TrinhPhatNhac {
public:
    static TrinhPhatNhac& LayThucThe();

    // Thay thế hàng đợi và phát bài tại vi_tri
    void PhatDanhSach(std::vector<BaiHat> danh_sach, size_t vi_tri = 0);
    void ThemVaoHangDoi(std::vector<BaiHat> danh_sach);
    bool DangHoatDong() const;
    void XoaHangDoi();
    std::string TamDung();
    std::string TiepTuc();
    void Dung();
    std::string BaiTiepTheo();
    std::string BaiTruoc();
    std::string TuaDen(int giay);
    void DatCheDoLap(CheDoLap che_do);
    void DatNgauNhien(bool bat);

    cJSON* TrangThaiJson();
    cJSON* HangDoiJson();

    static const char* TenCheDoLap(CheDoLap che_do);

private:
    TrinhPhatNhac() = default;

    void BatDauPhat(int bat_dau_giay);
    void ThuBatDau(uint32_t the, int lan_thu);
    void HenGioThuLai(uint32_t the, int lan_thu);
    void XuLyKetThuc(uint32_t the, Application::KetThucNhac ket_thuc);
    void TaoThuTu(bool giu_bai_hien_tai);
    bool ChuyenBai(int huong, bool tu_dong);  // true nếu còn bài để phát
    void HienThi(const BaiHat& bai);
    void BaoLoiHienThi(const std::string& noi_dung);
    const BaiHat* BaiHienTaiLocked() const;

    mutable std::mutex mutex_;
    std::vector<BaiHat> hang_doi_;
    std::vector<size_t> thu_tu_;  // thứ tự phát (chỉ số vào hang_doi_)
    size_t vi_tri_ = 0;           // vị trí hiện tại trong thu_tu_
    TrangThaiNhac trang_thai_ = TrangThaiNhac::kDung;
    CheDoLap lap_ = CheDoLap::kKhong;
    bool ngau_nhien_ = false;
    uint32_t the_phat_ = 0;  // tăng mỗi lần bắt đầu/dừng để loại bỏ callback cũ
    uint32_t vi_tri_ms_ = 0;
    uint32_t goc_ms_ = 0;  // vị trí bắt đầu của luồng hiện tại (khi tua)
    int loi_lien_tiep_ = 0;
    esp_timer_handle_t hen_gio_ = nullptr;
    uint32_t the_hen_gio_ = 0;
    int lan_thu_hen_gio_ = 0;
};

#endif  // MUSIC_PLAYER_H_
