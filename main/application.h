#ifndef _APPLICATION_H_
#define _APPLICATION_H_

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <esp_timer.h>

#include <string>
#include <mutex>
#include <deque>
#include <memory>
#include <functional>
#include <cstdint>
#include <vector>

#include "protocol.h"
#include "ota.h"
#include "audio_service.h"
#include "device_state.h"
#include "device_state_machine.h"
#include "notify/notify_player.h"

// Các bit sự kiện chính
#define MAIN_EVENT_SCHEDULE             (1 << 0)
#define MAIN_EVENT_SEND_AUDIO           (1 << 1)
#define MAIN_EVENT_WAKE_WORD_DETECTED   (1 << 2)
#define MAIN_EVENT_VAD_CHANGE           (1 << 3)
#define MAIN_EVENT_ERROR                (1 << 4)
#define MAIN_EVENT_ACTIVATION_DONE      (1 << 5)
#define MAIN_EVENT_CLOCK_TICK           (1 << 6)
#define MAIN_EVENT_NETWORK_CONNECTED    (1 << 7)
#define MAIN_EVENT_NETWORK_DISCONNECTED (1 << 8)
#define MAIN_EVENT_TOGGLE_CHAT          (1 << 9)
#define MAIN_EVENT_START_LISTENING      (1 << 10)
#define MAIN_EVENT_STOP_LISTENING       (1 << 11)
#define MAIN_EVENT_STATE_CHANGED        (1 << 12)
#define MAIN_EVENT_PLAYBACK_DRAINED     (1 << 13)


enum AecMode {
    kAecOff,
    kAecOnDeviceSide,
    kAecOnServerSide,
};

class Application {
public:
    static Application& GetInstance() {
        static Application instance;
        return instance;
    }
    // Xóa hàm khởi tạo sao chép và toán tử gán
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    /**
     * Khởi tạo ứng dụng
     * Thiết lập màn hình, âm thanh, các callback mạng, v.v.
     * Kết nối mạng được bắt đầu một cách bất đồng bộ.
     */
    void Initialize();

    /**
     * Chạy vòng lặp sự kiện chính
     * Hàm này chạy trong tác vụ chính và không bao giờ trả về.
     * Xử lý mọi sự kiện, gồm mạng, thay đổi trạng thái và tương tác của người dùng.
     */
    void Run();

    DeviceState GetDeviceState() const { return state_machine_.GetState(); }
    bool IsVoiceDetected() const { return audio_service_.IsVoiceDetected(); }
    
    /**
     * Yêu cầu chuyển trạng thái
     * Trả về true nếu chuyển trạng thái thành công
     */
    bool SetDeviceState(DeviceState state);

    /**
     * Lên lịch một callback để thực thi trong tác vụ chính
     */
    void Schedule(std::function<void()>&& callback);

    /**
     * Cảnh báo kèm trạng thái, thông điệp, biểu cảm và âm thanh tùy chọn
     */
    void Alert(const char* status, const char* message, const char* emotion = "", const std::string_view& sound = "");
    void DismissAlert();

    void AbortSpeaking(AbortReason reason);

    /**
     * Chuyển đổi trạng thái trò chuyện (dựa trên sự kiện, an toàn luồng)
     * Gửi MAIN_EVENT_TOGGLE_CHAT để được xử lý trong Run()
     */
    void ToggleChatState();

    /**
     * Bắt đầu lắng nghe (dựa trên sự kiện, an toàn luồng)
     * Gửi MAIN_EVENT_START_LISTENING để được xử lý trong Run()
     */
    void StartListening();

    /**
     * Dừng lắng nghe (dựa trên sự kiện, an toàn luồng)
     * Gửi MAIN_EVENT_STOP_LISTENING để được xử lý trong Run()
     */
    void StopListening();

    void Reboot();
    void WakeWordInvoke(const std::string& wake_word);
    bool UpgradeFirmware(const std::string& url, const std::string& version = "");
    bool CanEnterSleepMode();
    void SendMcpMessage(const std::string& payload);
    void RegisterMcpBroadcastCallback(std::function<void(const std::string&)> callback);
    void SetAecMode(AecMode mode);
    AecMode GetAecMode() const { return aec_mode_; }
    void PlaySound(const std::string_view& sound);
    AudioService& GetAudioService() { return audio_service_; }
    
    /**
     * Đặt lại tài nguyên giao thức (an toàn luồng)
     * Có thể gọi từ bất kỳ tác vụ nào để giải phóng tài nguyên được cấp phát sau khi mạng kết nối
     * Bao gồm đóng kênh âm thanh, đặt lại giao thức và các đối tượng ota
     */
    void ResetProtocol();

private:
    Application();
    ~Application();

    std::mutex mutex_;
    std::deque<std::function<void()>> main_tasks_;
    std::unique_ptr<Protocol> protocol_;
    EventGroupHandle_t event_group_ = nullptr;
    esp_timer_handle_t clock_timer_handle_ = nullptr;
    DeviceStateMachine state_machine_;
    ListeningMode listening_mode_ = kListeningModeAutoStop;
    AecMode aec_mode_ = kAecOff;
    std::string last_error_message_;
    AudioService audio_service_;
    NotifyPlayer notify_player_;
    uint32_t notification_playback_id_ = 0;
    std::unique_ptr<Ota> ota_;

    std::function<void(const std::string&)> mcp_broadcast_callback_;

    bool has_server_time_ = false;
    bool aborted_ = false;
    bool assets_version_checked_ = false;
    bool play_popup_on_listening_ = false;  // Cờ phát âm thanh popup sau khi trạng thái chuyển sang lắng nghe
    bool pending_listening_start_ = false;  // Đang chờ phát hết hàng đợi phát lại trước khi bắt đầu lắng nghe (chế độ tự động)
    int clock_ticks_ = 0;
    TaskHandle_t activation_task_handle_ = nullptr;


    // Các bộ xử lý sự kiện
    void HandleStateChangedEvent();
    void HandleToggleChatEvent();
    void HandleStartListeningEvent();
    void HandleStopListeningEvent();
    void HandleNetworkConnectedEvent();
    void HandleNetworkDisconnectedEvent();
    void HandleActivationDoneEvent();
    void HandleWakeWordDetectedEvent();
    void ContinueOpenAudioChannel(ListeningMode mode);
    void BeginWakeWordInvoke(const std::string& wake_word);
    void ContinueWakeWordInvoke(const std::string& wake_word);
    void StartListeningAudio();
    void ConfigureWakeWordForListening();
    void StartNotification(std::string audio_url, std::vector<NotifySubtitle> subtitles);
    void StopNotification();
    void HandleNotificationFinished(uint32_t playback_id, bool success);

    // Tác vụ kích hoạt (chạy nền)
    void ActivationTask();

    // Các phương thức hỗ trợ
    void CheckAssetsVersion();
    void CheckNewVersion();
    void InitializeProtocol();
    void ShowActivationCode(const std::string& code, const std::string& message);
    void SetListeningMode(ListeningMode mode);
    ListeningMode GetDefaultListeningMode() const;
    
    // Bộ xử lý thay đổi trạng thái được máy trạng thái gọi
    void OnStateChanged(DeviceState old_state, DeviceState new_state);
};


class TaskPriorityReset {
public:
    TaskPriorityReset(BaseType_t priority) {
        original_priority_ = uxTaskPriorityGet(NULL);
        vTaskPrioritySet(NULL, priority);
    }
    ~TaskPriorityReset() {
        vTaskPrioritySet(NULL, original_priority_);
    }

private:
    BaseType_t original_priority_;
};

#endif // _APPLICATION_H_
