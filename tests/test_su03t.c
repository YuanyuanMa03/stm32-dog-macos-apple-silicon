#define _POSIX_C_SOURCE 200809L

#include "../tools/su03t_xmodem.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#define FRAME_SIZE (3 + SU03T_BLOCK_SIZE + 2)

static void require(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        exit(1);
    }
}

static int read_exact(int fd, uint8_t *data, size_t length) {
    size_t offset = 0;
    while (offset < length) {
        ssize_t count = read(fd, data + offset, length - offset);
        if (count <= 0) return -1;
        offset += (size_t)count;
    }
    return 0;
}

static int check_frame(const uint8_t *frame, size_t block, const uint8_t *firmware,
                       size_t firmware_length) {
    size_t offset = block * SU03T_BLOCK_SIZE;
    size_t chunk = firmware_length - offset;
    if (chunk > SU03T_BLOCK_SIZE) chunk = SU03T_BLOCK_SIZE;
    uint8_t sequence = (uint8_t)(block + 1);
    if (frame[0] != 0x02 || frame[1] != sequence ||
        frame[2] != (uint8_t)~sequence) return -1;
    if (memcmp(frame + 3, firmware + offset, chunk) != 0) return -1;
    for (size_t i = chunk; i < SU03T_BLOCK_SIZE; ++i)
        if (frame[3 + i] != 0x1a) return -1;
    uint16_t crc = su03t_crc16(frame + 3, SU03T_BLOCK_SIZE);
    return frame[3 + SU03T_BLOCK_SIZE] == (uint8_t)(crc >> 8) &&
                   frame[4 + SU03T_BLOCK_SIZE] == (uint8_t)crc ? 0 : -1;
}

static void test_crc_and_no_handshake(void) {
    require(su03t_crc16((const uint8_t *)"123456789", 9) == 0x31c3,
            "CRC-16/XMODEM 标准向量");
    int pair[2];
    require(socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == 0, "socketpair");
    uint8_t zero = 0;
    require(write(pair[1], &zero, 1) == 1, "注入启动噪声");
    su03t_handshake_diagnostics diagnostics;
    require(su03t_wait_for_handshake(pair[0], 1, &diagnostics) == 1,
            "没有 CCC 必须超时");
    require(diagnostics.received == 1 && diagnostics.first_bytes[0] == 0,
            "超时诊断报告收到的字节");
    require(fcntl(pair[1], F_SETFL, O_NONBLOCK) == 0, "非阻塞检查");
    require(recv(pair[1], &zero, 1, 0) < 0 &&
            (errno == EAGAIN || errno == EWOULDBLOCK),
            "没有握手不得发送固件");
    close(pair[0]);
    close(pair[1]);
}

static void test_transfer(size_t firmware_length, int retry_first) {
    uint8_t *firmware = malloc(firmware_length);
    require(firmware != NULL, "分配测试固件");
    for (size_t i = 0; i < firmware_length; ++i)
        firmware[i] = (uint8_t)(i * 17 + 3);
    int pair[2];
    require(socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == 0, "socketpair");
    pid_t receiver = fork();
    require(receiver >= 0, "fork");
    if (receiver == 0) {
        close(pair[0]);
        if (write(pair[1], "CCC", 3) != 3) _exit(1);
        size_t blocks = (firmware_length + SU03T_BLOCK_SIZE - 1) / SU03T_BLOCK_SIZE;
        for (size_t block = 0; block < blocks; ++block) {
            uint8_t frame[FRAME_SIZE];
            if (read_exact(pair[1], frame, sizeof(frame)) != 0 ||
                check_frame(frame, block, firmware, firmware_length) != 0) _exit(2);
            if (retry_first && block == 0) {
                if (write(pair[1], "\x15", 1) != 1) _exit(3);
                uint8_t retried[FRAME_SIZE];
                if (read_exact(pair[1], retried, sizeof(retried)) != 0 ||
                    memcmp(frame, retried, sizeof(frame)) != 0) _exit(4);
            }
            if (write(pair[1], "\x06", 1) != 1) _exit(5);
        }
        uint8_t eot;
        if (read_exact(pair[1], &eot, 1) != 0 || eot != 0x04 ||
            write(pair[1], "\x06", 1) != 1) _exit(6);
        close(pair[1]);
        _exit(0);
    }
    close(pair[1]);
    su03t_handshake_diagnostics diagnostics;
    require(su03t_wait_for_handshake(pair[0], 1, &diagnostics) == 0,
            "识别三个 C");
    char error[128];
    require(su03t_send_xmodem(pair[0], firmware, firmware_length,
                              NULL, NULL, error, sizeof(error)) == 0,
            "模拟模块 ACK 后传输成功");
    close(pair[0]);
    int status;
    require(waitpid(receiver, &status, 0) == receiver &&
            WIFEXITED(status) && WEXITSTATUS(status) == 0,
            "接收端校验帧、CRC、补位、重试及 EOT");
    free(firmware);
}

int main(void) {
    alarm(15);
    test_crc_and_no_handshake();
    test_transfer(19, 1);
    test_transfer(SU03T_BLOCK_SIZE * 256 + 5, 0);
    puts("PASS: 无握手零写入、CRC/补位、NAK 重试、序号回卷、EOT ACK");
    return 0;
}
