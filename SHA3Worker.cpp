//
// Created by hzx on 18-12-20.
//

#include <cstdint>
#include <inttypes.h>
#include <assert.h>
#include <cstdlib>

static constexpr uint64_t ROUND_CONST[] = {
        0x0000000000000001, 0x0000000000008082, 0x800000000000808A,
        0x8000000080008000, 0x000000000000808B, 0x0000000080000001,
        0x8000000080008081, 0x8000000000008009, 0x000000000000008A,
        0x0000000000000088, 0x0000000080008009, 0x000000008000000A,
        0x000000008000808B, 0x800000000000008B, 0x8000000000008089,
        0x8000000000008003, 0x8000000000008002, 0x8000000000000080,
        0x000000000000800A, 0x800000008000000A, 0x8000000080008081,
        0x8000000000008080, 0x0000000080000001, 0x8000000080008008
};

template<int offset>
static uint64_t rotl(uint64_t x) {
    return x << offset | (x >> (sizeof(uint64_t) * 8 - offset));
}

static void permute(uint64_t p[5][5]) {
    for (int i = 0; i < 24; i++) {
        uint64_t c[5], c2[5];
        for (int j = 0; j < 5; j++) {
            c[j] = p[0][j] ^ p[1][j] ^ p[2][j] ^ p[3][j] ^ p[4][j];
        }
        // consider that in c++ -1%5== -1 !!!
        for (int j = 0; j < 5; j++) {
            c2[j] = rotl<1>(c[(j + 1) % 5]) ^ c[(j - 1 + 5) % 5];
        }

        // these loop unrolling method learn from botan
        // https://github.com/randombit/botan/blob/c4d3b64d2ebaa70c737c359a941301783036ca68/src/lib/hash/sha3/sha3.cpp
        const uint64_t B00 = p[0][0] ^c2[0];
        const uint64_t B20 = rotl<1>(p[0][1] ^ c2[1]);
        const uint64_t B40 = rotl<62>(p[0][2] ^ c2[2]);
        const uint64_t B10 = rotl<28>(p[0][3] ^ c2[3]);
        const uint64_t B30 = rotl<27>(p[0][4] ^ c2[4]);
        const uint64_t B31 = rotl<36>(p[1][0] ^ c2[0]);
        const uint64_t B01 = rotl<44>(p[1][1] ^ c2[1]);
        const uint64_t B21 = rotl<6>(p[1][2] ^ c2[2]);
        const uint64_t B41 = rotl<55>(p[1][3] ^ c2[3]);
        const uint64_t B11 = rotl<20>(p[1][4] ^ c2[4]);
        const uint64_t B12 = rotl<3>(p[2][0] ^ c2[0]);
        const uint64_t B32 = rotl<10>(p[2][1] ^ c2[1]);
        const uint64_t B02 = rotl<43>(p[2][2] ^ c2[2]);
        const uint64_t B22 = rotl<25>(p[2][3] ^ c2[3]);
        const uint64_t B42 = rotl<39>(p[2][4] ^ c2[4]);
        const uint64_t B43 = rotl<41>(p[3][0] ^ c2[0]);
        const uint64_t B13 = rotl<45>(p[3][1] ^ c2[1]);
        const uint64_t B33 = rotl<15>(p[3][2] ^ c2[2]);
        const uint64_t B03 = rotl<21>(p[3][3] ^ c2[3]);
        const uint64_t B23 = rotl<8>(p[3][4] ^ c2[4]);
        const uint64_t B24 = rotl<18>(p[4][0] ^ c2[0]);
        const uint64_t B44 = rotl<2>(p[4][1] ^ c2[1]);
        const uint64_t B14 = rotl<61>(p[4][2] ^ c2[2]);
        const uint64_t B34 = rotl<56>(p[4][3] ^ c2[3]);
        const uint64_t B04 = rotl<14>(p[4][4] ^ c2[4]);

        p[0][0] = (~B01 & B02) ^ B00;
        p[0][1] = (~B02 & B03) ^ B01;
        p[0][2] = (~B03 & B04) ^ B02;
        p[0][3] = (~B04 & B00) ^ B03;
        p[0][4] = (~B00 & B01) ^ B04;
        p[1][0] = (~B11 & B12) ^ B10;
        p[1][1] = (~B12 & B13) ^ B11;
        p[1][2] = (~B13 & B14) ^ B12;
        p[1][3] = (~B14 & B10) ^ B13;
        p[1][4] = (~B10 & B11) ^ B14;
        p[2][0] = (~B21 & B22) ^ B20;
        p[2][1] = (~B22 & B23) ^ B21;
        p[2][2] = (~B23 & B24) ^ B22;
        p[2][3] = (~B24 & B20) ^ B23;
        p[2][4] = (~B20 & B21) ^ B24;
        p[3][0] = (~B31 & B32) ^ B30;
        p[3][1] = (~B32 & B33) ^ B31;
        p[3][2] = (~B33 & B34) ^ B32;
        p[3][3] = (~B34 & B30) ^ B33;
        p[3][4] = (~B30 & B31) ^ B34;
        p[4][0] = (~B41 & B42) ^ B40;
        p[4][1] = (~B42 & B43) ^ B41;
        p[4][2] = (~B43 & B44) ^ B42;
        p[4][3] = (~B44 & B40) ^ B43;
        p[4][4] = (~B40 & B41) ^ B44;

        p[0][0] ^= ROUND_CONST[i];
    }
}

void update(const uint8_t data[], uint64_t stat[], int bitrate) {
    auto *p = (uint64_t *) data;
    for (int i = 0; i < bitrate / sizeof(uint64_t); i++) {
        stat[i] ^= p[i];
    }
    auto p_2 = (uint64_t (*)[5]) stat;
    permute(p_2);
}

void doFinal(const uint8_t data[], uint64_t stat[], uint64_t bitLen, int bitrate) {
    assert(bitLen < bitrate * 8);
    uint64_t t1 = bitLen / 64;
    uint64_t t2 = bitLen % 64;
    uint64_t t3 = bitrate * 8 - bitLen;
    auto *p = (const uint64_t *) data;
    for (uint64_t i = 0; i < t1; i++) {
        stat[i] ^= p[i];
    }
    if (t3 == 1) {
        stat[t1] ^= p[t1] & ((1ULL << 63) - 1);
        permute((uint64_t (*)[5]) stat);
        stat[0] ^= 0x3;
    } else if (t3 == 2) {
        stat[t1] ^= (p[t1] & ((1ULL << 62) - 1)) | (1ULL << 63);
        permute((uint64_t (*)[5]) stat);
        stat[0] ^= 1;
    } else {
        stat[t1] ^= (p[t1] & ((1ULL << t2) - 1)) | (0x6ULL << t2);
    }
    stat[bitrate / sizeof(uint64_t) - 1] ^= 1ULL << 63;
    permute((uint64_t (*)[5]) stat);
}

void doFinal(uint64_t stat[], int bitrate) {
    stat[0] ^= 0x6;
    stat[bitrate / sizeof(uint64_t) - 1] ^= 1UL << 63;
    permute((uint64_t (*)[5]) stat);
}

void sha3ProcessBytes(const uint8_t data[], uint64_t dataSize, int bitrate, uint64_t result[]) {
    uint64_t end = dataSize - bitrate;
    for (uint64_t i = 0; i <= end; i += bitrate) {
        update((data + i), result, bitrate);
    }
    uint64_t s = dataSize / bitrate;
    uint64_t t = dataSize % bitrate;
    if (t == 0) {
        doFinal(result, bitrate);
    } else {
        doFinal(data + s * bitrate, result, t * 8, bitrate);
    }
}
