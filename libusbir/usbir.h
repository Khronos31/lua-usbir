#ifndef USBIR_H
#define USBIR_H

#include <libusb-1.0/libusb.h>

#define VENDOR_ID     0x22ea
#define PRODUCT_ID    0x001e
#define INTERFACE_NUM 3
#define ENDPOINT_OUT  0x04
#define ENDPOINT_IN   0x84
#define PKT_SIZE      64

#ifdef __cplusplus
extern "C" {
#endif

// デバイスのハンドルを一括管理する構造体
typedef struct {
    libusb_context *ctx;
    libusb_device_handle *handle;
} USBIRDevice;

// 公開するAPI
USBIRDevice* openUSBIR();
void closeUSBIR(USBIRDevice *dev);
int writeUSBIRex(USBIRDevice *dev, int format_type, unsigned char *code, int code_len1, int code_len2);
int readUSBIRex(USBIRDevice *dev, unsigned char* receive_data);

#ifdef __cplusplus
}
#endif

#endif
