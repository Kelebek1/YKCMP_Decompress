#include "swizzle.h"

template <bool TO_LINEAR>
void Swizzle(u8* output, u8* input, u32 bytes_per_pixel, u32 width,
             u32 height, u32 depth, u32 block_height, u32 block_depth, u32 stride_alignment = 1) {
    // The origin of the transformation can be configured here, leave it as zero as the current API
    // doesn't expose it.
    static constexpr u32 origin_x = 0;
    static constexpr u32 origin_y = 0;
    static constexpr u32 origin_z = 0;

    // We can configure here a custom pitch
    // As it's not exposed 'width * bpp' will be the expected pitch.
    const u32 pitch = width * bytes_per_pixel;
    const u32 stride = AlignUpLog2(width, stride_alignment) * bytes_per_pixel;

    const u32 gobs_in_x = DivCeilLog2(stride, GOB_SIZE_X_SHIFT);
    const u32 block_size = gobs_in_x << (GOB_SIZE_SHIFT + block_height + block_depth);
    const u32 slice_size =
        DivCeilLog2(height, block_height + GOB_SIZE_Y_SHIFT) * block_size;

    const u32 block_height_mask = (1U << block_height) - 1;
    const u32 block_depth_mask = (1U << block_depth) - 1;
    const u32 x_shift = GOB_SIZE_SHIFT + block_height + block_depth;

    for (u32 slice = 0; slice < depth; ++slice) {
        const u32 z = slice + origin_z;
        const u32 offset_z = (z >> block_depth) * slice_size +
            ((z & block_depth_mask) << (GOB_SIZE_SHIFT + block_height));
        for (u32 line = 0; line < height; ++line) {
            const u32 y = line + origin_y;
            const auto& table = SWIZZLE_TABLE[y % GOB_SIZE_Y];

            const u32 block_y = y >> GOB_SIZE_Y_SHIFT;
            const u32 offset_y = (block_y >> block_height) * block_size +
                ((block_y & block_height_mask) << GOB_SIZE_SHIFT);

            for (u32 column = 0; column < width; ++column) {
                const u32 x = (column + origin_x) * bytes_per_pixel;
                const u32 offset_x = (x >> GOB_SIZE_X_SHIFT) << x_shift;

                const u32 base_swizzled_offset = offset_z + offset_y + offset_x;
                const u32 swizzled_offset = base_swizzled_offset + table[x % GOB_SIZE_X];

                const u32 unswizzled_offset =
                    slice * pitch * height + line * pitch + column * bytes_per_pixel;

                std::memcpy(&output[TO_LINEAR ? swizzled_offset : unswizzled_offset], 
                            &input[TO_LINEAR ? unswizzled_offset : swizzled_offset], 
                            bytes_per_pixel);
            }
        }
    }
}

extern "C" __declspec(dllexport)
void UnswizzleImage(u8* src, u8* dst,
                    u32 width, u32 height, u32 depth, u32 mipmaps,
                    u32 fmt, u32 tile_width_spacing, u32 block_height) {
    const auto format = static_cast<PixelFormat>(fmt);
    const auto bytes_per_block = BytesPerBlock(format);
    const u32 bpp_log2 = BytesPerBlockLog2(bytes_per_block);
    const Extent3D size = {.width = width, .height = height, .depth = depth};
    const Extent3D block2 = {.width = 0, .height = block_height, .depth = 0};

    const LevelInfo level_info = MakeLevelInfo(format, size, block2, bytes_per_block, tile_width_spacing);
    const s32 num_levels = mipmaps;
    const Extent2D tile_size = DefaultBlockSize(format);
    const std::array level_sizes = CalculateLevelSizes(level_info, num_levels);
    const Extent2D gob = GobSize(bpp_log2, block_height, tile_width_spacing);
    const u32 layer_size = CalculateLevelBytes(level_sizes, num_levels);
    const u32 layer_stride = AlignLayerSize(layer_size, size, level_info.block, tile_size.height,
                                            tile_width_spacing);
    size_t guest_offset = 0;
    u32 host_offset = 0;

    for (s32 level = 0; level < num_levels; ++level) {
        const Extent3D level_size = AdjustMipSize(size, level);
        const u32 num_blocks_per_layer = NumBlocks(level_size, tile_size);
        const u32 host_bytes_per_layer = num_blocks_per_layer << bpp_log2;

        const Extent3D num_tiles = AdjustTileSize(level_size, tile_size);
        const Extent3D block = AdjustMipBlockSize(num_tiles, level_info.block, level);
        const u32 stride_alignment = StrideAlignment(num_tiles, block, gob, bpp_log2);

        Swizzle<false>(dst + host_offset, src + guest_offset, 
                        1U << bpp_log2, num_tiles.width, num_tiles.height,
                        num_tiles.depth, block.height, block.depth, stride_alignment);

        host_offset += host_bytes_per_layer;
        guest_offset += level_sizes[level];
    }
}

extern "C" __declspec(dllexport)
void SwizzleImage(u8 * src, u8 * dst,
                    u32 width, u32 height, u32 depth, u32 mipmaps,
                    u32 fmt, u32 tile_width_spacing, u32 block_height) {
    const auto format = static_cast<PixelFormat>(fmt);
    const auto bytes_per_block = BytesPerBlock(format);
    const u32 bpp_log2 = BytesPerBlockLog2(bytes_per_block);
    const Extent3D size = {.width = width, .height = height, .depth = depth};
    const Extent3D block2 = {.width = 0, .height = block_height, .depth = 0};

    const LevelInfo level_info = MakeLevelInfo(static_cast<PixelFormat>(format), size, block2, bytes_per_block, tile_width_spacing);
    const Extent2D tile_size = DefaultBlockSize(format);

    const s32 level = 0;
    const Extent3D level_size = AdjustMipSize(size, level);
    const u32 num_blocks_per_layer = NumBlocks(level_size, tile_size);
    const u32 host_bytes_per_layer = num_blocks_per_layer << bpp_log2;

    const Extent2D gob = GobSize(bpp_log2, block_height, tile_width_spacing);
    const Extent3D num_tiles = AdjustTileSize(level_size, tile_size);
    const Extent3D block = AdjustMipBlockSize(num_tiles, level_info.block, level);
    const u32 stride_alignment = StrideAlignment(num_tiles, block, gob, bpp_log2);

    Swizzle<true>(dst, src, bytes_per_block, num_tiles.width, num_tiles.height,
                    num_tiles.depth, block.height, block.depth, stride_alignment);

}
