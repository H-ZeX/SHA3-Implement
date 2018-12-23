//
// Created by hzx on 18-12-17.
//

#include "SHA3.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>


constexpr char NUM_TO_HEX[] = {'0', '1', '2', '3', '4', '5',
                               '6', '7', '8', '9', 'a', 'b',
                               'c', 'd', 'e', 'f'};

void test_1() {
    const char *fileName = "./test_7";
    const SHA3::SHA3Algorithm algo = SHA3::ALGO_512;
    const int msgSize = 512 / 8;

    clock_t nioBegin = clock();
    SHA3 sha3(algo);
    uint64_t stat[1600 / 64];
    sha3.processUsingNIO(fileName, stat, sizeof(stat));
    clock_t nioEnd = clock();

    {
        auto *k = (uint8_t *) stat;
        for (int i = 0; i < msgSize; i++) {
            printf("%c%c", NUM_TO_HEX[k[i] >> 4], NUM_TO_HEX[k[i] & 0xf]);
        }
        printf("\n");
    }

    SHA3 sha3_2(algo);
    struct stat buf{};
    if (::stat(fileName, &buf) == -1) {
        fprintf(stderr, "stat: %s\n", strerror(errno));
        exit(1);
    }
    printf("fileSize: %ld\n", buf.st_size);
    uint64_t stat_2[1600 / 64];
    clock_t normalBegin = clock();
    assert(sha3_2.processWithoutNIO(fileName, buf.st_size, stat_2, sizeof(stat_2)));
    clock_t normalEnd = clock();

    {
        auto k = (uint8_t *) stat_2;
        for (int i = 0; i < msgSize; i++) {
            printf("%c%c", NUM_TO_HEX[k[i] >> 4], NUM_TO_HEX[k[i] & 0xf]);
        }
        printf("\n");
    }

    printf("time:\nNIO:\t%lf\nNormal:\t%lf\n",
           (nioEnd - nioBegin) * 1.0 / CLOCKS_PER_SEC,
           (normalEnd - normalBegin) * 1.0 / CLOCKS_PER_SEC);

    // for (int i = 0; i < 1600 / 64; i++) {
    //     assert(stat[i] == stat_2[i]);
    // }
}

void test_2() {
    const uint64_t dataSize = 1024 * 1024 * 1024;
    const int resSize = 1600 / 8;
    const int bitrate = 576 / 8;
    const int msgSize = 512 / 8;
    auto *data = new uint8_t[dataSize];

    clock_t begin = clock();
    uint64_t result[resSize / 8] = {0};
    sha3ProcessBytes(data, dataSize, bitrate, result);
    clock_t end = clock();
    printf("1G data, use time: %lf\n", (end - begin) * 1.0 / CLOCKS_PER_SEC);
}

int main(int argc, char** argv) {
    test_2();
}
