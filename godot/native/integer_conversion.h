#ifndef FOUNDATION_GODOT_INTEGER_CONVERSION_H
#define FOUNDATION_GODOT_INTEGER_CONVERSION_H

#include <cstdint>
#include <limits>

namespace foundation_godot {

inline bool checked_uint64_to_godot_int(
    uint64_t value, int64_t *out_value
)
{
    if (out_value == nullptr
        || value > static_cast<uint64_t>(
            std::numeric_limits<int64_t>::max()
        ))
        return false;
    *out_value = static_cast<int64_t>(value);
    return true;
}

}

#endif
