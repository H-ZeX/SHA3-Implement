//
// Created by hzx on 18-12-21.
//

#ifndef SHA3_SHA3_H
#define SHA3_SHA3_H


#include <cstdlib>
#include "SHA3Worker.h"
#include "NIOReader.h"
#include <cstdint>

class SHA3 {
private:
    // the read function's buf, in byte
    const uint64_t READ_BUF_SIZE = 2996352;
    // the data buf size in byte
    // for that the pad function use the dataBuf to construct pad block
    // so if it need roll back(this is ring buf), the perform may be low
    // so use the BITRATE's common multiple (1152/8*1088/8*823/8)
    const uint64_t DATA_BUF_SIZE = (1152 / 8 * 1088 / 8 * 832 / 8 * 576 / 8) * 10;
    const uint64_t MSG_SIZE;
    const uint64_t BITRATE;
    // consider that DATA_BUF_SIZE%WORK_SIZE_ONE_TIME must == 0 
    // and WORK_SIZE_ONE_TIME%BITRATE must == 0
    // and that this param should not be to big or to small, or the speed will slow
    // u can try to change this param to speed up
    const uint64_t WORK_SIZE_ONE_TIME = READ_BUF_SIZE;

public:
    enum SHA3Algorithm {
        ALGO_224,
        ALGO_256,
        ALGO_384,
        ALGO_512
    };

private:
    uint64_t initBitrate(SHA3Algorithm algorithm) {
        switch (algorithm) {
            case ALGO_224:
                return 1152 / 8;
            case ALGO_256:
                return 1088 / 8;
            case ALGO_384:
                return 832 / 8;
            case ALGO_512:
                return 576 / 8;
            default:
                return 1152 / 8;
        }
    }

    uint64_t initMsgSize(SHA3Algorithm algorithm) {
        switch (algorithm) {
            case ALGO_224:
                return 224 / 8;
            case ALGO_256:
                return 256 / 8;
            case ALGO_384:
                return 384 / 8;
            case ALGO_512:
                return 512 / 8;
            default:
                return 224 / 8;
        }
    }

public:
    explicit SHA3(SHA3Algorithm algorithm)
            : MSG_SIZE(initMsgSize(algorithm)),
              BITRATE(initBitrate(algorithm)) {
    }

    /**
     * @param resultSize the result array size in bytes should >= 200
     * @return whether the process is success
     */
    bool processUsingNIO(const char *fileName, uint64_t *result, uint32_t resultSize) {
        if (resultSize < 1600 / 8) {
            return false;
        }
        NIOReader reader(fileName, DATA_BUF_SIZE, READ_BUF_SIZE);
        if (reader.isEndAndSuccess == NIOReader::FAIL_END) {
            fprintf(stderr, "%s\n", reader.errMsg);
            return false;
        }
        memset(result, 0, 1600 / 8);
        bool hadDoFinal = false;
        while (!reader.isEnd()) {
            uint64_t toWorkSize = reader.readableByteCnt();
            const char *begin = reader.readablePos();
            assert((begin - reader.dataBuf) % WORK_SIZE_ONE_TIME == 0);
            if (!reader.isNoMore() && toWorkSize < WORK_SIZE_ONE_TIME) {
            } else if (toWorkSize >= WORK_SIZE_ONE_TIME) {
                for (int i = 0; i < WORK_SIZE_ONE_TIME; i += BITRATE) {
                    update(((const uint8_t *) begin) + i, result, BITRATE);
                    reader.recordReadCnt((uint32_t) BITRATE);
                }
            } else {
                if (toWorkSize == 0) {
                    doFinal(result, BITRATE);
                } else {
                    uint64_t n = toWorkSize / BITRATE;
                    uint64_t x = toWorkSize % BITRATE;
                    for (uint64_t i = 0; i < n; i++) {
                        update((const uint8_t *) begin + i * BITRATE, result, BITRATE);
                    }
                    doFinal((const uint8_t *) begin + n * BITRATE, result, x * 8, BITRATE);
                }
                hadDoFinal = true;
                reader.recordReadCnt(toWorkSize);
            }
        }
        if (!hadDoFinal) {
            doFinal(result, BITRATE);
        }
        return true;
    }

    /**
     * @param resultSize the result array size in bytes should >= 200
     * @return whether the process is success
     */
    bool processWithoutNIO(const char *fileName, uint64_t fileSize,
                           uint64_t result[], uint64_t resultSize) {
        if (resultSize < 1600 / 8) {
            return false;
        }
        memset(result, 0, resultSize);
        int fd = open(fileName, O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "open error: %s\n", strerror(errno));
            exit(1);
        }
        auto *buf = new uint8_t[fileSize];
        uint64_t resetSize = fileSize;
        uint8_t *ptr = buf;
        long re = 0;
        while ((re = read(fd, ptr, resetSize)) > 0) {
            ptr += re;
            resetSize -= re;
        }
        assert(re != 0 || re == 0 && resetSize == 0);
        if (re < 0) {
            fprintf(stderr, "readError: %s\n", strerror(errno));
            exit(1);
        }
        uint64_t i = 0;
        uint64_t end = fileSize - BITRATE;
        for (i = 0; i < end; i += BITRATE) {
            update(buf + i, result, BITRATE);
        }
        if (i == end) {
            doFinal(result, BITRATE);
        } else {
            doFinal(buf + i, result, (fileSize - i) * 8, BITRATE);
        }
        return true;
    }
};


#endif //SHA3_SHA3_H
