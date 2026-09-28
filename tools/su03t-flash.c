#define _DARWIN_C_SOURCE

#include "su03t_xmodem.h"

#include <CommonCrypto/CommonDigest.h>
#include <IOKit/serial/ioss.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

#define SU03T_BAUD 921600
#define MAX_FIRMWARE_SIZE (16 * 1024 * 1024)

static void usage(const char *program) {
    fprintf(stderr,
            "用法：%s --port /dev/cu.wchusbserialXXXX --firmware xxx_update.bin "
            "[--sha256 64位哈希] [--wait-seconds 180]\n",
            program);
}

static int has_update_suffix(const char *path) {
    const char *name = strrchr(path, '/');
    name = name ? name + 1 : path;
    const char *suffix = "_update.bin";
    size_t name_length = strlen(name), suffix_length = strlen(suffix);
    return name_length > suffix_length &&
           strcmp(name + name_length - suffix_length, suffix) == 0;
}

static uint8_t *load_firmware(const char *path, size_t *size) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        perror("打开固件失败");
        return NULL;
    }
    struct stat info;
    if (fstat(fileno(file), &info) != 0 || info.st_size <= 0 ||
        info.st_size > MAX_FIRMWARE_SIZE) {
        fprintf(stderr, "固件大小无效（允许 1..%d 字节）\n", MAX_FIRMWARE_SIZE);
        fclose(file);
        return NULL;
    }
    *size = (size_t)info.st_size;
    uint8_t *contents = malloc(*size);
    if (!contents || fread(contents, 1, *size, file) != *size) {
        fprintf(stderr, "读取固件失败\n");
        free(contents);
        fclose(file);
        return NULL;
    }
    fclose(file);
    return contents;
}

static void sha256_hex(const uint8_t *data, size_t size, char output[65]) {
    uint8_t digest[CC_SHA256_DIGEST_LENGTH];
    CC_SHA256(data, (CC_LONG)size, digest);
    for (size_t i = 0; i < sizeof(digest); ++i)
        snprintf(output + 2 * i, 3, "%02x", digest[i]);
    output[64] = '\0';
}

static int open_serial(const char *path) {
    int fd = open(path, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) return -1;
    struct termios settings;
    if (tcgetattr(fd, &settings) != 0) goto failed;
    cfmakeraw(&settings);
    settings.c_cflag |= CLOCAL | CREAD;
    settings.c_cflag &= ~(PARENB | CSTOPB | CRTSCTS | CSIZE);
    settings.c_cflag |= CS8;
    settings.c_cc[VMIN] = 1;
    settings.c_cc[VTIME] = 0;
    if (cfsetspeed(&settings, B115200) != 0 ||
        tcsetattr(fd, TCSANOW, &settings) != 0) goto failed;
    speed_t baud = SU03T_BAUD;
    if (ioctl(fd, IOSSIOSPEED, &baud) != 0) goto failed;
    int flags = fcntl(fd, F_GETFL);
    if (flags < 0 || fcntl(fd, F_SETFL, flags & ~O_NONBLOCK) != 0) goto failed;
    if (tcflush(fd, TCIFLUSH) != 0) goto failed;
    return fd;
failed:
    { int saved_errno = errno; close(fd); errno = saved_errno; }
    return -1;
}

static void show_progress(size_t acknowledged, size_t total, void *context) {
    (void)context;
    if (acknowledged % 100 == 0 || acknowledged == total) {
        printf("已确认 %zu/%zu 块\n", acknowledged, total);
        fflush(stdout);
    }
}

int main(int argc, char **argv) {
    const char *port = NULL, *firmware_path = NULL, *expected_hash = NULL;
    int wait_seconds = 180;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        }
        if (i + 1 >= argc) { usage(argv[0]); return 2; }
        if (strcmp(argv[i], "--port") == 0) port = argv[++i];
        else if (strcmp(argv[i], "--firmware") == 0) firmware_path = argv[++i];
        else if (strcmp(argv[i], "--sha256") == 0) expected_hash = argv[++i];
        else if (strcmp(argv[i], "--wait-seconds") == 0) {
            char *end;
            long value = strtol(argv[++i], &end, 10);
            if (*end != '\0' || value < 1 || value > 3600) {
                fprintf(stderr, "--wait-seconds 必须是 1..3600\n");
                return 2;
            }
            wait_seconds = (int)value;
        } else { usage(argv[0]); return 2; }
    }
    if (!port || !firmware_path || !has_update_suffix(firmware_path)) {
        fprintf(stderr, "必须指定串口及 *_update.bin 固件\n");
        usage(argv[0]);
        return 2;
    }
    size_t length = 0;
    uint8_t *firmware = load_firmware(firmware_path, &length);
    if (!firmware) return 2;
    char actual_hash[65];
    sha256_hex(firmware, length, actual_hash);
    printf("固件：%s（%zu 字节，SHA-256 %s）\n", firmware_path, length, actual_hash);
    if (expected_hash && (strlen(expected_hash) != 64 ||
                          strcasecmp(expected_hash, actual_hash) != 0)) {
        fprintf(stderr, "SHA-256 不匹配，未打开串口\n");
        free(firmware);
        return 2;
    }
    int fd = open_serial(port);
    if (fd < 0) {
        perror("打开 921600 8N1 串口失败");
        free(firmware);
        return 1;
    }
    printf("READY：%s 已打开，921600 8N1；现在给 SU-03T1 接通 5V。\n", port);
    fflush(stdout);
    su03t_handshake_diagnostics diagnostics;
    int handshake = su03t_wait_for_handshake(fd, wait_seconds, &diagnostics);
    if (handshake != 0) {
        fprintf(stderr, "未收到 CCC；收到 %zu 字节，前 %zu 字节：",
                diagnostics.received, diagnostics.first_count);
        for (size_t i = 0; i < diagnostics.first_count; ++i)
            fprintf(stderr, "%02x%s", diagnostics.first_bytes[i],
                    i + 1 == diagnostics.first_count ? "" : " ");
        fprintf(stderr, "；未发送固件数据%s\n", handshake < 0 ? "（串口读取错误）" : "");
        close(fd);
        free(firmware);
        return 1;
    }
    printf("收到 CCC，开始传输；此时不要断电。\n");
    fflush(stdout);
    char error[128];
    int result = su03t_send_xmodem(fd, firmware, length, show_progress,
                                   NULL, error, sizeof(error));
    close(fd);
    free(firmware);
    if (result != 0) {
        fprintf(stderr, "升级未确认成功：%s\n", error);
        return 1;
    }
    printf("EOT 已得到 ACK；传输完成，请检查模块重启和语音响应。\n");
    return 0;
}
