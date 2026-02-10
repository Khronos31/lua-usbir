#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <libusb-1.0/libusb.h>

#include "usbir.h"

// デバイスを開いて構造体を返す（失敗したらNULL）
USBIRDevice* openUSBIR() {
    libusb_context *ctx = NULL;
    if (libusb_init(&ctx) < 0) return NULL;

    libusb_device_handle *handle = libusb_open_device_with_vid_pid(ctx, VENDOR_ID, PRODUCT_ID);
    if (!handle) {
        libusb_exit(ctx);
        return NULL;
    }

    // 自動でカーネルドライバを切り離す設定（これ1行でdetachとclaimが楽になります）
    libusb_set_auto_detach_kernel_driver(handle, 1);

    if (libusb_claim_interface(handle, INTERFACE_NUM) < 0) {
        libusb_close(handle);
        libusb_exit(ctx);
        return NULL;
    }

    // メモリを確保してハンドルを保持
    USBIRDevice *dev = (USBIRDevice*)malloc(sizeof(USBIRDevice));
    dev->ctx = ctx;
    dev->handle = handle;
    return dev;
}

// デバイスを閉じてメモリを解放する
void closeUSBIR(USBIRDevice *dev) {
    if (!dev) return;
    libusb_release_interface(dev->handle, INTERFACE_NUM);
    libusb_close(dev->handle);
    libusb_exit(dev->ctx);
    free(dev);
}

// 送信関数（引数でデバイスを受け取る）
int writeUSBIRex(USBIRDevice *dev, int format_type, unsigned char *code, int code_len1, int code_len2) {
    if (!dev || !dev->handle) return -1;

    unsigned char OUTBuffer[64];
    memset(OUTBuffer, 0, sizeof(OUTBuffer));

    OUTBuffer[0] = 0x61;
    OUTBuffer[1] = (unsigned char)(format_type & 0xFF);
    OUTBuffer[2] = (unsigned char)(code_len1 & 0xFF);
    OUTBuffer[3] = (unsigned char)(code_len2 & 0xFF);

    int code_len_check = (int)((code_len1 + code_len2) / 8);
    if (((code_len1 + code_len2) % 8) > 0) code_len_check++;

    if (code_len_check > 0 && code_len_check <= 60) {
        for (int fi = 0; fi < code_len_check; fi++) {
            OUTBuffer[fi + 4] = code[fi];
        }

        int actual_length = 0;
        int res = libusb_interrupt_transfer(dev->handle, ENDPOINT_OUT, OUTBuffer, 64, &actual_length, 1000);
        return (res == 0) ? 0 : -1;
    }
    return -2;
}

/**
 * readUSBIRex: 赤外線データを受信するまで待機し、受信データを配列に格納する
 * @param handle: libusbデバイスハンドル
 * @param receive_data: 取得したデータを格納するバッファ (64バイト以上を推奨)
 * @return: 受信したバイト数（エラー時は負の値）
 */
int readUSBIRex(USBIRDevice *dev, unsigned char* receive_data) {
    if (!dev || !dev->handle) return -1;
    
    unsigned char OUTBuffer[PKT_SIZE];
    unsigned char INBuffer[PKT_SIZE];
    int actual, res;

    // --- 1. 受信待ちモード(WAIT)に設定 (C#の0x53に相当) ---
    memset(OUTBuffer, 0xFF, PKT_SIZE);
    OUTBuffer[0] = 0x53; 
    OUTBuffer[1] = 0x01; // RECEIVE_WAIT_MODE_WAIT
    
    res = libusb_interrupt_transfer(dev->handle, ENDPOINT_OUT, OUTBuffer, PKT_SIZE, &actual, 1000);
    if (res != 0) return -1;
    res = libusb_interrupt_transfer(dev->handle, ENDPOINT_IN, INBuffer, PKT_SIZE, &actual, 1000);
    if (res != 0 || INBuffer[0] != 0x53) return -1;

    // --- 2. データが来るまでループ (getc風のブロッキング) ---
    //printf("Waiting for IR signal...");
    //fflush(stdout);

    bool received = false;
    while (!received) {
        memset(OUTBuffer, 0xFF, PKT_SIZE);
        OUTBuffer[0] = 0x52; // Get Remocon Data command

        res = libusb_interrupt_transfer(dev->handle, ENDPOINT_OUT, OUTBuffer, PKT_SIZE, &actual, 1000);
        if (res == 0) {
            res = libusb_interrupt_transfer(dev->handle, ENDPOINT_IN, INBuffer, PKT_SIZE, &actual, 1000);
            
            // INBuffer[1] が 0 以外ならデータ受信
            if (res == 0 && INBuffer[0] == 0x52 && INBuffer[1] != 0) {
                // 受信データを引bInterfaceProtocol数の配列へコピー
                // Windows版のロジックに従い INBuffer[1] 以降が実データ
                memcpy(receive_data, &INBuffer[1], PKT_SIZE - 1);
                received = true;
            }
        }
        
        if (!received) {
            usleep(10000); // 10ms待機して再試行
            //printf("."); fflush(stdout);
        }
    }
    //printf(" Received!\n");

    // --- 3. 受信待ちモードを解除 (NONE) (C#の0x53 + MODE_NONEに相当) ---
    memset(OUTBuffer, 0xFF, PKT_SIZE);
    OUTBuffer[0] = 0x53; 
    OUTBuffer[1] = 0x00; // RECEIVE_WAIT_MODE_NONE
    libusb_interrupt_transfer(dev->handle, ENDPOINT_OUT, OUTBuffer, PKT_SIZE, &actual, 1000);
    libusb_interrupt_transfer(dev->handle, ENDPOINT_IN, INBuffer, PKT_SIZE, &actual, 1000);

    return PKT_SIZE - 1; // 返却したデータサイズ
}

