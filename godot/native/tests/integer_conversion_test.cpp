#include <cstdint>
#include <limits>

#include "../integer_conversion.h"

int main()
{
    int64_t output = -1;
    using foundation_godot::checked_uint64_to_godot_int;

    if (!checked_uint64_to_godot_int(0U, &output) || output != 0)
        return 1;
    if (!checked_uint64_to_godot_int(1U, &output) || output != 1)
        return 2;
    const uint64_t maximum = static_cast<uint64_t>(
        std::numeric_limits<int64_t>::max()
    );
    if (!checked_uint64_to_godot_int(maximum, &output)
        || output != std::numeric_limits<int64_t>::max())
        return 3;

    output = 17;
    if (checked_uint64_to_godot_int(maximum + 1U, &output)
        || output != 17)
        return 4;
    if (checked_uint64_to_godot_int(
            std::numeric_limits<uint64_t>::max(), &output
        ) || output != 17)
        return 5;
    if (checked_uint64_to_godot_int(1U, nullptr))
        return 6;
    return 0;
}
