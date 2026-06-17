#include <cstdint>
#include <array>

using Big5HKSCSTable = std::array<uint32_t, 0xFEFF>;
const Big5HKSCSTable& getBig5HKSCSTable();