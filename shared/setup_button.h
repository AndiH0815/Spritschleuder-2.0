#pragma once
#include <cstdint>
namespace sprit {
enum class ButtonAction { None, Info, Setup };
class SetupButton {
  public:
    ButtonAction update(bool pressed, uint32_t now) {
        if (pressed != raw) { raw = pressed; changed = now; }
        if (raw != stable && uint32_t(now - changed) >= 30) {
            stable = raw;
            if (stable) { started = now; fired = false; }
            else if (!fired) return ButtonAction::Info;
        }
        if (stable && raw && !fired && uint32_t(now - started) >= 5000) {
            fired = true; return ButtonAction::Setup;
        }
        return ButtonAction::None;
    }
  private:
    bool raw = false, stable = false, fired = false;
    uint32_t changed = 0, started = 0;
};
}
