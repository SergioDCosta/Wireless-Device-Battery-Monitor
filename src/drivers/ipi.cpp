// IPI PIAO88, 2.4 GHz receiver. Protocol from IPI PIAO v1.0.0.9
// (GetMouseData / GetFeatureReport), verified on receiver firmware v115.
// Feature report 03 reads ten bytes of device information per page. Page 81
// contains the battery in its first payload byte: bit 7 charging, bits 0..6 percent.
#include "drivers.h"

namespace {

bool ReadBattery(HidDev& d, DeviceStatus& s) {
    // Byte 1 is the low byte of the sum of bytes 2..63. HidDev pads with zeros.
    if (!d.SetFeature({0x03, 0x22, 0x50, 0x00, 0x02, 0x4F, 0x81})) return false;
    for (int attempt = 0; attempt < 5; ++attempt) {
        Sleep(30);
        std::vector<BYTE> r;
        if (!d.GetFeature(0x03, r)) return false;
        // Successful device-info reply, page 1. Other pages or a receiver
        // still waiting for the mouse must never be interpreted as a level.
        if (r.size() < 16 || r[0] != 0x03 || r[1] != 0x50 || r[2] != 0 ||
            r[3] != 0x0B || r[4] != 0x02 || r[5] != 0x01) continue;
        int percent = r[6] & 0x7F;
        if (percent > 100) return false;
        s.percent = percent;
        s.charging = (r[6] & 0x80) != 0;
        return true;
    }
    return false;
}

}  // namespace

void PollIpi(PollContext& ctx) {
    constexpr USHORT vid = 0x372E, pid = 0x1014;
    if (ctx.IsClaimed(vid, pid)) return;
    for (const HidColl* c : ctx.Find(vid, pid, [](const HidColl& c) {
             return c.page() == 0xFF00 && c.usage() == 1 && c.featLen() == 64;
         })) {
        if (HidProduct(*c) != L"PIAO-2.4G") continue;
        HidDev d;
        DeviceStatus s;
        if (!d.OpenFeatureOnly(*c) || !ReadBattery(d, s)) continue;
        s.key = L"ipi:piao88";
        s.name = L"IPI PIAO88";
        s.kind = DeviceKind::Mouse;
        s.via = L"2.4 GHz receiver";
        ctx.Add(s, vid, pid);
        return;
    }
}
