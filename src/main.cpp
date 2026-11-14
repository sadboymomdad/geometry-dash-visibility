#include <Geode/Geode.hpp>
#include <Geode/modify/CCNode.hpp>
#include <CoreGraphics/CoreGraphics.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

using namespace geode::prelude;

namespace Visibility {
    struct OriginalGamma {
        CGDirectDisplayID display = kCGNullDirectDisplay;
        uint32_t count = 0;
        std::vector<CGGammaValue> red;
        std::vector<CGGammaValue> green;
        std::vector<CGGammaValue> blue;
        bool saved = false;
    };

    OriginalGamma original;
    double lastAmount = -1.0;

    void restore() {
        if (!original.saved) return;

        CGError result = CGSetDisplayTransferByTable(
            original.display, original.count,
            original.red.data(), original.green.data(), original.blue.data()
        );

        if (result != kCGErrorSuccess) {
            log::warn("Could not restore original display gamma (CoreGraphics error {}).",
                      static_cast<int>(result));
        }

        original = OriginalGamma{};
        lastAmount = -1.0;
    }

    bool saveOriginal(CGDirectDisplayID display) {
        uint32_t capacity = CGDisplayGammaTableCapacity(display);
        if (capacity == 0 || capacity > 4096) {
            log::warn("This display does not provide a usable gamma table.");
            return false;
        }

        OriginalGamma saved;
        saved.display = display;
        saved.red.resize(capacity);
        saved.green.resize(capacity);
        saved.blue.resize(capacity);

        CGError result = CGGetDisplayTransferByTable(
            display, capacity,
            saved.red.data(), saved.green.data(), saved.blue.data(),
            &saved.count
        );

        if (result != kCGErrorSuccess || saved.count == 0 || saved.count > capacity) {
            log::warn("Could not read the original display gamma (CoreGraphics error {}).",
                      static_cast<int>(result));
            return false;
        }

        saved.red.resize(saved.count);
        saved.green.resize(saved.count);
        saved.blue.resize(saved.count);
        saved.saved = true;
        original = std::move(saved);
        return true;
    }

    void apply() {
        const double amount = Mod::get()->getSettingValue<double>("visibility");

        if (amount <= 0.001) {
            restore();
            return;
        }

        const CGDirectDisplayID display = CGMainDisplayID();
        if (original.saved && original.display != display) {
            restore();
        }

        if (!original.saved && !saveOriginal(display)) return;
        if (std::abs(amount - lastAmount) < 0.0001) return;

        const float t = static_cast<float>(std::clamp(amount, 0.0, 1.0));
        const float gamma = 1.0f + 1.25f * t;
        const float brightness = 1.0f + 0.16f * t;

        std::vector<CGGammaValue> red(original.count);
        std::vector<CGGammaValue> green(original.count);
        std::vector<CGGammaValue> blue(original.count);

        for (uint32_t i = 0; i < original.count; ++i) {
            const float input = original.count > 1
                ? static_cast<float>(i) / static_cast<float>(original.count - 1)
                : 0.0f;

            auto adjust = [&](CGGammaValue value) {
                const float base = std::clamp(static_cast<float>(value), 0.0f, 1.0f);
                return static_cast<CGGammaValue>(
                    std::clamp(std::pow(base, 1.0f / gamma) * brightness, 0.0f, 1.0f)
                );
            };

            red[i] = adjust(original.red[i]);
            green[i] = adjust(original.green[i]);
            blue[i] = adjust(original.blue[i]);
            (void)input;
        }

        CGError result = CGSetDisplayTransferByTable(
            original.display, original.count,
            red.data(), green.data(), blue.data()
        );

        if (result == kCGErrorSuccess) {
            lastAmount = amount;
            log::info("Applied display gamma adjustment.");
        } else {
            log::warn("Could not apply display gamma (CoreGraphics error {}).",
                      static_cast<int>(result));
        }
    }

    struct RestoreOnUnload {
        ~RestoreOnUnload() {
            restore();
        }
    };

    RestoreOnUnload restoreOnUnload;
}

class $modify(VisibilityNode, CCNode) {
    void visit() {
        CCNode::visit();

        if (this == static_cast<CCNode*>(CCDirector::sharedDirector()->getRunningScene())) {
            Visibility::apply();
        }
    }
};
