#include "Util.h"
#include "lz4.h"

struct YKCMP_HDR {
    char magic[8];
    u32 compType;
    u32 compSize;
    u32 decompSize;
};

extern "C" __declspec(dllexport)
bool decompress(u8* fd, u32 in_size, u8* out, u32 out_size) {
    YKCMP_HDR hdr{};
    memcpy(&hdr, fd, sizeof(YKCMP_HDR));

    if (hdr.decompSize != out_size) return false;

    //printf("compsize %08X -- decompsize %08X", hdr.compSize, hdr.decompSize);

    switch (hdr.compType) {
        case 4: // custom
        {
            size_t inPos = 0x14, outPos = 0;

            while (inPos < in_size) {
                uint8_t control = fd[inPos++];

                //printf("%08X control 0x%02X\n", inPos - 1, control);

                if (control < 0x80) {
                    memcpy(&out[outPos], &fd[inPos], control);
                    outPos += control; inPos += control;
                } else {
                    uint32_t offset = 0, size = 0;

                    if (control < 0xC0) {
                        offset = (control & 0xF) + 1;
                        size = (control >> 4) - 8 + 1;
                    } else if (control < 0xE0) {
                        offset = fd[inPos++] + 1;
                        size = control - 0xC0 + 2;
                    } else {
                        uint8_t temp = fd[inPos++];
                        uint8_t temp2 = fd[inPos++];
                        size = (control << 4) + (temp >> 4) - 0xE00 + 3;
                        offset = ((temp & 0xF) << 8) + temp2 + 1;
                    }

                    //printf("size 0x%08X offset 0x%08X output is currently %08X\n\n", size, offset, outPos);
                    memcpy(&out[outPos], &out[outPos - offset], size);
                    outPos += size;
                }
            }
        }
        break;

        case 8:
        case 9:
            LZ4_decompress_safe((char*)fd + sizeof(YKCMP_HDR), (char*)out, hdr.compSize, hdr.decompSize);
            break;

        default:
            printf("Invalid compression type %X", hdr.compType);
            return false;
    }

    return true;
}
