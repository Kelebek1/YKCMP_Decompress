#include "Util.h"
#include <array>
#include <span>
#include <tuple>
#include <numeric>

constexpr u32 GOB_SIZE_X = 64;
constexpr u32 GOB_SIZE_Y = 8;
constexpr u32 GOB_SIZE_Z = 1;
constexpr u32 GOB_SIZE_SHIFT = 9;
constexpr u32 GOB_SIZE_X_SHIFT = 6;
constexpr u32 GOB_SIZE_Y_SHIFT = 3;

struct Extent2D {
    constexpr auto operator<=>(const Extent2D&) const noexcept = default;
    u32 width;
    u32 height;
};

struct Extent3D {
    constexpr auto operator<=>(const Extent3D&) const noexcept = default;
    u32 width;
    u32 height;
    u32 depth;
};

struct LevelInfo {
    Extent3D size;
    Extent3D block;
    Extent2D tile_size;
    u32 bpp_log2;
    u32 tile_width_spacing;
};

using LevelArray = std::array<u32, 15>;
using SwizzleTable = std::array<std::array<u32, GOB_SIZE_X>, GOB_SIZE_Y>;

[[nodiscard]] constexpr u32 BytesPerBlockLog2(u32 bytes_per_block) {
    return std::countl_zero(bytes_per_block) ^ 0x1F;
}

template <typename T>
requires std::is_unsigned_v<T> [[nodiscard]] constexpr T AlignUp(T value, size_t size) {
    auto mod{static_cast<T>(value % size)};
    value -= mod;
    return static_cast<T>(mod == T{0} ? value : value + size);
}

template <typename T>
requires std::is_unsigned_v<T> [[nodiscard]] constexpr T AlignUpLog2(T value, size_t align_log2) {
    return static_cast<T>((value + ((1ULL << align_log2) - 1)) >> align_log2 << align_log2);
}

template <typename N, typename D>
requires std::is_integral_v<N>&& std::is_unsigned_v<D> [[nodiscard]] constexpr N DivCeil(N number,
                                                                                         D divisor) {
    return static_cast<N>((static_cast<D>(number) + divisor - 1) / divisor);
}

template <typename N, typename D>
requires std::is_integral_v<N>&& std::is_unsigned_v<D> [[nodiscard]] constexpr N DivCeilLog2(
    N value, D alignment_log2) {
    return static_cast<N>((static_cast<D>(value) + (D(1) << alignment_log2) - 1) >> alignment_log2);
}

[[nodiscard]] constexpr LevelInfo MakeLevelInfo(Extent3D size, Extent3D block, u32 bytes_per_block,
                                                u32 tile_width_spacing) {
    const auto [samples_x, samples_y] = std::make_pair<u32>(1, 1);
    return {
        .size =
        {
            .width = size.width * samples_x,
            .height = size.height * samples_y,
            .depth = size.depth,
        },
    .block = block,
    .tile_size = {4, 4},
    .bpp_log2 = BytesPerBlockLog2(bytes_per_block),
    .tile_width_spacing = tile_width_spacing,
    };
}

constexpr SwizzleTable MakeSwizzleTable() {
    SwizzleTable table{};
    for (u32 y = 0; y < table.size(); ++y) {
        for (u32 x = 0; x < table[0].size(); ++x) {
            table[y][x] = ((x % 64) / 32) * 256 + ((y % 8) / 2) * 64 + ((x % 32) / 16) * 32 +
                (y % 2) * 16 + (x % 16);
        }
    }
    return table;
}
constexpr SwizzleTable SWIZZLE_TABLE = MakeSwizzleTable();

[[nodiscard]] constexpr u32 AdjustMipSize(u32 size, u32 level) {
    return std::max<u32>(size >> level, 1);
}

[[nodiscard]] constexpr Extent3D AdjustMipSize(Extent3D size, s32 level) {
    return Extent3D{
        .width = AdjustMipSize(size.width, level),
        .height = AdjustMipSize(size.height, level),
        .depth = AdjustMipSize(size.depth, level),
    };
}

[[nodiscard]] constexpr u32 AdjustSize(u32 size, u32 level, u32 block_size) {
    return DivCeil(AdjustMipSize(size, level), block_size);
}

[[nodiscard]] constexpr Extent3D NumLevelBlocks(const LevelInfo& info, u32 level) {
    return Extent3D{
        .width = AdjustSize(info.size.width, level, info.tile_size.width) << info.bpp_log2,
        .height = AdjustSize(info.size.height, level, info.tile_size.height),
        .depth = AdjustMipSize(info.size.depth, level),
    };
}

[[nodiscard]] constexpr u32 AdjustTileSize(u32 shift, u32 unit_factor, u32 dimension) {
    if (shift == 0) {
        return 0;
    }
    u32 x = unit_factor << (shift - 1);
    if (x >= dimension) {
        while (--shift) {
            x >>= 1;
            if (x < dimension) {
                break;
            }
        }
    }
    return shift;
}

[[nodiscard]] constexpr Extent3D AdjustTileSize(Extent3D size, Extent2D tile_size) {
    return {
        .width = DivCeil(size.width, tile_size.width),
        .height = DivCeil(size.height, tile_size.height),
        .depth = size.depth,
    };
}

[[nodiscard]] constexpr Extent3D TileShift(const LevelInfo& info, u32 level) {
    const Extent3D blocks = NumLevelBlocks(info, level);
    return Extent3D{
        .width = AdjustTileSize(info.block.width, GOB_SIZE_X, blocks.width),
        .height = AdjustTileSize(info.block.height, GOB_SIZE_Y, blocks.height),
        .depth = AdjustTileSize(info.block.depth, GOB_SIZE_Z, blocks.depth),
    };
}

[[nodiscard]] constexpr bool IsSmallerThanGobSize(Extent3D num_tiles, Extent2D gob,
                                                  u32 block_depth) {
    return num_tiles.width <= (1U << gob.width) || num_tiles.height <= (1U << gob.height) ||
        num_tiles.depth < (1U << block_depth);
}

[[nodiscard]] constexpr Extent2D GobSize(u32 bpp_log2, u32 block_height, u32 tile_width_spacing) {
    return Extent2D{
        .width = GOB_SIZE_X_SHIFT - bpp_log2 + tile_width_spacing,
        .height = GOB_SIZE_Y_SHIFT + block_height,
    };
}

[[nodiscard]] constexpr Extent2D NumGobs(const LevelInfo& info, u32 level) {
    const Extent3D blocks = NumLevelBlocks(info, level);
    const Extent2D gobs{
        .width = DivCeilLog2(blocks.width, GOB_SIZE_X_SHIFT),
        .height = DivCeilLog2(blocks.height, GOB_SIZE_Y_SHIFT),
    };
    const Extent2D gob = GobSize(info.bpp_log2, info.block.height, info.tile_width_spacing);
    const bool is_small = IsSmallerThanGobSize(blocks, gob, info.block.depth);
    const u32 alignment = is_small ? 0 : info.tile_width_spacing;
    return Extent2D{
        .width = AlignUpLog2(gobs.width, alignment),
        .height = gobs.height,
    };
}

[[nodiscard]] constexpr Extent3D LevelTiles(const LevelInfo& info, u32 level) {
    const Extent3D blocks = NumLevelBlocks(info, level);
    const Extent3D tile_shift = TileShift(info, level);
    const Extent2D gobs = NumGobs(info, level);
    return Extent3D{
        .width = DivCeilLog2(gobs.width, tile_shift.width),
        .height = DivCeilLog2(gobs.height, tile_shift.height),
        .depth = DivCeilLog2(blocks.depth, tile_shift.depth),
    };
}

[[nodiscard]] constexpr u32 CalculateLevelSize(const LevelInfo& info, u32 level) {
    const Extent3D tile_shift = TileShift(info, level);
    const Extent3D tiles = LevelTiles(info, level);
    const u32 num_tiles = tiles.width * tiles.height * tiles.depth;
    const u32 shift = GOB_SIZE_SHIFT + tile_shift.width + tile_shift.height + tile_shift.depth;
    return num_tiles << shift;
}

[[nodiscard]] constexpr LevelArray CalculateLevelSizes(const LevelInfo& info, u32 num_levels) {
    LevelArray sizes{};
    for (u32 level = 0; level < num_levels; ++level) {
        sizes[level] = CalculateLevelSize(info, level);
    }
    return sizes;
}

[[nodiscard]] u32 CalculateLevelBytes(const LevelArray& sizes, u32 num_levels) {
    return std::reduce(sizes.begin(), sizes.begin() + num_levels, 0U);
}

[[nodiscard]] constexpr u32 AlignLayerSize(u32 size_bytes, Extent3D size, Extent3D block,
                                           u32 tile_size_y, u32 tile_width_spacing) {
    if (tile_width_spacing > 0) {
        const u32 alignment_log2 = GOB_SIZE_SHIFT + tile_width_spacing + block.height + block.depth;
        return AlignUpLog2(size_bytes, alignment_log2);
    }
    const u32 aligned_height = AlignUp(size.height, tile_size_y);
    while (block.height != 0 && aligned_height <= (1U << (block.height - 1)) * GOB_SIZE_Y) {
        --block.height;
    }
    while (block.depth != 0 && size.depth <= (1U << (block.depth - 1))) {
        --block.depth;
    }
    const u32 block_shift = GOB_SIZE_SHIFT + block.height + block.depth;
    const u32 num_blocks = size_bytes >> block_shift;
    if (size_bytes != num_blocks << block_shift) {
        return (num_blocks + 1) << block_shift;
    }
    return size_bytes;
}

[[nodiscard]] constexpr u32 NumBlocks(Extent3D size, Extent2D tile_size) {
    const Extent3D num_blocks = AdjustTileSize(size, tile_size);
    return num_blocks.width * num_blocks.height * num_blocks.depth;
}

template <u32 GOB_EXTENT>
[[nodiscard]] constexpr u32 AdjustMipBlockSize(u32 num_tiles, u32 block_size, u32 level) {
    do {
        while (block_size > 0 && num_tiles <= (1U << (block_size - 1)) * GOB_EXTENT) {
            --block_size;
        }
    } while (level--);
    return block_size;
}

[[nodiscard]] constexpr Extent3D AdjustMipBlockSize(Extent3D num_tiles, Extent3D block_size,
                                                    u32 level) {
    return {
        .width = AdjustMipBlockSize<GOB_SIZE_X>(num_tiles.width, block_size.width, level),
        .height = AdjustMipBlockSize<GOB_SIZE_Y>(num_tiles.height, block_size.height, level),
        .depth = AdjustMipBlockSize<GOB_SIZE_Z>(num_tiles.depth, block_size.depth, level),
    };
}

[[nodiscard]] constexpr u32 StrideAlignment(Extent3D num_tiles, Extent3D block, Extent2D gob,
                                            u32 bpp_log2) {
    if (IsSmallerThanGobSize(num_tiles, gob, block.depth)) {
        return GOB_SIZE_X_SHIFT - bpp_log2;
    } else {
        return gob.width;
    }
}