#ifndef DEVICE_STATE_MACHINE_H
#define DEVICE_STATE_MACHINE_H

#include <atomic>
#include <functional>
#include <mutex>
#include <vector>

#include "device_state.h"

/**
 * DeviceStateMachine - Quản lý chuyển trạng thái thiết bị có kiểm tra hợp lệ
 * 
 * Lớp này đảm bảo các quy tắc chuyển trạng thái nghiêm ngặt và cung cấp cơ chế callback
 * để các thành phần phản ứng với thay đổi trạng thái.
 */
class DeviceStateMachine {
public:
    DeviceStateMachine();
    ~DeviceStateMachine() = default;

    // Xóa hàm khởi tạo sao chép và toán tử gán
    DeviceStateMachine(const DeviceStateMachine&) = delete;
    DeviceStateMachine& operator=(const DeviceStateMachine&) = delete;

    /**
     * Lấy trạng thái thiết bị hiện tại
     */
    DeviceState GetState() const { return current_state_.load(); }

    /**
     * Thử chuyển sang trạng thái mới
     * @param new_state Trạng thái đích
     * @return true nếu chuyển thành công, false nếu chuyển không hợp lệ
     */
    bool TransitionTo(DeviceState new_state);

    /**
     * Kiểm tra việc chuyển sang trạng thái đích có hợp lệ từ trạng thái hiện tại không
     */
    bool CanTransitionTo(DeviceState target) const;

    /**
     * Kiểu callback thay đổi trạng thái
     * Tham số: old_state, new_state
     */
    using StateCallback = std::function<void(DeviceState, DeviceState)>;

    /**
     * Thêm bộ lắng nghe thay đổi trạng thái (mẫu observer)
     * Callback được gọi trong ngữ cảnh của bên gọi TransitionTo()
     * @return id của bộ lắng nghe để gỡ bỏ
     */
    int AddStateChangeListener(StateCallback callback);

    /**
     * Gỡ bộ lắng nghe thay đổi trạng thái theo id
     */
    void RemoveStateChangeListener(int listener_id);

    /**
     * Lấy chuỗi tên trạng thái để ghi log
     */
    static const char* GetStateName(DeviceState state);

private:
    std::atomic<DeviceState> current_state_{kDeviceStateUnknown};
    std::vector<std::pair<int, StateCallback>> listeners_;
    int next_listener_id_{0};
    std::mutex mutex_;

    /**
     * Kiểm tra việc chuyển từ trạng thái nguồn sang đích có hợp lệ không
     */
    bool IsValidTransition(DeviceState from, DeviceState to) const;

    /**
     * Thông báo cho callback về thay đổi trạng thái
     */
    void NotifyStateChange(DeviceState old_state, DeviceState new_state);
};

#endif // DEVICE_STATE_MACHINE_H
