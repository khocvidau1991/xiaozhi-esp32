#include "music_mcp_tools.h"

#include <cJSON.h>
#include <esp_log.h>
#include <sdkconfig.h>

#include <cctype>
#include <string>
#include <vector>

#include "mcp_server.h"
#include "music_client.h"
#include "music_player.h"

#define TAG "NhacMCP"

namespace {

constexpr int kSoBaiToiDa = 50;
const std::string kRong = "";

cJSON* TaoPhanHoiPhat(const std::vector<BaiHat>& ds) {
    cJSON* json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "status", "playing");
    cJSON_AddStringToObject(json, "title", ds.front().tieu_de.c_str());
    cJSON_AddStringToObject(json, "artist", ds.front().nghe_si.c_str());
    cJSON_AddNumberToObject(json, "queued", static_cast<double>(ds.size() - 1));
    return json;
}

ToolResult PhatDanhSachBai(std::vector<BaiHat> ds, const char* thong_bao_rong) {
    if (ds.empty()) {
        return std::unexpected(thong_bao_rong);
    }
    auto* phan_hoi = TaoPhanHoiPhat(ds);
    TrinhPhatNhac::LayThucThe().PhatDanhSach(std::move(ds));
    return phan_hoi;
}

ToolResult KetQuaThaoTac(const std::string& loi) {
    if (!loi.empty()) {
        return std::unexpected(loi);
    }
    return true;
}

bool TenMayChuHopLe(const std::string& host) {
    if (host.size() > 63) {
        return false;
    }
    for (unsigned char c : host) {
        if (!isalnum(c) && c != '.' && c != '-') {
            return false;
        }
    }
    return true;
}

}  // namespace

void DangKyCongCuNhac() {
    auto& mcp = McpServer::GetInstance();
    auto& khach = MayChuNhac::LayThucThe();
    auto& phat = TrinhPhatNhac::LayThucThe();

    mcp.AddTool("self.music.search",
                "Tìm bài hát trên máy chủ nhạc cục bộ theo tên bài, nghệ sĩ hoặc album. "
                "Tìm được cả có dấu lẫn không dấu. Trả về danh sách ngắn gồm id, tên bài và nghệ sĩ.",
                PropertyList({Property("query", kPropertyTypeString),
                              Property("limit", kPropertyTypeInteger, 5, 1, 20)}),
                [&khach](const PropertyList& p) -> ToolResult {
                    auto ds = khach.TimBaiHat(p["query"].value<std::string>(), kRong, kRong,
                                              p["limit"].value<int>());
                    if (!ds) {
                        return std::unexpected(ds.error());
                    }
                    cJSON* mang = cJSON_CreateArray();
                    for (const auto& b : *ds) {
                        cJSON* muc = cJSON_CreateObject();
                        cJSON_AddNumberToObject(muc, "id", b.id);
                        cJSON_AddStringToObject(muc, "title", b.tieu_de.c_str());
                        cJSON_AddStringToObject(muc, "artist", b.nghe_si.c_str());
                        cJSON_AddItemToArray(mang, muc);
                    }
                    return mang;
                });

    mcp.AddTool("self.music.play",
                "Tìm và phát nhạc từ máy chủ nhạc cục bộ. Dùng khi người dùng nói như \"mở bài Lạc "
                "Trôi của Sơn Tùng\". Truyền query (tên bài/nghệ sĩ) hoặc id của bài hát. Bài đầu "
                "tiên được phát, các kết quả còn lại được đưa vào hàng đợi.",
                PropertyList({Property("query", kPropertyTypeString, std::string("")),
                              Property("id", kPropertyTypeInteger, -1, -1, 2147483647)}),
                [&khach](const PropertyList& p) -> ToolResult {
                    int id = p["id"].value<int>();
                    auto query = p["query"].value<std::string>();
                    if (id < 0 && query.empty()) {
                        return std::unexpected("Cần cung cấp tên bài hát (query) hoặc id");
                    }
                    if (id >= 0) {
                        auto bai = khach.LayBaiHat(id);
                        if (!bai) {
                            return std::unexpected(bai.error());
                        }
                        return PhatDanhSachBai({*bai}, "");
                    }
                    auto ds = khach.TimBaiHat(query, kRong, kRong, 20);
                    if (!ds) {
                        return std::unexpected(ds.error());
                    }
                    return PhatDanhSachBai(std::move(*ds), "Không tìm thấy bài hát phù hợp");
                });

    mcp.AddTool("self.music.play_artist",
                "Phát các bài hát của một nghệ sĩ từ máy chủ nhạc cục bộ.",
                PropertyList({Property("artist", kPropertyTypeString)}),
                [&khach](const PropertyList& p) -> ToolResult {
                    auto ds = khach.TimBaiHat(kRong, p["artist"].value<std::string>(), kRong,
                                              kSoBaiToiDa);
                    if (!ds) {
                        return std::unexpected(ds.error());
                    }
                    return PhatDanhSachBai(std::move(*ds), "Không tìm thấy nghệ sĩ này");
                });

    mcp.AddTool("self.music.play_album", "Phát một album từ máy chủ nhạc cục bộ.",
                PropertyList({Property("album", kPropertyTypeString)}),
                [&khach](const PropertyList& p) -> ToolResult {
                    auto ds = khach.TimBaiHat(kRong, kRong, p["album"].value<std::string>(),
                                              kSoBaiToiDa);
                    if (!ds) {
                        return std::unexpected(ds.error());
                    }
                    return PhatDanhSachBai(std::move(*ds), "Không tìm thấy album này");
                });

    mcp.AddTool("self.music.play_playlist", "Phát một danh sách phát đã lưu trên máy chủ nhạc.",
                PropertyList({Property("name", kPropertyTypeString)}),
                [&khach](const PropertyList& p) -> ToolResult {
                    auto ds = khach.LayDanhSachPhat(p["name"].value<std::string>());
                    if (!ds) {
                        return std::unexpected(ds.error());
                    }
                    return PhatDanhSachBai(std::move(*ds), "Danh sách phát này đang trống");
                });

    mcp.AddTool("self.music.list_playlists", "Liệt kê tên các danh sách phát trên máy chủ nhạc.",
                PropertyList(), [&khach](const PropertyList&) -> ToolResult {
                    auto ten = khach.LayTenDanhSachPhat();
                    if (!ten) {
                        return std::unexpected(ten.error());
                    }
                    cJSON* mang = cJSON_CreateArray();
                    for (const auto& t : *ten) {
                        cJSON_AddItemToArray(mang, cJSON_CreateString(t.c_str()));
                    }
                    return mang;
                });

    mcp.AddTool("self.music.pause", "Tạm dừng bài nhạc đang phát.", PropertyList(),
                [&phat](const PropertyList&) -> ToolResult {
                    return KetQuaThaoTac(phat.TamDung());
                });
    mcp.AddTool("self.music.resume", "Tiếp tục phát bài nhạc đang tạm dừng.", PropertyList(),
                [&phat](const PropertyList&) -> ToolResult {
                    return KetQuaThaoTac(phat.TiepTuc());
                });
    mcp.AddTool("self.music.stop", "Dừng phát nhạc hẳn.", PropertyList(),
                [&phat](const PropertyList&) -> ToolResult {
                    phat.Dung();
                    return true;
                });
    mcp.AddTool("self.music.next", "Chuyển sang bài tiếp theo trong hàng đợi.", PropertyList(),
                [&phat](const PropertyList&) -> ToolResult {
                    return KetQuaThaoTac(phat.BaiTiepTheo());
                });
    mcp.AddTool("self.music.previous",
                "Quay lại bài trước (hoặc phát lại từ đầu nếu bài hiện tại đã phát hơn 3 giây).",
                PropertyList(), [&phat](const PropertyList&) -> ToolResult {
                    return KetQuaThaoTac(phat.BaiTruoc());
                });
    mcp.AddTool("self.music.seek", "Tua bài đang phát đến một vị trí (tính bằng giây).",
                PropertyList({Property("seconds", kPropertyTypeInteger, 0, 86400)}),
                [&phat](const PropertyList& p) -> ToolResult {
                    return KetQuaThaoTac(phat.TuaDen(p["seconds"].value<int>()));
                });

    mcp.AddTool("self.music.set_repeat",
                "Đặt chế độ lặp: none (không lặp), one (lặp một bài), all (lặp cả hàng đợi). "
                "Để chỉnh âm lượng hãy dùng công cụ self.audio_speaker.set_volume.",
                PropertyList({Property("mode", kPropertyTypeString)}),
                [&phat](const PropertyList& p) -> ToolResult {
                    auto mode = p["mode"].value<std::string>();
                    if (mode == "none") {
                        phat.DatCheDoLap(CheDoLap::kKhong);
                    } else if (mode == "one") {
                        phat.DatCheDoLap(CheDoLap::kMotBai);
                    } else if (mode == "all") {
                        phat.DatCheDoLap(CheDoLap::kTatCa);
                    } else {
                        return std::unexpected("Chế độ lặp chỉ nhận none, one hoặc all");
                    }
                    return true;
                });
    mcp.AddTool("self.music.set_shuffle", "Bật hoặc tắt phát ngẫu nhiên.",
                PropertyList({Property("enabled", kPropertyTypeBoolean)}),
                [&phat](const PropertyList& p) -> ToolResult {
                    phat.DatNgauNhien(p["enabled"].value<bool>());
                    return true;
                });

    mcp.AddTool("self.music.queue_add",
                "Thêm bài hát vào cuối hàng đợi phát nhạc. Truyền query (tên bài/nghệ sĩ) hoặc id. "
                "Nếu đang không phát nhạc, bài được thêm sẽ phát ngay.",
                PropertyList({Property("query", kPropertyTypeString, std::string("")),
                              Property("id", kPropertyTypeInteger, -1, -1, 2147483647)}),
                [&khach, &phat](const PropertyList& p) -> ToolResult {
                    int id = p["id"].value<int>();
                    auto query = p["query"].value<std::string>();
                    std::vector<BaiHat> them;
                    if (id >= 0) {
                        auto bai = khach.LayBaiHat(id);
                        if (!bai) {
                            return std::unexpected(bai.error());
                        }
                        them.push_back(*bai);
                    } else if (!query.empty()) {
                        auto ds = khach.TimBaiHat(query, kRong, kRong, 1);
                        if (!ds) {
                            return std::unexpected(ds.error());
                        }
                        them = std::move(*ds);
                    } else {
                        return std::unexpected("Cần cung cấp tên bài hát (query) hoặc id");
                    }
                    if (them.empty()) {
                        return std::unexpected("Không tìm thấy bài hát phù hợp");
                    }
                    if (!phat.DangHoatDong()) {
                        return PhatDanhSachBai(std::move(them), "");
                    }
                    phat.ThemVaoHangDoi(std::move(them));
                    return true;
                });
    mcp.AddTool("self.music.queue_clear", "Xóa hàng đợi (giữ lại bài đang phát).", PropertyList(),
                [&phat](const PropertyList&) -> ToolResult {
                    phat.XoaHangDoi();
                    return true;
                });
    mcp.AddTool("self.music.queue_list", "Xem danh sách bài trong hàng đợi phát nhạc.",
                PropertyList(), [&phat](const PropertyList&) -> ToolResult {
                    return phat.HangDoiJson();
                });
    mcp.AddTool("self.music.now_playing",
                "Cho biết bài đang phát, vị trí, thời lượng, trạng thái, chế độ lặp và ngẫu nhiên.",
                PropertyList(), [&phat](const PropertyList&) -> ToolResult {
                    return phat.TrangThaiJson();
                });

    mcp.AddUserOnlyTool("self.music.rescan", "Yêu cầu máy chủ nhạc quét lại thư mục nhạc.",
                        PropertyList(), [&khach](const PropertyList&) -> ToolResult {
                            auto so_bai = khach.QuetLai();
                            if (!so_bai) {
                                return std::unexpected(so_bai.error());
                            }
                            return *so_bai;
                        });
    mcp.AddUserOnlyTool(
        "self.music.set_server",
        "Cấu hình địa chỉ máy chủ nhạc. Để trống host để tự tìm bằng mDNS (xiaozhi-music.local).",
        PropertyList({Property("host", kPropertyTypeString, std::string("")),
                      Property("port", kPropertyTypeInteger, CONFIG_MUSIC_SERVER_DEFAULT_PORT, 1,
                               65535),
                      Property("api_key", kPropertyTypeString, std::string(""))}),
        [&khach](const PropertyList& p) -> ToolResult {
            auto host = p["host"].value<std::string>();
            if (!TenMayChuHopLe(host)) {
                return std::unexpected("Địa chỉ máy chủ không hợp lệ");
            }
            khach.DatMayChu(host, p["port"].value<int>(), p["api_key"].value<std::string>());
            ESP_LOGI(TAG, "Đã cập nhật cấu hình máy chủ nhạc");  // không ghi khóa API vào log
            return true;
        });
}
