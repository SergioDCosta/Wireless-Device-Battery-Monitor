// Run the PIAO88 poller against the captured device-info response and error cases.
#include "../src/drivers/drivers.h"

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
