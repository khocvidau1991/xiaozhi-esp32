#include "device_state_machine.h"

#include <algorithm>
#include <esp_log.h>

static const char* TAG = "StateMachine";

// Chuỗi tên trạng thái dùng để ghi log
static const char* const STATE_STRINGS[] = {
    "unknown",
    "starting",
    "wifi_configuring",
    "idle",
    "connecting",
    "listening",
    "speaking",
    "notifying",
    "upgrading",
    "activating",
    "audio_testing",
    "fatal_error",
    "invalid_state"
};

DeviceStateMachine::DeviceStateMachine() {
}

const char* DeviceStateMachine::GetStateName(DeviceState state) {
    if (state >= 0 && state <= kDeviceStateFatalError) {
        return STATE_STRINGS[state];
    }
    return STATE_STRINGS[kDeviceStateFatalError + 1];
}

bool DeviceStateMachine::IsValidTransition(DeviceState from, DeviceState to) const {
    // Cho phép chuyển sang cùng trạng thái (không làm gì)
    if (from == to) {
        return true;
    }

    // Định nghĩa các chuyển trạng thái hợp lệ dựa trên sơ đồ trạng thái
    switch (from) {
        case kDeviceStateUnknown:
            // Chỉ có thể sang starting
            return to == kDeviceStateStarting;

        case kDeviceStateStarting:
            // Có thể sang cấu hình wifi hoặc kích hoạt
            return to == kDeviceStateWifiConfiguring ||
                   to == kDeviceStateActivating;

        case kDeviceStateWifiConfiguring:
            // Có thể sang kích hoạt (sau khi wifi kết nối) hoặc kiểm tra âm thanh
            return to == kDeviceStateActivating ||
                   to == kDeviceStateAudioTesting;

        case kDeviceStateAudioTesting:
            // Có thể quay lại cấu hình wifi
            return to == kDeviceStateWifiConfiguring;

        case kDeviceStateActivating:
            // Có thể sang nâng cấp, rảnh, hoặc quay lại cấu hình wifi (khi lỗi)
            return to == kDeviceStateUpgrading ||
                   to == kDeviceStateIdle ||
                   to == kDeviceStateWifiConfiguring;

        case kDeviceStateUpgrading:
            // Có thể sang rảnh (nâng cấp thất bại) hoặc kích hoạt
            return to == kDeviceStateIdle ||
                   to == kDeviceStateActivating;

        case kDeviceStateIdle:
            // Có thể sang kết nối, lắng nghe (chế độ thủ công), nói, kích hoạt, nâng cấp hoặc cấu hình wifi
            return to == kDeviceStateConnecting ||
                   to == kDeviceStateListening ||
                   to == kDeviceStateSpeaking ||
                   to == kDeviceStateNotifying ||
                   to == kDeviceStateActivating ||
                   to == kDeviceStateUpgrading ||
                   to == kDeviceStateWifiConfiguring;

        case kDeviceStateConnecting:
            // Có thể sang rảnh (thất bại) hoặc lắng nghe (thành công)
            return to == kDeviceStateIdle ||
                   to == kDeviceStateListening;

        case kDeviceStateListening:
            // Có thể sang nói hoặc rảnh
            return to == kDeviceStateSpeaking ||
                   to == kDeviceStateIdle;

        case kDeviceStateSpeaking:
            // Có thể sang lắng nghe hoặc rảnh
            return to == kDeviceStateListening ||
                   to == kDeviceStateIdle;

        case kDeviceStateNotifying:
            return to == kDeviceStateIdle;

        case kDeviceStateFatalError:
            // Không thể chuyển ra khỏi trạng thái lỗi nghiêm trọng
            return false;

        default:
            return false;
    }
}

bool DeviceStateMachine::CanTransitionTo(DeviceState target) const {
    return IsValidTransition(current_state_.load(), target);
}

bool DeviceStateMachine::TransitionTo(DeviceState new_state) {
    DeviceState old_state = current_state_.load();
    
    // Không làm gì nếu đã ở trạng thái đích
    if (old_state == new_state) {
        return true;
    }

    // Kiểm tra tính hợp lệ của việc chuyển trạng thái
    if (!IsValidTransition(old_state, new_state)) {
        ESP_LOGW(TAG, "Invalid state transition: %s -> %s",
                 GetStateName(old_state), GetStateName(new_state));
        return false;
    }

    // Thực hiện chuyển trạng thái
    current_state_.store(new_state);
    ESP_LOGI(TAG, "State: %s -> %s",
             GetStateName(old_state), GetStateName(new_state));

    // Thông báo cho callback
    NotifyStateChange(old_state, new_state);
    return true;
}

int DeviceStateMachine::AddStateChangeListener(StateCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    int id = next_listener_id_++;
    listeners_.emplace_back(id, std::move(callback));
    return id;
}

void DeviceStateMachine::RemoveStateChangeListener(int listener_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    listeners_.erase(
        std::remove_if(listeners_.begin(), listeners_.end(),
            [listener_id](const auto& p) { return p.first == listener_id; }),
        listeners_.end());
}

void DeviceStateMachine::NotifyStateChange(DeviceState old_state, DeviceState new_state) {
    std::vector<StateCallback> callbacks_copy;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        callbacks_copy.reserve(listeners_.size());
        for (const auto& [id, cb] : listeners_) {
            callbacks_copy.push_back(cb);
        }
    }
    
    for (const auto& cb : callbacks_copy) {
        cb(old_state, new_state);
    }
}
