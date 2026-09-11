#pragma once

#include <Arduino.h>
#include <TJpg_Decoder.h>
#include "drv/sd_card_conf.h"
#include "drv/display_conf.h"
#include "drv/lvgl_port.h" 

extern LGFX tft;
 
/**
 * @brief Module hiển thị JPG từ SD card, tự động:
 *  - Scan thư mục tìm toàn bộ file .jpg (không cần khai tên tay)
 *  - Chọn rotation (ngang/dọc) khớp tỉ lệ ảnh
 *  - Chọn scale (1/2/4/8) để ảnh luôn vừa khung màn hình, không bị crop
 *
 * Usage:
 *   gallery.begin("/");       // quét thư mục gốc tìm ảnh jpg
 *   gallery.showNext();       // hiển thị ảnh kế tiếp, tự canh giữa màn hình
 */
class JpgGallery
{
public:
    void start(lv_obj_t *parent, const char *dir = "/",
               uint8_t rotationPortrait = 0, uint8_t rotationLandscape = 1);
    void begin(const char *dir = "/", uint8_t rotationPortrait = 0,
               uint8_t rotationLandscape = 1);
    void stop();
    bool isActive() const { return _active; }
    void showNext();
    void showImage(const char *path);
    size_t imageCount() const { return _files.size(); }
    void setMaxBrightness(uint8_t value) { (void)value; }
    void refreshFileList();
    void setIntervalMs(uint32_t ms) { _intervalMs = ms; }
    void tick();

private:
    static bool _bufferOutputCallback(int16_t x, int16_t y, uint16_t w,
                                      uint16_t h, uint16_t *bitmap);
    void cleanupBuffers();

    std::vector<String> _files;
    size_t _index = 0;
    uint8_t _rotationPortrait = 0;
    uint8_t _rotationLandscape = 1;
    String _lastShownPath;
    String _dir;
    lv_obj_t *_parent = nullptr;
    lv_obj_t *_imageArea = nullptr;
    uint16_t *_frameBuf[2] = {nullptr, nullptr};
    int _activeBuf = 0;
    lv_obj_t *_imgObj[2] = {nullptr, nullptr};
    lv_image_dsc_t _imgDsc[2] = {};
    size_t _frameBufPixels = 0;
    bool _active = false;
    bool _pendingFirstImage = false;
    bool _firstImageDelayMs = false;
    uint32_t _lastSwitchMs;
    uint32_t _intervalMs = 1000;  
    bool _firstImageRendered = false;
};
 
extern JpgGallery gallery;