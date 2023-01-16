
#include <fstream>
#include <vector>
#include "Util.h"

void UnswizzleImage(u8* src, u8* dst,
    u32 width, u32 height, u32 depth, u32 mipmaps,
    u32 bytes_per_block, u32 tile_width_spacing, u32 block_height);

typedef struct {
    uint8_t magic[8];
    uint8_t unk_08[8];
    uint8_t unk_10[4];
    uint8_t type;
    uint8_t unk_15[1];
    uint8_t unk_16[2];
    uint32_t width;
    uint32_t height;
    uint8_t unk_20[2];
    uint8_t unk_22[2];
    uint8_t unk_24[1];
    uint8_t mipmaps;
    uint8_t unk_26[1];
    uint8_t unk_27[5];
    uint32_t decompSize;
    uint32_t compSize;
    uint8_t unk_34[4];
    uint8_t block_height;
    uint8_t tile_spacing;
    uint8_t unk_3A[2];
    uint8_t unk_3C[2];
    uint8_t unk_3E[2];
    uint8_t pad_40[0x40];
} TEX_HDR;
static_assert(sizeof(TEX_HDR) == 0x80);

/*
int main() {
    std::ifstream f("test.bin", std::ios_base::in | std::ios_base::binary);

    f.seekg(0, std::ios_base::end);

    const auto file_size = f.tellg();

    std::vector<u8> fd(file_size, 0);
    std::vector<u8> new_fd(file_size, 0);

    f.seekg(0, std::ios_base::beg);

    f.read((char*)fd.data(), file_size);

    f.close();

    TEX_HDR hdr{};
    hdr.width = 512;
    hdr.height = 512;
    hdr.mipmaps = 1;
    hdr.tile_spacing = 4;
    hdr.block_height = 3;

    UnswizzleImage(fd.data(), new_fd.data(),
        hdr.height, hdr.height, 1, // depth
        hdr.mipmaps,
        static_cast<u32>(PixelFormat::BC1_RGBA_UNORM), hdr.tile_spacing, hdr.block_height);

    return 0;
}
*/