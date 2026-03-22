#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <mtd/mtd-user.h>

#define ENV_SIZE 0x10000

static uint32_t crc32_le(uint32_t crc, unsigned char const *p, size_t len)
{
    int i;
    while (len--) {
        crc ^= *p++;
        for (i = 0; i < 8; i++)
            crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320 : 0);
    }
    return crc;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        printf("Usage: %s <mtd_dev> <mac>\n", argv[0]);
        return 1;
    }

    int fd = open(argv[1], O_RDWR);
    if (fd < 0) {
        perror("open mtd");
        return 1;
    }

    uint8_t *env = malloc(ENV_SIZE);
    if (!env) {
        perror("malloc");
        return 1;
    }

    if (read(fd, env, ENV_SIZE) != ENV_SIZE) {
        perror("read mtd");
        return 1;
    }

    uint8_t *data = env + 4;
    int data_len = ENV_SIZE - 4;

    // Check CRC
    uint32_t old_crc = *(uint32_t*)env;
    uint32_t calc_crc = crc32_le(0, data, data_len);
    if (old_crc != calc_crc) {
        printf("Warning: Old CRC mismatch (0x%08x != 0x%08x)\n", old_crc, calc_crc);
    }

    // Find ethaddr or end of env
    int i = 0;
    int ethaddr_idx = -1;
    int end_idx = 0;
    while (i < data_len && data[i] != 0) {
        int len = strlen((char *)&data[i]);
        if (strncmp((char *)&data[i], "ethaddr=", 8) == 0) {
            ethaddr_idx = i;
        }
        i += len + 1;
    }
    end_idx = i;

    char new_ethaddr[64];
    snprintf(new_ethaddr, sizeof(new_ethaddr), "ethaddr=%s", argv[2]);
    int new_len = strlen(new_ethaddr) + 1;

    if (ethaddr_idx != -1) {
        // Remove old ethaddr
        int old_len = strlen((char *)&data[ethaddr_idx]) + 1;
        memmove(&data[ethaddr_idx], &data[ethaddr_idx + old_len], end_idx - (ethaddr_idx + old_len) + 1); // +1 to copy the extra null terminator
        end_idx -= old_len;
    }

    // Append new ethaddr
    if (end_idx + new_len + 1 > data_len) {
        printf("Environment too large!\n");
        return 1;
    }
    memcpy(&data[end_idx], new_ethaddr, new_len);
    end_idx += new_len;
    data[end_idx] = 0; // Double null termination

    // Calculate CRC
    uint32_t crc = crc32_le(0, data, data_len);
    *(uint32_t*)env = crc;

    // Erase
    erase_info_t ei;
    ei.start = 0;
    ei.length = ENV_SIZE;
    if (ioctl(fd, MEMERASE, &ei) < 0) {
        perror("ioctl MEMERASE");
        return 1;
    }

    // Rewind and Write
    lseek(fd, 0, SEEK_SET);
    if (write(fd, env, ENV_SIZE) != ENV_SIZE) {
        perror("write mtd");
        return 1;
    }

    close(fd);
    free(env);
    printf("Successfully updated MAC to %s\n", argv[2]);
    return 0;
}