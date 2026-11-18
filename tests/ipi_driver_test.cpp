// Run the PIAO88 poller against the captured device-info response and error cases.
#include "../src/drivers/drivers.h"
#include "../src/drivers/ipi_status.h"

#include <cassert>
#include <deque>

namespace {
std::deque<std::vector<BYTE>> replies;
int requests = 0;

HidColl Receiver() {
    HidColl c;
    c.vid = 0x372E;
    c.pid = 0x1014;
    c.product = L"PIAO-2.4G";
    c.caps.UsagePage = 0xFF00;
    c.caps.Usage = 1;
    c.caps.FeatureReportByteLength = 64;
    return c;
}

PollContext Poll(std::deque<std::vector<BYTE>> frames, const HidColl& c = Receiver()) {
    replies = std::move(frames);
    requests = 0;
    PollContext ctx;
    ctx.colls = {c};
    PollIpi(ctx);
    return ctx;
}
}  // namespace

std::wstring HidProduct(const HidColl& c) { return c.product; }
bool HidDev::OpenFeatureOnly(const HidColl&) { return true; }
void HidDev::Close() {}
bool HidDev::SetFeature(const std::vector<BYTE>& data) {
    const std::vector<BYTE> query = {0x03, 0x22, 0x50, 0, 0x02, 0x4F, 0x81};
    assert(data == query);
    ++requests;
    return true;
}
bool HidDev::GetFeature(BYTE id, std::vector<BYTE>& out) {
    assert(id == 0x03);
    if (replies.empty()) return false;
    out = replies.front();
    replies.pop_front();
    return true;
}

int main() {
    std::vector<BYTE> page = {3, 0x50, 0, 11, 2, 1, 0x23, 0x21, 7, 2, 0x30, 0x75, 4, 0xA5, 0x5A, 0};
    const std::vector<BYTE> waiting = {3, 0, 2, 0, 15};
    auto ctx = Poll({waiting, page});
    assert(requests == 1 && ctx.out.size() == 1 && ctx.IsClaimed(0x372E, 0x1014));
    assert(ctx.out[0].percent == 35 && !ctx.out[0].charging && ctx.out[0].kind == DeviceKind::Mouse);

    auto wired = Receiver();
    wired.pid = 0x1056;
    wired.product = L"PIAO";
    page[6] = 0xD6;  // Captured through the cable: 86% with the charging bit set.
    ctx = Poll({page}, wired);
    assert(ctx.out.size() == 1 && ctx.out[0].key == kIpiKey && ctx.out[0].via == L"USB cable");
    assert(ctx.out[0].percent == 86 && ctx.out[0].charging);
    DeviceStatus cable = ctx.out.front(), radio = cable, bt = cable;
    ctx = {};
    ctx.colls = {Receiver(), wired};
    replies = {page};
    requests = 0;
    PollIpi(ctx);
    assert(requests == 1 && ctx.out.size() == 1 && ctx.out[0].via == L"USB cable");
    radio.via = L"2.4 GHz receiver";
    radio.charging = false;
    bt.via = L"Bluetooth";
    bt.percent = 37;
    bt.charging = false;
    assert(IsIpiBluetooth(L"PIAO-5.3", 0x2E37, 0x1056));
    assert(!IsIpiBluetooth(L"PIAO-5.3", 0x1234, 0x1056));
    assert(!IsIpiBluetooth(L"Other mouse", 0, 0));
    AddIpiBluetooth(ctx.out, bt);
    assert(ctx.out.size() == 1 && ctx.out[0].via == cable.via);
    ctx.out = {radio};
    AddIpiBluetooth(ctx.out, bt);
    assert(ctx.out.size() == 1 && ctx.out[0].via == bt.via && ctx.out[0].key == kIpiKey);

    IpiMonitor monitor;
    auto poll = [&](std::vector<DeviceStatus> readings, unsigned now) {
        monitor.Update(readings, now);
        return readings;
    };
    assert(poll({cable}, 0).front().percent == 86);
    assert(poll({}, 1000).front().updating);
    radio.percent = 48;
    assert(poll({radio}, 3000).front().percent == -1);
    radio.percent = 43;
    assert(poll({radio}, 15000).front().updating);
    radio.percent = 40;
    assert(poll({radio}, 23000).front().updating);
    assert(poll({radio}, 28000).front().percent == 40);
    assert(poll({bt}, 29000).front().updating);
    assert(poll({bt}, 34000).front().percent == 37);
    assert(poll({}, 51000).empty());

    // A USB charger can attach without changing the receiver PID. A noisy
    // post-charge estimate must eventually be shown, even if it keeps moving.
    radio.charging = true;
    assert(!poll({radio}, 52000).front().updating);
    radio.charging = false;
    assert(poll({radio}, 53000).front().updating);
    for (unsigned time = 55000; time < 98000; time += 2000) {
        radio.percent = radio.percent == 40 ? 41 : 40;
        assert(poll({radio}, time).front().updating);
    }
    assert(!poll({radio}, 98000).front().updating);

    page[6] = 0x80 | 100;
    ctx = Poll({page});
    assert(ctx.out.size() == 1 && ctx.out[0].percent == 100 && ctx.out[0].charging);
    page[6] = 0;
    ctx = Poll({page});
    assert(ctx.out.size() == 1 && ctx.out[0].percent == 0);
    page[6] = 101;
    assert(Poll({page}).out.empty());
    page[6] = 3;
    page[5] = 0;  // Model-info page; its first byte is unrelated to charge.
    assert(Poll({page, waiting}).out.empty());
    assert(Poll({}).out.empty());

    auto other = Receiver();
    other.product = L"Other receiver";
    assert(Poll({}, other).out.empty() && requests == 0);
    other = Receiver();
    other.caps.UsagePage = 0xFF06;
    assert(Poll({}, other).out.empty() && requests == 0);
    other = Receiver();
    other.pid = 0x106C;
    assert(Poll({}, other).out.empty() && requests == 0);
}
