#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDManager.h>
#include <IOKit/IOKitLib.h>

static int g_got_reply = 0;

static void input_report_callback(void *context, IOReturn result, void *sender,
                                  IOHIDReportType type, uint32_t reportID,
                                  uint8_t *report, CFIndex reportLength) {
    printf("\n[CALLBACK] result=0x%08x type=%d reportID=0x%02x len=%ld\n  HEX: ",
           result, type, reportID, (long)reportLength);
    for (CFIndex i = 0; i < reportLength; i++) {
        printf("%02x ", report[i]);
    }
    printf("\n");
    if (reportID == 0x03 && reportLength >= 5 && report[1] == 0x04 && report[2] == 0x04) {
        int vlen = report[3];
        if (vlen > 0 && 4 + vlen <= reportLength) {
            char ver[64] = {0};
            memcpy(ver, &report[4], vlen < 63 ? vlen : 63);
            printf("  ==> PARSED FIRMWARE VERSION: \"%s\" (len=%d)\n", ver, vlen);
        }
    }
    g_got_reply = 1;
    CFRunLoopStop(CFRunLoopGetCurrent());
}

int main(void) {
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
    printf("IOHIDManagerOpen: 0x%08x\n", ret);

    CFSetRef devs = IOHIDManagerCopyDevices(mgr);
    if (!devs || CFSetGetCount(devs) == 0) {
        printf("No 17EF:619E (UsagePage 0xFF00) device found.\n");
        return 1;
    }

    CFIndex count = CFSetGetCount(devs);
    const void **vals = calloc(count, sizeof(void *));
    CFSetGetValues(devs, vals);
    IOHIDDeviceRef dev = (IOHIDDeviceRef)vals[0];

    static uint8_t in_buf[64];
    IOHIDDeviceRegisterInputReportCallback(dev, in_buf, sizeof(in_buf), input_report_callback, NULL);
    IOHIDDeviceScheduleWithRunLoop(dev, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);

    // Official 7-byte read-only version query from a/e.java line 30:
    // {0x04, 0x03, 0x04, 0x01, 0x01, 0x35, 0x02}, padded to 64 bytes for HID Report ID 4 (1 + 63 bytes)
    uint8_t cmd[64];
    memset(cmd, 0, sizeof(cmd));
    cmd[0] = 0x04;
    cmd[1] = 0x03;
    cmd[2] = 0x04;
    cmd[3] = 0x01;
    cmd[4] = 0x01;
    cmd[5] = 0x35;
    cmd[6] = 0x02;

    printf("Sending 64-byte Output Report ID 0x04: 04 03 04 01 01 35 02 ...\n");
    ret = IOHIDDeviceSetReport(dev, kIOHIDReportTypeOutput, 0x04, cmd, sizeof(cmd));
    printf("IOHIDDeviceSetReport (64B) returned: 0x%08x\n", ret);

    if (ret != kIOReturnSuccess) {
        printf("Trying 7-byte Output Report ID 0x04...\n");
        ret = IOHIDDeviceSetReport(dev, kIOHIDReportTypeOutput, 0x04, cmd, 7);
        printf("IOHIDDeviceSetReport (7B) returned: 0x%08x\n", ret);
    }

    // Run loop for up to 1.0 second to receive Report ID 0x03
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);

    if (!g_got_reply) {
        printf("No callback received within 1.0s.\n");
    }

    return 0;
}
