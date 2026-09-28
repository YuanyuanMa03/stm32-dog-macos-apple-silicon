#define _POSIX_C_SOURCE 200809L

#include "su03t_xmodem.h"

#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define STX 0x02
#define EOT 0x04
#define ACK 0x06
#define NAK 0x15
#define CAN 0x18
#define FRAME_SIZE (3 + SU03T_BLOCK_SIZE + 2)
#define MAX_RETRIES 16

static int64_t now_ms(void) {
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return (int64_t)time.tv_sec * 1000 + time.tv_nsec / 1000000;
}

/* 1 byte, 0 on timeout, -1 on I/O failure. */
static int read_byte_until(int fd, int64_t deadline, uint8_t *value) {
    for (;;) {
        int64_t remaining = deadline - now_ms();
        if (remaining <= 0) return 0;
        struct pollfd event = {.fd = fd, .events = POLLIN};
        int ready = poll(&event, 1, remaining > 1000 ? 1000 : (int)remaining);
        if (ready == 0 || (ready < 0 && errno == EINTR)) continue;
        if (ready < 0) return -1;
        if (!(event.revents & POLLIN)) {
            if (event.revents & (POLLERR | POLLHUP | POLLNVAL)) return -1;
            continue;
        }
        ssize_t count = read(fd, value, 1);
        if (count == 1) return 1;
        if (count == 0) return -1;
        if (errno != EINTR && errno != EAGAIN) return -1;
    }
}

static int write_all(int fd, const uint8_t *data, size_t length) {
    int64_t deadline = now_ms() + 10000;
    size_t offset = 0;
    while (offset < length) {
        int64_t remaining = deadline - now_ms();
        if (remaining <= 0) return -1;
        struct pollfd event = {.fd = fd, .events = POLLOUT};
        int ready = poll(&event, 1, remaining > 1000 ? 1000 : (int)remaining);
        if (ready == 0 || (ready < 0 && errno == EINTR)) continue;
        if (ready < 0 || (event.revents & (POLLERR | POLLHUP | POLLNVAL))) return -1;
        if (!(event.revents & POLLOUT)) continue;
        ssize_t count = write(fd, data + offset, length - offset);
        if (count > 0) offset += (size_t)count;
        else if (count == 0 || (errno != EINTR && errno != EAGAIN)) return -1;
    }
    /* A serial write can be queued by the OS before it reaches the wire. */
    if (isatty(fd) && tcdrain(fd) != 0) return -1;
    return 0;
}

uint16_t su03t_crc16(const uint8_t *data, size_t length) {
    uint16_t crc = 0;
    for (size_t i = 0; i < length; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (int bit = 0; bit < 8; ++bit)
            crc = (uint16_t)((crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1);
    }
    return crc;
}

int su03t_wait_for_handshake(int fd, int timeout_seconds,
                             su03t_handshake_diagnostics *diagnostics) {
    if (fd < 0 || timeout_seconds <= 0 || diagnostics == NULL) return -1;
    memset(diagnostics, 0, sizeof(*diagnostics));
    int64_t deadline = now_ms() + (int64_t)timeout_seconds * 1000;
    int consecutive = 0;
    uint8_t byte;
    for (;;) {
        int result = read_byte_until(fd, deadline, &byte);
        if (result <= 0) return result == 0 ? 1 : -1;
        ++diagnostics->received;
        if (diagnostics->first_count < sizeof(diagnostics->first_bytes))
            diagnostics->first_bytes[diagnostics->first_count++] = byte;
        consecutive = byte == 'C' ? consecutive + 1 : 0;
        if (consecutive == 3) return 0;
    }
}

static void make_frame(uint8_t frame[FRAME_SIZE], uint8_t sequence,
                       const uint8_t *data, size_t length) {
    frame[0] = STX;
    frame[1] = sequence;
    frame[2] = (uint8_t)~sequence;
    memset(frame + 3, 0x1a, SU03T_BLOCK_SIZE);
    memcpy(frame + 3, data, length);
    uint16_t crc = su03t_crc16(frame + 3, SU03T_BLOCK_SIZE);
    frame[3 + SU03T_BLOCK_SIZE] = (uint8_t)(crc >> 8);
    frame[4 + SU03T_BLOCK_SIZE] = (uint8_t)crc;
}

static int wait_for_reply(int fd) {
    int64_t deadline = now_ms() + 5000;
    uint8_t byte;
    for (;;) {
        int result = read_byte_until(fd, deadline, &byte);
        if (result <= 0) return result == 0 ? 0 : -1;
        if (byte == ACK || byte == NAK || byte == CAN) return byte;
        /* The receiver may still have queued startup 'C' bytes. */
    }
}

int su03t_send_xmodem(int fd, const uint8_t *firmware, size_t length,
                      su03t_progress_fn progress, void *context,
                      char *error, size_t error_capacity) {
    if (fd < 0 || firmware == NULL || length == 0 || error == NULL || error_capacity == 0)
        return -1;
    error[0] = '\0';
    size_t total = (length + SU03T_BLOCK_SIZE - 1) / SU03T_BLOCK_SIZE;
    for (size_t block = 0; block < total; ++block) {
        size_t offset = block * SU03T_BLOCK_SIZE;
        size_t chunk = length - offset;
        if (chunk > SU03T_BLOCK_SIZE) chunk = SU03T_BLOCK_SIZE;
        uint8_t frame[FRAME_SIZE];
        make_frame(frame, (uint8_t)(block + 1), firmware + offset, chunk);
        int reply = 0;
        for (int attempt = 0; attempt <= MAX_RETRIES; ++attempt) {
            if (write_all(fd, frame, sizeof(frame)) != 0) {
                snprintf(error, error_capacity, "第 %zu 块发送失败", block + 1);
                return -1;
            }
            reply = wait_for_reply(fd);
            if (reply == ACK) break;
            if (reply == CAN || reply == -1) break;
        }
        if (reply != ACK) {
            snprintf(error, error_capacity, "第 %zu/%zu 块未得到 ACK", block + 1, total);
            return -1;
        }
        if (progress) progress(block + 1, total, context);
    }
    const uint8_t eot = EOT;
    for (int attempt = 0; attempt <= MAX_RETRIES; ++attempt) {
        if (write_all(fd, &eot, 1) != 0) {
            snprintf(error, error_capacity, "EOT 发送失败");
            return -1;
        }
        int reply = wait_for_reply(fd);
        if (reply == ACK) return 0;
        if (reply == CAN || reply == -1) break;
    }
    snprintf(error, error_capacity, "EOT 未得到 ACK，不能认定升级完成");
    return -1;
}
