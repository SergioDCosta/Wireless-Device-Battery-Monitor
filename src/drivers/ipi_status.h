#pragma once
#include "../devices.h"

#include <algorithm>
#include <cstdint>

inline constexpr wchar_t kIpiKey[] = L"ipi:piao88";

inline bool IsIpiBluetooth(const std::wstring& name, unsigned vid, unsigned pid) {
    // Windows reports the Bluetooth VID as 2E37, the USB VID as 372E.
    return name == L"PIAO-5.3" && ((!vid && !pid) || (vid == 0x2E37 && pid == 0x1056));
}

inline void AddIpiBluetooth(std::vector<DeviceStatus>& devices, DeviceStatus s) {
    auto other = std::find_if(devices.begin(), devices.end(), [](const DeviceStatus& d) { return d.key == kIpiKey; });
    if (other != devices.end() && other->via == L"USB cable") return;
    s.key = kIpiKey;
    s.name = L"IPI PIAO88";
    s.kind = DeviceKind::Mouse;
    if (other == devices.end()) devices.push_back(s);
    else *other = s;
}

// Applied to the live UI only. --list continues to print the actual device report.
class IpiMonitor {
public:
    unsigned Update(std::vector<DeviceStatus>& devices, std::uint64_t now) {
        auto it = std::find_if(devices.begin(), devices.end(), [](const DeviceStatus& d) { return d.key == kIpiKey; });
        if (it == devices.end()) {
            if (via_.empty()) return 60000;
            if (now - lastSeen_ > 15000) {
                via_.clear();
                return 60000;
            }
            DeviceStatus pending;
            pending.key = kIpiKey;
            pending.name = L"IPI PIAO88";
            pending.kind = DeviceKind::Mouse;
            pending.updating = true;
            devices.push_back(pending);
            return 2500;
        }
        const bool unplugged = !via_.empty() && it->via != L"USB cable" &&
                               (via_ == L"USB cable" || (charging_ && !it->charging));
        if (unplugged) {
            // Physical capture: 48 -> 43 -> 41 -> 40 after unplugging.
            earliest_ = now + 20000;
            deadline_ = now + 45000;
            lastChange_ = now;
        } else if (via_ != it->via) {
            // Windows briefly exposes its cached percentage on BLE reconnect.
            earliest_ = it->via == L"Bluetooth" ? now + 3000 : now;
            deadline_ = earliest_;
            lastChange_ = now;
        }
        if (previousPercent_ != it->percent) lastChange_ = now;
        previousPercent_ = it->percent;
        via_ = it->via;
        charging_ = it->charging;
        lastSeen_ = now;
        if (now < deadline_ && (now < earliest_ || now - lastChange_ < 5000)) {
            it->percent = -1;
            it->updating = true;
            return 2500;
        }
        deadline_ = 0;
        return 10000;
    }

private:
    std::wstring via_;
    bool charging_ = false;
    int previousPercent_ = -1;
    std::uint64_t lastSeen_ = 0, earliest_ = 0, deadline_ = 0, lastChange_ = 0;
};
