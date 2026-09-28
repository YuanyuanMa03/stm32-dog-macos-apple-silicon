#ifndef SU03T_XMODEM_H
#define SU03T_XMODEM_H

#include <stddef.h>
#include <stdint.h>

#define SU03T_BLOCK_SIZE 1024

typedef struct {
    size_t received;
    uint8_t first_bytes[32];
    size_t first_count;
} su03t_handshake_diagnostics;

typedef void (*su03t_progress_fn)(size_t acknowledged, size_t total, void *context);

uint16_t su03t_crc16(const uint8_t *data, size_t length);

/* Returns 0 only after three consecutive 'C' bytes; never transmits. */
int su03t_wait_for_handshake(int fd, int timeout_seconds,
                             su03t_handshake_diagnostics *diagnostics);

/* Returns 0 only after every block and EOT receive ACK. */
int su03t_send_xmodem(int fd, const uint8_t *firmware, size_t length,
                      su03t_progress_fn progress, void *context,
                      char *error, size_t error_capacity);

#endif
