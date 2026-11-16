#pragma once
#include <algorithm>
#include <cstdint>
#include <limits>

namespace legacy2 {
struct ProjectedWorldMargins { std::uint32_t x=0, y=0; };
// ALFP camera 08004BC4 produces an invertible fixed-1024 rotation.
// In the depth interval accepted by 0801C822, perspective scale <=1024.
// Inverting a horizontal screen displacement therefore bounds BOTH world
// axes. The final world unit covers integer inverse/projection rounding.
inline bool projected_world_margins(std::int32_t a,std::int32_t b,
        std::int32_t c,std::int32_t d,std::uint32_t pixels,
        ProjectedWorldMargins* out) {
    if (!out) return false;
    *out={};
    if (!pixels) return true;
    const auto abs64=[](std::int64_t v) { return v<0 ? -v : v; };
    // These are rotation coefficients from the original ROM routine. Reject
    // uninitialised/non-rotation data rather than widening a different scene.
    if (abs64(a)>1024 || abs64(b)>1024 || abs64(c)>1024 || abs64(d)>1024)
        return false;
    const std::int64_t det=abs64(std::int64_t(a)*d-std::int64_t(b)*c);
    if (det<1000000 || det>1100000) return false;
    const auto pad=[&](std::int32_t coefficient) {
        return std::uint32_t((std::uint64_t(pixels)*1024u*abs64(coefficient)+det-1)/det+1);
    };
    out->x=pad(d);out->y=pad(b);return true;
}
inline std::uint32_t extend_projected_bound(std::uint32_t original,
        std::uint32_t margin,bool upper) {
    const std::int64_t value=std::int32_t(original)+
        (upper ? std::int64_t(margin) : -std::int64_t(margin));
    return std::uint32_t(std::clamp(value,
        std::int64_t(std::numeric_limits<std::int32_t>::min()),
        std::int64_t(std::numeric_limits<std::int32_t>::max())));
}
} // namespace legacy2
