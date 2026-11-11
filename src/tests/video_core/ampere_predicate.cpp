// SPDX-License-Identifier: GPL-3.0-or-later

#include <array>
#include <bit>

#include <catch2/catch_test_macros.hpp>

#include "shader_recompiler/frontend/ampere/lower_to_maxwell.h"

namespace {
using Shader::Ampere::Instruction;
constexpr Instruction Exit{{0x0000794d, 0, 0x03800000, 0}};

template <typename T>
bool Compare(T a, T b, u32 operation) {
    switch (operation) {
    case 1: return a < b;
    case 2: return a == b;
    case 3: return a <= b;
    case 4: return a > b;
    case 5: return a != b;
    case 6: return a >= b;
    default: FAIL("Unexpected comparison"); return false;
    }
}

bool Combine(bool a, bool b, u32 operation) {
    switch (operation) {
    case 0: return a && b;
    case 1: return a || b;
    case 2: return a != b;
    default: FAIL("Unexpected predicate combination"); return false;
    }
}

// Execute the emitted comparison subset against independent 64-bit reference comparisons.
struct PredicateMachine {
    std::array<u32, 256> regs{};
    std::array<bool, 7> predicates{};

    bool Pred(u32 encoded) const {
        const bool value = (encoded & 7) == 7 || predicates[encoded & 7];
        return value != ((encoded & 8) != 0);
    }

    u32 Reg(u32 index) const { return index == 255 ? 0 : regs[index]; }
    void SetPred(u32 index, bool value) {
        if (index != 7) predicates[index] = value;
    }

    void Run(const Shader::Ampere::LoweredProgram& program) {
        for (const u64 word : program.instructions) {
            if ((word >> 56) == 0xe3) break;
            if (!Pred(static_cast<u32>((word >> 16) & 15))) continue;
            const u32 opcode = static_cast<u32>(word >> 52);
            if (opcode == 0x38a || opcode == 0x39a) {
                const u32 immediate = static_cast<u32>((word >> 20) & 0x7ffff) |
                                      ((word & (u64{1} << 56)) ? 0xfff80000U : 0U);
                regs[word & 255] = Pred(static_cast<u32>((word >> 39) & 15))
                                      ? Reg(static_cast<u32>((word >> 8) & 255)) : immediate;
            } else if (opcode == 0x5b6) {
                const u32 a = Reg(static_cast<u32>((word >> 8) & 255));
                const u32 b = Reg(static_cast<u32>((word >> 20) & 255));
                const u32 operation = static_cast<u32>((word >> 49) & 7);
                const bool result = (word & (u64{1} << 48))
                                        ? Compare(std::bit_cast<s32>(a), std::bit_cast<s32>(b), operation)
                                        : Compare(a, b, operation);
                const bool input = Pred(static_cast<u32>((word >> 39) & 15));
                const u32 combine = static_cast<u32>((word >> 45) & 3);
                SetPred(static_cast<u32>((word >> 3) & 7), Combine(result, input, combine));
                SetPred(static_cast<u32>(word & 7), Combine(!result, input, combine));
            } else if (opcode == 0x5be) {
                const u32 a = Reg(static_cast<u32>((word >> 8) & 255));
                const u32 b = Reg(static_cast<u32>((word >> 20) & 255));
                const u32 c = Reg(static_cast<u32>((word >> 39) & 255));
                const u32 lut = static_cast<u32>((word >> 28) & 255);
                u32 result{};
                for (u32 bit = 0; bit < 32; ++bit) {
                    const u32 index = (((a >> bit) & 1) << 2) |
                                      (((b >> bit) & 1) << 1) | ((c >> bit) & 1);
                    result |= ((lut >> index) & 1) << bit;
                }
                REQUIRE(((word >> 36) & 3) == 3); // Nonzero result sets the predicate.
                SetPred(static_cast<u32>((word >> 48) & 7), result != 0);
            } else {
                FAIL("Unexpected instruction in extended comparison");
            }
        }
    }
};
} // namespace

TEST_CASE("Ampere extended comparisons retain aliased input predicates", "[video_core][ampere][predicate]") {
    constexpr std::array<std::array<u64, 2>, 8> values{{
        {0, 0}, {0, 1}, {1, 0}, {0xffffffffULL, 0x100000000ULL},
        {0x100000001ULL, 0x100000000ULL}, {0xffffffffffffffffULL, 0},
        {0x8000000000000000ULL, 0x7fffffffffffffffULL},
        {0x7fffffff00000001ULL, 0x7fffffff00000002ULL},
    }};
    for (const u32 opcode : {0x20cU, 0xc0cU})
    for (u32 compare = 1; compare <= 6; ++compare)
    for (const bool is_signed : {false, true})
    for (u32 combine = 0; combine < 3; ++combine)
    for (const bool negate_low : {false, true})
    for (const bool negate_input : {false, true})
    for (const u32 input : {1U, 3U, 7U}) {
        const Instruction instruction{{0x00ff0000U | (4U << 24) | opcode,
            opcode == 0xc0c ? 9U : 5U,
            (2U << 4) | (static_cast<u32>(negate_low) << 7) | (1U << 8) |
                (static_cast<u32>(is_signed) << 9) | (combine << 10) | (compare << 12) |
                (1U << 17) | (7U << 20) | (input << 23) |
                (static_cast<u32>(negate_input) << 26) | (opcode == 0xc0c ? 1U << 27 : 0U), 0}};
        const auto lowered = Shader::Ampere::LowerToMaxwell(std::array{instruction, Exit});
        REQUIRE(static_cast<bool>(lowered));
        for (const auto& pair : values)
        for (const bool input_value : {false, true})
        for (const bool execute : {false, true}) {
            PredicateMachine state;
            state.regs[4] = static_cast<u32>(pair[0] >> 32);
            state.regs[opcode == 0xc0c ? 225 : 5] = static_cast<u32>(pair[1] >> 32);
            state.predicates[0] = execute;
            state.predicates[1] = input_value;
            state.predicates[3] = input_value;
            state.predicates[2] = Compare(static_cast<u32>(pair[0]), static_cast<u32>(pair[1]), compare)
                                  != negate_low;
            const bool wide_result = is_signed
                ? Compare(std::bit_cast<s64>(pair[0]), std::bit_cast<s64>(pair[1]), compare)
                : Compare(pair[0], pair[1], compare);
            const bool expected = execute ? Combine(wide_result, state.Pred(input | (negate_input ? 8U : 0U)), combine)
                                          : state.predicates[1];
            const auto before = state.predicates;
            state.Run(lowered);
            REQUIRE(state.predicates[1] == expected);
            for (u32 pred : {0U, 2U, 3U, 4U, 5U, 6U})
                REQUIRE(state.predicates[pred] == before[pred]);
        }
    }
}

TEST_CASE("Ampere register shadowing preserves immediate and uniform fields", "[video_core][ampere][pressure]") {
    using Shader::Ampere::PrepareRegisterPressure;
    const Instruction fma{{0xdcdd7a23, 0x00ff00ff, 0x000000e0, 0}};
    const auto pressure = PrepareRegisterPressure(fma, 216);
    REQUIRE(pressure.valid);
    REQUIRE(pressure.instruction.words[1] == fma.words[1]);
    REQUIRE(pressure.instruction.Bits(16, 8) < 216);
    REQUIRE(pressure.instruction.Bits(24, 8) < 216);
    REQUIRE(pressure.instruction.Bits(64, 8) < 216);
    const Instruction uniform_move{{0x00dc7c02, 63, 0x08000f00, 0}};
    const auto move = PrepareRegisterPressure(uniform_move, 216);
    REQUIRE(move.valid);
    REQUIRE(move.instruction.words[1] == 63);
    REQUIRE(move.instruction.Bits(16, 8) < 216);
}

TEST_CASE("Ampere attribute vectors cannot straddle shadow quartets", "[video_core][ampere][pressure]") {
    const Instruction aligned{{0xfff47321, 0x000100ff, 0x00000c00, 0}};
    const auto pressure = Shader::Ampere::PrepareRegisterPressure(aligned, 216);
    REQUIRE(pressure.valid);
    REQUIRE(pressure.instruction.Bits(16, 8) % 4 == 0);
    REQUIRE(pressure.prefix.size() == 8);
    REQUIRE(pressure.suffix.size() == 8);
    auto straddling = aligned;
    straddling.words[0] += 1U << 16;
    REQUIRE_FALSE(Shader::Ampere::PrepareRegisterPressure(straddling, 216).valid);
}

TEST_CASE("Ampere shadowed global loads retain address pairs and live registers", "[video_core][ampere][pressure]") {
    const Instruction load{{0xded87381, 0, 0x001ead00, 0}};
    const auto pressure = Shader::Ampere::PrepareRegisterPressure(load, 216);
    REQUIRE(pressure.valid);
    REQUIRE(static_cast<bool>(Shader::Ampere::LowerToMaxwell(std::array{load, Exit})));

    std::array<std::array<u32, 256>, 3> banks{};
    for (u32 bank = 0; bank < banks.size(); ++bank)
        for (u32 reg = 0; reg < banks[bank].size(); ++reg)
            banks[bank][reg] = bank * 1000 + reg;
    const auto before = banks;
    const auto copies = [&](const auto& instructions) {
        for (const u64 word : instructions) {
            const u32 destination_bank = static_cast<u32>((word >> 8) & 3);
            const u32 source_bank = static_cast<u32>((word >> 10) & 3);
            banks[destination_bank][word & 255] = banks[source_bank][(word >> 20) & 255];
        }
    };
    copies(pressure.prefix);
    const u32 address = pressure.instruction.Bits(24, 8);
    REQUIRE(banks[0][address] == before[1][222]);
    REQUIRE(banks[0][address + 1] == before[1][223]);
    const u32 destination = pressure.instruction.Bits(16, 8);
    REQUIRE(destination % 4 == 0);
    for (u32 component = 0; component < 4; ++component)
        banks[0][destination + component] = 0x12340000 + component;
    copies(pressure.suffix);
    REQUIRE(banks[0] == before[0]);
    for (u32 reg = 0; reg < 256; ++reg)
        REQUIRE(banks[1][reg] == (reg >= 216 && reg < 220 ? 0x12340000 + reg - 216 : before[1][reg]));
}

TEST_CASE("Ampere shadowing keeps predicate and immediate encodings intact", "[video_core][ampere][pressure]") {
    const Instruction compare{{0xda00780c, 0x00020000, 0x04701070, 0}};
    const auto pressure = Shader::Ampere::PrepareRegisterPressure(compare, 216);
    REQUIRE(pressure.valid);
    REQUIRE(pressure.instruction.Bits(24, 8) < 216);
    REQUIRE(pressure.instruction.Bits(16, 8) == 0);
    REQUIRE(pressure.instruction.words[1] == compare.words[1]);
    REQUIRE(pressure.instruction.words[2] == compare.words[2]);

    const Instruction stage_write{{0xd9007927, 0x000000fc, 0x0000f800, 0}};
    const auto write = Shader::Ampere::PrepareRegisterPressure(stage_write, 216);
    REQUIRE(write.valid);
    REQUIRE(write.instruction.Bits(24, 8) < 216);
    REQUIRE(write.instruction.Bits(32, 8) < 216);
    REQUIRE(write.instruction.words[2] == stage_write.words[2]);
    REQUIRE(static_cast<bool>(Shader::Ampere::LowerToMaxwell(std::array{stage_write, Exit})));
}
