//
// Created by hzx on 18-12-9.
//

#ifndef NIO_READER_H
#define NIO_READER_H

#include <fcntl.h>
#include <string>
#include <stdio.h>
#include <cassert>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <thread>

#define ERR_MSG_BUF_SIZE 128

class NIOReader {
public:
    enum READER_STATUS {
        NOT_END,
        SUCCESS_END,
        FAIL_END,
        INTERRUPT_END
    };
    const uint64_t DATA_BUF_SIZE, READ_BUF_SIZE;

    char *dataBuf = nullptr;
    char errMsg[ERR_MSG_BUF_SIZE]{};
    volatile READER_STATUS isEndAndSuccess;
    const int fd;

private:
    // nextPosReadable is the next pos can read,
    // nextPosWritable is the next pos can write
    // if nextPosReadable==nextPosWritable, there is nothing
    // if nextPosReadable+1==nextPosWritable, it is full
    // only the consumer can change the nextPosReadable,
    // only the producer can change the nextPosWritable
    // if the producer read the old value of nextPosReadable,
    // just make the producer read less bytes than it may can
    // same as the nextPosWritable for consumer
    // this may can use volatile int but not atomic<int>
    volatile uint64_t nextPosReadable, nextPosWritable;
    // should make sure that this pointer not to reassigned
    // however, i can't use const, for that it need to init it
    // before others is init, and can't handle the error stat of opening file
    std::thread *noblockReader = nullptr;
    // use this bool var to transmit the interrupt signal to reader thread
    volatile bool willInterrupt = false;
    bool willCloseFd = false;

public:
    /**
     * user should check whether isEndAndSuccess==FAIL_END
     * after the constructor return
     */
    explicit NIOReader(int fd, uint64_t dataBufSize = 1024 * 1024, uint64_t readBufSize = 1024) :
            DATA_BUF_SIZE(dataBufSize), READ_BUF_SIZE(readBufSize), fd(fd) {
        if (this->fd < 0) {
            sprintf(errMsg, "fd=%d < 0", fd);
            this->isEndAndSuccess = FAIL_END;
        } else {
            this->dataBuf = new char[DATA_BUF_SIZE];
            if (this->dataBuf == nullptr) {
                strncpy(this->errMsg, "allocate dataBuf error", ERR_MSG_BUF_SIZE);
                this->isEndAndSuccess = FAIL_END;
            } else {
                this->errMsg[0] = 0;
                nextPosReadable = 0;
                nextPosWritable = 0;
                isEndAndSuccess = NOT_END;
                noblockReader = new std::thread(&NIOReader::readFile, this);
            }
        }
    }

    explicit NIOReader(const char *const fileName, uint64_t dataBufSize = 256 * 1024 * 1024,
                       uint64_t readBufSize = 1024) :
            DATA_BUF_SIZE(dataBufSize), READ_BUF_SIZE(readBufSize), fd(open(fileName, O_RDONLY)) {
        if (this->fd <= 0) {
            strcpy(errMsg, "Open File Error: ");
            strncpy(errMsg + sizeof("Open File Error: ") - 1,
                    strerror_r(errno, this->errMsg, ERR_MSG_BUF_SIZE),
                    ERR_MSG_BUF_SIZE);
            this->isEndAndSuccess = FAIL_END;
        } else {
            this->dataBuf = new char[DATA_BUF_SIZE];
            if (this->dataBuf == nullptr) {
                strncpy(this->errMsg, "allocate dataBuf error", ERR_MSG_BUF_SIZE);
                this->isEndAndSuccess = FAIL_END;
            } else {
                this->errMsg[0] = 0;
                nextPosReadable = 0;
                nextPosWritable = 0;
                isEndAndSuccess = NOT_END;
                this->willCloseFd = true;
                noblockReader = new std::thread(&NIOReader::readFile, this);
            }
        }
    }

    ~NIOReader() {
        willInterrupt = true;
        if (noblockReader != nullptr) {
            noblockReader->join();
        }
        if (this->willCloseFd) {
            close(this->fd);
        }
        delete[] dataBuf;
        delete noblockReader;
    }

    /**
     * @return return whether the read process ends and all data is read
     */
    bool isEnd() {
        return (this->isEndAndSuccess == SUCCESS_END &&
                this->nextPosWritable == this->nextPosReadable) ||
               (this->isEndAndSuccess != SUCCESS_END &&
                this->isEndAndSuccess != NOT_END);
    }

    /**
     * @return whether the readableByteCnt can increase
     */
    bool isNoMore() {
        return isEndAndSuccess != NOT_END;
    }

    uint64_t readableByteCnt() {
        int64_t t = this->nextPosWritable - this->nextPosReadable;
        if (t < 0) {
            return DATA_BUF_SIZE - this->nextPosReadable;
        } else {
            return (uint64_t) t;
        }
    }

    const char *readablePos() {
        return this->dataBuf + this->nextPosReadable;
    }

    // if the user use illegal cnt,
    // then the stat of this NIOReader instance will be illegal
    void recordReadCnt(uint64_t cnt) {
        this->nextPosReadable = (this->nextPosReadable + cnt) % DATA_BUF_SIZE;
    }

private:
    uint64_t writeableByteCnt() {
        uint64_t w = nextPosWritable, r = nextPosReadable;
        // if the next's next pos to write is the pos haven't read
        // stop write
        if ((w + 1) % DATA_BUF_SIZE == r) {
            return 0;
        }
        uint64_t t;
        if (w >= r) {
            t = DATA_BUF_SIZE - w - (r == 0);
        } else {
            t = r - w - 1;
        }
        return t > READ_BUF_SIZE ? READ_BUF_SIZE : t;
    }

    void readFile() {
        int64_t rt;
        uint64_t canRead = writeableByteCnt();
        uint64_t total = 0;
        const char *const begin = dataBuf;
        const char *const end = dataBuf + DATA_BUF_SIZE;
        while ((rt = read(this->fd, dataBuf + nextPosWritable, canRead)) >= 0) {
            // for array out of index check
            /*
            if (!(dataBuf + nextPosWritable >= begin && dataBuf + nextPosWritable < end)) {
                printf("reader begin: %p %p %p\n", dataBuf + nextPosWritable, begin, end);
                exit(1);
            }
            if (!(dataBuf + nextPosWritable + canRead >= begin && dataBuf + nextPosWritable + canRead <= end)) {
                printf("reader +canRead: %p %p %p\n", dataBuf + nextPosWritable + canRead, begin, end);
                exit(1);
            }
            */
            if ((canRead > 0 && rt == 0) || rt < 0) {
                break;
            }
            total += rt;
            nextPosWritable = ((nextPosWritable + rt) % DATA_BUF_SIZE);
            canRead = writeableByteCnt();
            // for speed profile
            /*
            if (canRead == 0) {
                printf("consumer not powerful\n");
            }
            */
            if (willInterrupt) {
                isEndAndSuccess = INTERRUPT_END;
                return;
            }
        }
        if (rt < 0) {
            strcpy(errMsg, "Open File Error: ");
            strncpy(errMsg + sizeof("Open File Error: ") - 1,
                    strerror_r(errno, errMsg, ERR_MSG_BUF_SIZE),
                    ERR_MSG_BUF_SIZE);
            isEndAndSuccess = FAIL_END;
        } else {
            isEndAndSuccess = SUCCESS_END;
        }
    }
};

#endif // NIO_READER_H
