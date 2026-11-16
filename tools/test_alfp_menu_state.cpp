#include "legacy2_menu_state.h"
#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

int main(int argc, char** argv) {
    assert(argc == 2);
    std::ifstream file(argv[1], std::ios::binary);
    const std::vector<std::uint8_t> rom{
        std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    assert(rom.size() == 0x800000);
    std::array<std::uint8_t, 0x2000> ram{};
    ram[0x1213] = 4;
    ram[0x10AC] = 3;
    // All six records have initialized HP, but only Trunks is unlocked.
    for (int id = 0; id < 6; ++id) {
        ram[0x10B8 + id * 0x1C] = 85;
        ram[0x10B8 + id * 0x1C + 0x14] = 0x40;
    }
    // Read-only capture of the actual ALFP Trunks record.
    const std::uint8_t trunks[] = {
        0x95,0,0x95,0,0x27,0,0x27,0,0x12,3,0xc8,0x0b,0x2a,9,0,0x0a,
        0xe8,3,0,0,0x41,0,0,0,1,0x0d,0x0c,2};
    std::copy(std::begin(trunks), std::end(trunks), ram.begin() + 0x110C);
    const auto before = ram;
    auto state = legacy2::menu::decode_state(ram.data(), ram.size(),
                                            rom.data(), rom.size(), 7);
    assert(ram == before);
    assert(state[1] == 4 && state[2] == 3 && state[3] == 8 && state[4] == 7);
    const auto* t = state.data() + 5 + 3 * 12;
    assert(t[0] == 1 && t[1] == 3 && t[2] == 6);
    assert(t[3] == 149 && t[4] == 149 && t[5] == 39 && t[6] == 39);
    assert(t[7] == 11 && t[8] == 9 && t[9] == 10);
    assert(t[10] == 1000 && t[11] == 550);
    for (int id : {0, 1, 2, 4, 5}) {
        const auto* locked = state.data() + 5 + id * 12;
        assert(locked[0] == 0);
        for (int n = 2; n < 12; ++n) assert(locked[n] == -1);
    }
    ram[0x10B8 + 0x14] |= 1;
    state = legacy2::menu::decode_state(ram.data(), ram.size(),
                                      rom.data(), rom.size(), 8);
    assert(state[3] == 9 && state[5] == 1);
    const auto max_exp = legacy2::menu::read32(rom.data() + 0x2A6A0 + 49 * 4);
    for (unsigned byte = 0; byte < 4; ++byte)
        ram[0x110C + 0x10 + byte] = (max_exp >> (byte * 8)) & 255;
    state = legacy2::menu::decode_state(ram.data(), ram.size(),
                                      rom.data(), rom.size(), 9);
    assert(state[5 + 3 * 12 + 2] == 50);
    assert(state[5 + 3 * 12 + 11] == -1);
    state = legacy2::menu::decode_state(ram.data(), 0x1000,
                                      rom.data(), rom.size(), 10);
    assert(state[3] == 0 && state[2] == -1);
    std::cout << "PASS: native ALFP stats, availability, EXP/SIG, cap and read-only input\n";
}
