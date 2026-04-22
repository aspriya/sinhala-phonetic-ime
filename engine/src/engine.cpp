#include "sinhala_ime/engine.h"

#include <memory>
#include <stdexcept>
#include <string>

#include "sinhala_ime/composition.h"
#include "sinhala_ime/mapping.h"

namespace sinhala_ime {
namespace {

class EngineImpl final : public Engine {
public:
    explicit EngineImpl(Mapping&& mapping)
        : mapping_(std::make_unique<Mapping>(std::move(mapping))),
          composer_(*mapping_) {}

    FeedResult feed(const KeyEvent& ev) override {
        FeedResult out;
        try {
            char32_t c = ev.codepoint;
            if (!ev.is_backspace && has_shift(ev.mods)) {
                c = shift_key(c);
            }
            auto oc = composer_.feed(c, ev.is_backspace);
            out.committed   = std::move(oc.committed);
            out.composition = std::move(oc.composition);
            out.consumed    = oc.consumed;
        } catch (...) {
            out.consumed = false;
        }
        return out;
    }

    std::string commit() override {
        try {
            return composer_.commit();
        } catch (...) {
            return {};
        }
    }

    void reset() override { composer_.reset(); }

    std::string composition() const override { return composer_.composition(); }

private:
    std::unique_ptr<Mapping> mapping_;  // owned; referenced by composer_
    Composer                 composer_;
};

}  // namespace

std::unique_ptr<Engine> Engine::from_mapping_json(std::string_view json) {
    auto m = Mapping::from_json_string(json);
    return std::make_unique<EngineImpl>(std::move(m));
}

std::unique_ptr<Engine> Engine::from_mapping_file(const std::string& path) {
    auto m = Mapping::from_file(path);
    return std::make_unique<EngineImpl>(std::move(m));
}

}  // namespace sinhala_ime
