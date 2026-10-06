#ifndef MUSIC_CLIENT_H_
#define MUSIC_CLIENT_H_

#include <cstdint>
#include <expected>
#include <mutex>
#include <string>
#include <vector>

// Thông tin một bài hát trên máy chủ nhạc cục bộ
struct BaiHat {
    int id = 0;
    std::string tieu_de;
    std::string nghe_si;
    std::string album;
    int thoi_luong = 0;  // giây
};

// Máy khách HTTP truy vấn máy chủ nhạc trong mạng LAN
class MayChuNhac {
public:
    template <typename T>
    using KetQua = std::expected<T, std::string>;

    static MayChuNhac& LayThucThe();

    KetQua<std::vector<BaiHat>> TimBaiHat(const std::string& tu_khoa, const std::string& nghe_si,
                                          const std::string& album, int so_luong);
    KetQua<BaiHat> LayBaiHat(int id);
    KetQua<std::vector<BaiHat>> LayDanhSachPhat(const std::string& ten);
    KetQua<std::vector<std::string>> LayTenDanhSachPhat();
    KetQua<int> QuetLai();

    // Địa chỉ phát luồng; có thể chứa khóa API nên không được ghi vào log
    KetQua<std::string> TaoUrlPhat(int id, int bat_dau_giay);

    void DatMayChu(const std::string& host, int port, const std::string& api_key);
    std::string MoTaMayChu();

private:
    MayChuNhac() = default;

    struct DiaChi {
        std::string host;
        int port = 0;
        std::string api_key;
    };

    KetQua<DiaChi> LayDiaChi();
    KetQua<std::string> Goi(const std::string& phuong_thuc, const std::string& duong_dan);
    KetQua<std::string> GoiMotLan(const std::string& phuong_thuc, const std::string& duong_dan);
    bool TimBangMdns(std::string& host, int& port);
    static std::string MaHoaUrl(const std::string& van_ban);
    static bool PhanTichBaiHat(const void* cjson_doi_tuong, BaiHat& bai);

    std::mutex mutex_;
    std::string host_cache_;
    int port_cache_ = 0;
};

#endif  // MUSIC_CLIENT_H_
