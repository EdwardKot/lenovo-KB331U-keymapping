#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/hid/IOHIDDevice.h>
#include <IOKit/hid/IOHIDElement.h>
#include <IOKit/hid/IOHIDValue.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Private IOHIDEventSystemClient APIs used by hidutil (100% read-only event monitoring)
typedef struct __IOHIDEventSystemClient *IOHIDEventSystemClientRef;
typedef struct __IOHIDEvent *IOHIDEventRef;
extern IOHIDEventSystemClientRef IOHIDEventSystemClientCreateWithType(CFAllocatorRef allocator, int type, CFDictionaryRef attrs);
extern void IOHIDEventSystemClientScheduleWithRunLoop(IOHIDEventSystemClientRef client, CFRunLoopRef runloop, CFStringRef mode);
extern void IOHIDEventSystemClientRegisterEventCallback(IOHIDEventSystemClientRef client, void (*callback)(void *target, void *refcon, void *sender, IOHIDEventRef event), void *target, void *refcon);
extern uint32_t IOHIDEventGetType(IOHIDEventRef event);
extern CFIndex IOHIDEventGetIntegerValue(IOHIDEventRef event, uint32_t field);

static void hid_event_callback(void *target, void *refcon, void *sender, IOHIDEventRef event) {
    (void)target;
    (void)refcon;
    (void)sender;
    if (!event) return;
    uint32_t type = IOHIDEventGetType(event);
    // kIOHIDEventTypeKeyboard = 3, kIOHIDEventTypeVendorDefined = 1
    if (type == 3) {
        // Field (3 << 16) | 0 = UsagePage, (3 << 16) | 1 = Usage, (3 << 16) | 2 = Down
        CFIndex up = IOHIDEventGetIntegerValue(event, (3 << 16) | 0);
        CFIndex u = IOHIDEventGetIntegerValue(event, (3 << 16) | 1);
        CFIndex down = IOHIDEventGetIntegerValue(event, (3 << 16) | 2);
        printf("[IOHIDEvent Keyboard] UsagePage=0x%04lX Usage=0x%04lX (%ld) Down=%ld\n",
               (long)up, (long)u, (long)u, (long)down);
        fflush(stdout);
    } else if (type == 1) {
        CFIndex up = IOHIDEventGetIntegerValue(event, (1 << 16) | 0);
        CFIndex u = IOHIDEventGetIntegerValue(event, (1 << 16) | 1);
        CFIndex len = IOHIDEventGetIntegerValue(event, (1 << 16) | 3);
        printf("[IOHIDEvent Vendor] UsagePage=0x%04lX Usage=0x%04lX Len=%ld\n",
               (long)up, (long)u, (long)len);
        fflush(stdout);
    } else {
        printf("[IOHIDEvent type=%u]\n", type);
        fflush(stdout);
    }
}

static void input_report_callback(
    void *context,
    IOReturn result,
    void *sender,
    IOHIDReportType type,
    uint32_t reportID,
    uint8_t *report,
    CFIndex reportLength
) {
    const char *label = (const char *)context;
    (void)sender;
    printf("[%s] InputReport (res=0x%x, type=%d, ReportID=0x%02x, len=%ld): ",
           label ? label : "?", result, (int)type, reportID, (long)reportLength);
    for (CFIndex i = 0; i < reportLength; i++) {
        printf("%02x ", report[i]);
    }
    if (reportID == 0x02 && reportLength >= 3) {
        uint16_t usage = (uint16_t)report[1] | ((uint16_t)report[2] << 8);
        printf(" -> Consumer Usage: 0x%04X (%u) [%s]",
               usage, usage, usage == 0 ? "RELEASE" : "PRESS");
    }
    printf("\n");
    fflush(stdout);
}

static void input_value_callback(
    void *context,
    IOReturn result,
    void *sender,
    IOHIDValueRef value
) {
    const char *label = (const char *)context;
    (void)result;
    (void)sender;
    IOHIDElementRef elem = IOHIDValueGetElement(value);
    if (!elem) return;
    uint32_t up = IOHIDElementGetUsagePage(elem);
    uint32_t u = IOHIDElementGetUsage(elem);
    uint32_t rid = IOHIDElementGetReportID(elem);
    CFIndex intVal = IOHIDValueGetIntegerValue(value);
    if (intVal != 0 || up == 0x0C || up == 0xFF00) {
        printf("  [%s HIDValue] ReportID=0x%02x UsagePage=0x%04X Usage=0x%04X Value=%ld (0x%lx)\n",
               label ? label : "?", rid, up, u, (long)intVal, (unsigned long)intVal);
        fflush(stdout);
    }
}

static io_service_t g_services[4];
static int g_num_services = 0;
static long long g_last_counts[4] = {-1, -1, -1, -1};

static void timer_callback(CFRunLoopTimerRef timer, void *info) {
    (void)timer;
    (void)info;
    for (int i = 0; i < g_num_services; i++) {
        CFDictionaryRef dbg = IORegistryEntryCreateCFProperty(g_services[i], CFSTR("DebugState"), kCFAllocatorDefault, kNilOptions);
        if (dbg && CFGetTypeID(dbg) == CFDictionaryGetTypeID()) {
            CFNumberRef cntRef = CFDictionaryGetValue(dbg, CFSTR("InputReportCount"));
            long long cnt = 0;
            if (cntRef) CFNumberGetValue(cntRef, kCFNumberLongLongType, &cnt);
            if (g_last_counts[i] != -1 && cnt != g_last_counts[i]) {
                printf("  [DebugState Poll] Device #%d InputReportCount changed: %lld -> %lld\n",
                       i, g_last_counts[i], cnt);
                fflush(stdout);
            }
            g_last_counts[i] = cnt;
        }
        if (dbg) CFRelease(dbg);
    }
}

int main(int argc, char **argv) {
    int listen_secs = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--listen") == 0 && i + 1 < argc) {
            listen_secs = atoi(argv[++i]);
        }
    }

    CFMutableDictionaryRef matching = IOServiceMatching("IOHIDDevice");
    int vid = 0x17ef;
    int pid = 0x619e;
    CFNumberRef vidNum = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &vid);
    CFNumberRef pidNum = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &pid);
    CFDictionarySetValue(matching, CFSTR(kIOHIDVendorIDKey), vidNum);
    CFDictionarySetValue(matching, CFSTR(kIOHIDProductIDKey), pidNum);
    CFRelease(vidNum);
    CFRelease(pidNum);

    io_iterator_t iter = 0;
    kern_return_t kr = IOServiceGetMatchingServices(kIOMainPortDefault, matching, &iter);
    if (kr != KERN_SUCCESS) {
        printf("IOServiceGetMatchingServices failed: 0x%x\n", kr);
        return 1;
    }

    io_service_t service;
    int idx = 0;
    while ((service = IOIteratorNext(iter)) != 0) {
        CFNumberRef upRef = IORegistryEntryCreateCFProperty(service, CFSTR(kIOHIDPrimaryUsagePageKey), kCFAllocatorDefault, kNilOptions);
        int up = 0;
        if (upRef) {
            CFNumberGetValue(upRef, kCFNumberIntType, &up);
            CFRelease(upRef);
        }
        CFTypeRef dbgRef = IORegistryEntryCreateCFProperty(service, CFSTR("DebugState"), kCFAllocatorDefault, kNilOptions);
        printf("=== Device #%d (PrimaryUsagePage=0x%04X) ===\n", idx, up);
        if (dbgRef) {
            CFStringRef desc = CFCopyDescription(dbgRef);
            char buf[512];
            if (desc && CFStringGetCString(desc, buf, sizeof(buf), kCFStringEncodingUTF8)) {
                printf("  DebugState: %s\n", buf);
            }
            if (desc) CFRelease(desc);
            CFRelease(dbgRef);
        }

        if (listen_secs > 0 && idx < 4) {
            g_services[g_num_services++] = service;
            IOHIDDeviceRef dev = IOHIDDeviceCreate(kCFAllocatorDefault, service);
            if (dev) {
                IOReturn ret = IOHIDDeviceOpen(dev, kIOHIDOptionsTypeNone);
                printf("  IOHIDDeviceOpen(0x%04X) -> 0x%x\n", up, ret);
                if (ret == kIOReturnSuccess) {
                    const char *label = (up == 0xFF00) ? "Iface1:Vendor+Consumer" : "Iface0:Keyboard";
                    uint8_t *rbuf = (uint8_t *)calloc(1, 128);
                    IOHIDDeviceScheduleWithRunLoop(dev, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
                    IOHIDDeviceRegisterInputReportCallback(dev, rbuf, 128, input_report_callback, (void *)label);
                    IOHIDDeviceRegisterInputValueCallback(dev, input_value_callback, (void *)label);
                }
            }
        } else {
            IOObjectRelease(service);
        }
        idx++;
    }
    IOObjectRelease(iter);

    if (listen_secs > 0) {
        // Also attach IOHIDEventSystemClient (type 1 = Monitor/Passive)
        IOHIDEventSystemClientRef evClient = IOHIDEventSystemClientCreateWithType(kCFAllocatorDefault, 1, NULL);
        if (evClient) {
            IOHIDEventSystemClientScheduleWithRunLoop(evClient, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
            IOHIDEventSystemClientRegisterEventCallback(evClient, hid_event_callback, NULL, NULL);
            printf("  Registered IOHIDEventSystemClient passive callback.\n");
        }
        CFRunLoopTimerRef timer = CFRunLoopTimerCreate(
            kCFAllocatorDefault, CFAbsoluteTimeGetCurrent() + 0.2, 0.2, 0, 0, timer_callback, NULL);
        CFRunLoopAddTimer(CFRunLoopGetCurrent(), timer, kCFRunLoopDefaultMode);

        printf("\nListening for %d seconds (press keys now)...\n", listen_secs);
        fflush(stdout);
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, (CFTimeInterval)listen_secs, false);
        printf("Done listening.\n");
    }
    return 0;
}
