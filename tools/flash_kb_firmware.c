#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDManager.h>
#include <IOKit/IOKitLib.h>

static volatile int g_got_reply = 0;
static uint8_t g_reply_buf[64];
static CFIndex g_reply_len = 0;

static uint16_t crc16_outer(const uint8_t *data, size_t len) {
    uint16_t crc = 0x2A6E; // 10862 from a/a.java
    for (size_t i = 0; i < len; i++) {
        crc ^= ((uint16_t)data[i]) << 8;
        for (int b = 0; b < 8; b++) {
            if (crc & 0x8000) {
                crc = (uint16_t)((crc << 1) ^ 0x8005); // 32773
            } else {
                crc = (uint16_t)(crc << 1);
            }
        }
    }
    return crc;
}

static uint16_t crc16_modbus(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFF; // 65535 from a/a.java
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            if (crc & 0x0001) {
                crc = (uint16_t)((crc >> 1) ^ 0xA001);
            } else {
                crc = (uint16_t)(crc >> 1);
            }
        }
    }
    return crc;
}

static void input_report_callback(void *context, IOReturn result, void *sender,
                                  IOHIDReportType type, uint32_t reportID,
                                  uint8_t *report, CFIndex reportLength) {
    (void)context;
    (void)result;
    (void)sender;
    (void)type;
    if (reportID == 0x03 && reportLength > 0) {
        CFIndex copy_len = reportLength > 64 ? 64 : reportLength;
        memcpy(g_reply_buf, report, copy_len);
        g_reply_len = copy_len;
        g_got_reply = 1;
        CFRunLoopStop(CFRunLoopGetCurrent());
    }
}

static int send_and_wait(IOHIDDeviceRef dev, const uint8_t *frame, size_t frame_len, double timeout_sec) {
    uint8_t out[64];
    memset(out, 0, sizeof(out));
    if (frame_len > sizeof(out)) return -1;
    memcpy(out, frame, frame_len);

    g_got_reply = 0;
    g_reply_len = 0;
    memset(g_reply_buf, 0, sizeof(g_reply_buf));

    IOReturn ret = IOHIDDeviceSetReport(dev, kIOHIDReportTypeOutput, 0x04, out, sizeof(out));
    if (ret != kIOReturnSuccess) {
        fprintf(stderr, "IOHIDDeviceSetReport failed: 0x%08x\n", ret);
        return -2;
    }

    CFRunLoopRunInMode(kCFRunLoopDefaultMode, timeout_sec, false);
    if (!g_got_reply) {
        return -3;
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <firmware.bin>\n", argv[0]);
        return 1;
    }

    const char *fw_path = argv[1];
    FILE *fp = fopen(fw_path, "rb");
    if (!fp) {
        perror("fopen");
        return 1;
    }
    fseek(fp, 0, SEEK_END);
    long fw_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (fw_size <= 0 || fw_size > 262144) {
        fprintf(stderr, "Invalid firmware size: %ld\n", fw_size);
        fclose(fp);
        return 1;
    }
    uint8_t *fw = malloc((size_t)fw_size);
    if (!fw || fread(fw, 1, (size_t)fw_size, fp) != (size_t)fw_size) {
        fprintf(stderr, "Failed to read firmware\n");
        fclose(fp);
        return 1;
    }
    fclose(fp);
    printf("Loaded firmware: %s (%ld bytes)\n", fw_path, fw_size);

    IOHIDManagerRef mgr = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
    CFMutableDictionaryRef match = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    int vid = 0x17EF, pid = 0x619E, up = 0xFF00;
    CFNumberRef nVid = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &vid);
    CFNumberRef nPid = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &pid);
    CFNumberRef nUp  = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &up);
    CFDictionarySetValue(match, CFSTR(kIOHIDVendorIDKey), nVid);
    CFDictionarySetValue(match, CFSTR(kIOHIDProductIDKey), nPid);
    CFDictionarySetValue(match, CFSTR(kIOHIDPrimaryUsagePageKey), nUp);

    IOHIDManagerSetDeviceMatching(mgr, match);
    IOHIDManagerScheduleWithRunLoop(mgr, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    IOReturn ret = IOHIDManagerOpen(mgr, kIOHIDOptionsTypeNone);
    if (ret != kIOReturnSuccess) {
        fprintf(stderr, "IOHIDManagerOpen failed: 0x%08x\n", ret);
        free(fw);
        return 1;
    }

    CFSetRef devs = IOHIDManagerCopyDevices(mgr);
    if (!devs || CFSetGetCount(devs) == 0) {
        fprintf(stderr, "No 17EF:619E (UsagePage 0xFF00) device found.\n");
        free(fw);
        return 1;
    }
    const void *dev_ptr = NULL;
    CFSetGetValues(devs, &dev_ptr);
    IOHIDDeviceRef dev = (IOHIDDeviceRef)dev_ptr;

    static uint8_t in_buf[64];
    IOHIDDeviceRegisterInputReportCallback(dev, in_buf, sizeof(in_buf), input_report_callback, NULL);
    IOHIDDeviceScheduleWithRunLoop(dev, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);

    // 1. Query current version
    const uint8_t cmd_ver[] = {0x04, 0x03, 0x04, 0x01, 0x01, 0x35, 0x02};
    if (send_and_wait(dev, cmd_ver, sizeof(cmd_ver), 1.0) != 0) {
        fprintf(stderr, "Version query failed.\n");
        free(fw);
        return 1;
    }
    if (g_reply_buf[1] == 0x04 && g_reply_buf[2] == 0x04) {
        char ver[64] = {0};
        int vlen = g_reply_buf[3];
        if (vlen > 0 && vlen < 60) memcpy(ver, &g_reply_buf[4], vlen);
        printf("Current keyboard version: %s\n", ver);
    }

    // 2. Send CMD_OTA_START (0xFF01)
    const uint8_t cmd_start[] = {0x04, 0x03, 0x01, 0x02, 0x01, 0xFF, 0x44, 0x80};
    int start_ok = 0;
    for (int r = 0; r < 3; r++) {
        if (send_and_wait(dev, cmd_start, sizeof(cmd_start), 1.0) == 0 &&
            g_reply_buf[1] == 0x04 && g_reply_buf[2] == 0x01 && g_reply_buf[4] == 0x01) {
            start_ok = 1;
            break;
        }
        usleep(10000);
    }
    if (!start_ok) {
        fprintf(stderr, "CMD_OTA_START failed! reply: %02x %02x %02x %02x %02x\n",
                g_reply_buf[0], g_reply_buf[1], g_reply_buf[2], g_reply_buf[3], g_reply_buf[4]);
        free(fw);
        return 1;
    }
    printf("CMD_OTA_START accepted.\n");

    // 3. Send 16-byte chunks
    uint16_t total_chunks = (uint16_t)((fw_size + 15) / 16);
    uint8_t pkt[26];
    pkt[0] = 0x04;
    pkt[1] = 0x03;
    pkt[2] = 0x02;
    pkt[3] = 0x14;

    for (uint16_t idx = 0; idx < total_chunks; idx++) {
        pkt[4] = (uint8_t)(idx & 0xFF);
        pkt[5] = (uint8_t)((idx >> 8) & 0xFF);

        uint8_t chunk[16];
        memset(chunk, 0xFF, sizeof(chunk));
        long offset = (long)idx * 16;
        long remain = fw_size - offset;
        if (remain > 16) remain = 16;
        memcpy(chunk, fw + offset, (size_t)remain);
        memcpy(&pkt[6], chunk, 16);

        uint16_t inner_crc = crc16_modbus(&pkt[4], 18);
        pkt[22] = (uint8_t)(inner_crc & 0xFF);
        pkt[23] = (uint8_t)((inner_crc >> 8) & 0xFF);

        uint16_t outer_crc = crc16_outer(&pkt[1], 23);
        pkt[24] = (uint8_t)((outer_crc >> 8) & 0xFF);
        pkt[25] = (uint8_t)(outer_crc & 0xFF);

        int chunk_ok = 0;
        for (int r = 0; r < 3; r++) {
            if (send_and_wait(dev, pkt, sizeof(pkt), 0.5) == 0 &&
                g_reply_buf[1] == 0x04 && g_reply_buf[2] == 0x02 &&
                (g_reply_buf[4] == 0x01 || g_reply_buf[4] == 0x0A)) {
                chunk_ok = 1;
                break;
            }
            usleep(5000);
        }
        if (!chunk_ok) {
            fprintf(stderr, "Chunk %u/%u failed! reply: %02x %02x %02x %02x %02x\n",
                    idx, total_chunks,
                    g_reply_buf[0], g_reply_buf[1], g_reply_buf[2], g_reply_buf[3], g_reply_buf[4]);
            free(fw);
            return 1;
        }
        if ((idx + 1) % 500 == 0 || idx + 1 == total_chunks) {
            printf("Flashing progress: %u / %u chunks (%.1f%%)\n",
                   idx + 1, total_chunks, 100.0 * (idx + 1) / total_chunks);
            fflush(stdout);
        }
    }

    // 4. Send CMD_OTA_END (0xFF02)
    uint8_t end_pkt[14];
    end_pkt[0] = 0x04;
    end_pkt[1] = 0x03;
    end_pkt[2] = 0x03;
    end_pkt[3] = 0x08;
    end_pkt[4] = 0x02;
    end_pkt[5] = 0xFF;
    end_pkt[6] = (uint8_t)(total_chunks & 0xFF);
    end_pkt[7] = (uint8_t)((total_chunks >> 8) & 0xFF);
    end_pkt[8] = (uint8_t)(~end_pkt[6]);
    end_pkt[9] = (uint8_t)(~end_pkt[7]);

    uint16_t end_inner = crc16_modbus(&end_pkt[4], 6);
    end_pkt[10] = (uint8_t)(end_inner & 0xFF);
    end_pkt[11] = (uint8_t)((end_inner >> 8) & 0xFF);

    uint16_t end_outer = crc16_outer(&end_pkt[1], 11);
    end_pkt[12] = (uint8_t)((end_outer >> 8) & 0xFF);
    end_pkt[13] = (uint8_t)(end_outer & 0xFF);

    printf("Sending CMD_OTA_END (total_chunks=%u)...\n", total_chunks);
    int end_ret = send_and_wait(dev, end_pkt, sizeof(end_pkt), 2.0);
    if (end_ret == 0) {
        printf("CMD_OTA_END reply: %02x %02x %02x %02x %02x\n",
               g_reply_buf[0], g_reply_buf[1], g_reply_buf[2], g_reply_buf[3], g_reply_buf[4]);
        if (g_reply_buf[1] == 0x04 && g_reply_buf[2] == 0x03 && g_reply_buf[4] == 0x02) {
            printf("UPGRADE SUCCESS! Keyboard will reboot into new firmware.\n");
        } else {
            fprintf(stderr, "Unexpected CMD_OTA_END status!\n");
            free(fw);
            return 1;
        }
    } else {
        printf("CMD_OTA_END sent (ret=%d; device may have rebooted immediately).\n", end_ret);
    }

    free(fw);
    return 0;
}
