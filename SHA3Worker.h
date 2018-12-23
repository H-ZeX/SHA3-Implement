//
// Created by hzx on 18-12-21.
//

#ifndef SHA3_WORKER_H
#define SHA3_WORKER_H

#include <cstdint>

void sha3ProcessBytes(const uint8_t data[], uint64_t dataSize, int bitrate, uint64_t result[]);
void doFinal(uint64_t stat[], int bitrate);
void doFinal(const uint8_t data[], uint64_t stat[], uint64_t bitLen, int bitrate);
void update(const uint8_t data[], uint64_t stat[], int bitrate);

#endif // SHA3_WORKER_H
