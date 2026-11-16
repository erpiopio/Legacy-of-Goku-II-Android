#pragma once

#include <cstdint>

namespace gbarecomp {
struct ExtendedViewFrameInfo;
}

namespace legacy2 {

void install_extended_view(std::uint32_t extra_left,
                           std::uint32_t extra_right);
void update_extended_view(const gbarecomp::ExtendedViewFrameInfo* frame);

}  // namespace legacy2
