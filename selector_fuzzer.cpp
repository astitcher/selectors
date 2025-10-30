#include <cstdint>
#include <sstream>
#include <string_view>
#include <unordered_map>

#include "SelectorExpression.h"
#include "SelectorEnv.h"

class NullEnv : public selector::Env {
    static constexpr selector::Value EMPTY{};

    const selector::Value& value(std::string_view v) const override {
        return EMPTY;
    }
};

// This fuzzer focuses only on testing selector::make_selector() (the parser/AST builder).
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (!data || size == 0) return 0;
    // Interpret input bytes as a string (may contain zeros)
    std::string_view expr_str(reinterpret_cast<const char*>(data), size);

    try {
        auto expr = selector::make_selector(expr_str);
        if (expr) {
            std::ostringstream oss;
            // exercise repr (may throw for malformed ASTs)
            expr->repr(oss);
            expr->eval_bool(NullEnv{});
        }
    } catch (...) {
        // Fuzzer should still continue even if exceptions are thrown
    }

    return 0;
}
