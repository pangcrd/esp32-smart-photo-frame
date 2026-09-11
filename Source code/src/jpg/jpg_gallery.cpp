#include "jpg_gallery.h"

#include <cstring>
#include "esp_heap_caps.h"


JpgGallery gallery;
LGFX tft;

bool JpgGallery::_bufferOutputCallback(int16_t x, int16_t y, uint16_t w,
                                       uint16_t h, uint16_t *bitmap)
{
    if (!gallery._active || bitmap == nullptr) return false;

    uint16_t *target = gallery._frameBuf[gallery._activeBuf];
    if (target == nullptr || gallery._frameBufPixels == 0) return false;

    const uint16_t screenW = tft.width();
    const uint16_t screenH = tft.height();
    for (uint16_t row = 0; row < h; ++row) {
        const int16_t py = y + row;
        if (py < 0 || py >= screenH) continue;

        for (uint16_t col = 0; col < w; ++col) {
            const int16_t px = x + col;
            if (px < 0 || px >= screenW) continue;
            target[static_cast<size_t>(py) * screenW + px] = bitmap[row * w + col];
        }
    }
    return true;
}

void JpgGallery::cleanupBuffers()
{
    for (int i = 0; i < 2; ++i) {
        if (_imgObj[i] != nullptr) {
            lv_anim_delete(_imgObj[i], nullptr);
        }
    }

    const bool imageAreaValid = _imageArea != nullptr && lv_obj_is_valid(_imageArea);
    if (imageAreaValid) {
        lv_obj_del(_imageArea);
    } else {
        for (int i = 0; i < 2; ++i) {
            if (_imgObj[i] != nullptr && lv_obj_is_valid(_imgObj[i])) {
                lv_obj_del(_imgObj[i]);
            }
        }
    }

    for (int i = 0; i < 2; ++i) {
        _imgObj[i] = nullptr;
        if (_frameBuf[i] != nullptr) {
            heap_caps_free(_frameBuf[i]);
            _frameBuf[i] = nullptr;
        }
        std::memset(&_imgDsc[i], 0, sizeof(_imgDsc[i]));
    }

    _imageArea = nullptr;
    _frameBufPixels = 0;
    _activeBuf = 0;
}

void JpgGallery::stop()
{
    _active = false;
    TJpgDec.setCallback(nullptr);
    cleanupBuffers();
    _firstImageRendered = false;

    _parent = nullptr;
    _files.clear();
    _index = 0;
    _lastShownPath = "";
}

void JpgGallery::start(lv_obj_t *parent, const char *dir,
                       uint8_t rotationPortrait, uint8_t rotationLandscape)
{
    stop();

    if (parent == nullptr || !lv_obj_is_valid(parent)) {
        Serial.println("[Gallery] Parent LVGL khong hop le.");
        return;
    }

    _parent = parent;
    _dir = dir != nullptr ? dir : "/";
    _rotationPortrait = rotationPortrait;
    _rotationLandscape = rotationLandscape;
    tft.setSwapBytes(true);

    _frameBufPixels = static_cast<size_t>(tft.width()) * tft.height();
    if (_frameBufPixels == 0) {
        Serial.println("[Gallery] Kich thuoc framebuffer khong hop le.");
        stop();
        return;
    }

    for (int i = 0; i < 2; ++i) {
        _frameBuf[i] = static_cast<uint16_t *>(
            heap_caps_malloc(_frameBufPixels * sizeof(uint16_t), MALLOC_CAP_SPIRAM));
        if (_frameBuf[i] == nullptr) {
            Serial.printf("[Gallery] Khong cap phat duoc PSRAM buffer #%d\n", i);
            stop();
            return;
        }
        std::memset(_frameBuf[i], 0, _frameBufPixels * sizeof(uint16_t));
    }

    _imageArea = lv_obj_create(_parent);
    if (_imageArea == nullptr) {
        Serial.println("[Gallery] Khong tao duoc image area.");
        stop();
        return;
    }
    lv_obj_remove_style_all(_imageArea);
    lv_obj_set_size(_imageArea, LV_PCT(100), LV_PCT(100));
    lv_obj_set_pos(_imageArea, 0, 0);
    lv_obj_clear_flag(_imageArea, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(_imageArea, LV_OBJ_FLAG_SCROLLABLE);      
    lv_obj_move_background(_imageArea);

    for (int i = 0; i < 2; ++i) {
        _imgDsc[i].header.cf = LV_COLOR_FORMAT_RGB565;
        _imgDsc[i].header.w = tft.width();
        _imgDsc[i].header.h = tft.height();
        _imgDsc[i].data_size = _frameBufPixels * sizeof(uint16_t);
        _imgDsc[i].data = reinterpret_cast<const uint8_t *>(_frameBuf[i]);

        _imgObj[i] = lv_image_create(_imageArea);
        if (_imgObj[i] == nullptr) {
            Serial.printf("[Gallery] Khong tao duoc LVGL image #%d\n", i);
            stop();
            return;
        }
        lv_image_set_src(_imgObj[i], &_imgDsc[i]);
        lv_obj_set_pos(_imgObj[i], 0, 0);
        lv_obj_set_style_opa(_imgObj[i], i == 0 ? LV_OPA_COVER : LV_OPA_TRANSP,
                             LV_PART_MAIN);
    }

    TJpgDec.setCallback(_bufferOutputCallback);
    TJpgDec.setJpgScale(1);
    _active = true;
    refreshFileList();
    _pendingFirstImage = true;
    _lastSwitchMs = millis(); 
}

void JpgGallery::begin(const char *dir, uint8_t rotationPortrait,
                       uint8_t rotationLandscape)
{
    (void)dir;
    (void)rotationPortrait;
    (void)rotationLandscape;
    stop();
    Serial.println("[Gallery] begin() da bi vo hieu; dung start(parent, ...) khi vao gallery.");
}

void JpgGallery::refreshFileList()
{
    if (!sd.isReady()) {
        Serial.println("[Gallery] SD chua init, khong the refresh.");
        return;
    }

    const String currentPath = _lastShownPath;
    const char *dir = _dir.length() > 0 ? _dir.c_str() : "/";

    _files.clear();
    auto jpgFiles = sd.listFiles(dir, ".jpg");
    auto jpegFiles = sd.listFiles(dir, ".jpeg");
    _files.insert(_files.end(), jpgFiles.begin(), jpgFiles.end());
    _files.insert(_files.end(), jpegFiles.begin(), jpegFiles.end());

    Serial.printf("[Gallery] Refresh: tim thay %u anh JPEG trong %s\n",
                  static_cast<unsigned>(_files.size()), dir);

    if (_files.empty()) {
        _index = 0;
        _lastShownPath = "";
        return;
    }

    int foundIndex = -1;
    for (size_t i = 0; i < _files.size(); ++i) {
        if (_files[i] == currentPath) {
            foundIndex = static_cast<int>(i);
            break;
        }
    }

    if (foundIndex >= 0) {
        _index = (static_cast<size_t>(foundIndex) + 1) % _files.size();
    } else {
        _index = 0;
        _lastShownPath = "";
    }
}

void JpgGallery::showNext()
{
    if (!_active || _files.empty()) return;
    if (_imgObj[0] == nullptr || _imgObj[1] == nullptr ||
        _frameBuf[0] == nullptr || _frameBuf[1] == nullptr) {
        return;
    }

    const String &nextPath = _files[_index];
    if (nextPath == _lastShownPath) {
        _index = (_index + 1) % _files.size();
        return;
    }

    showImage(nextPath.c_str());
    _index = (_index + 1) % _files.size();
}

void JpgGallery::showImage(const char *path)
{
    if (!_active || path == nullptr || _frameBufPixels == 0) return;

    const int nextBuf = 1 - _activeBuf;
    if (_frameBuf[nextBuf] == nullptr || _imgObj[nextBuf] == nullptr ||
        !lv_obj_is_valid(_imgObj[nextBuf])) {
        return;
    }

    String safePath = path;
    if (!safePath.startsWith("/")) safePath = "/" + safePath;

    uint16_t w = 0;
    uint16_t h = 0;
    TJpgDec.getFsJpgSize(&w, &h, safePath.c_str(), SD_MMC);
    if (w == 0 || h == 0) {
        Serial.printf("[Gallery] Khong doc duoc kich thuoc: %s\n", safePath.c_str());
        return;
    }

    const bool isLandscape = w >= h;
    const uint8_t targetRotation = isLandscape ? _rotationLandscape : _rotationPortrait;
    tft.setRotation(targetRotation);

    const uint16_t screenW = tft.width();
    const uint16_t screenH = tft.height();
    uint8_t scale = 1;
    while ((w / scale > screenW || h / scale > screenH) && scale < 8) {
        scale *= 2;
    }
    TJpgDec.setJpgScale(scale);

    const uint16_t scaledW = w / scale;
    const uint16_t scaledH = h / scale;
    const int16_t x = screenW > scaledW ? (screenW - scaledW) / 2 : 0;
    const int16_t y = screenH > scaledH ? (screenH - scaledH) / 2 : 0;

    _activeBuf = nextBuf;
    std::memset(_frameBuf[nextBuf], 0, _frameBufPixels * sizeof(uint16_t));
    uint32_t decodeMs = millis();
    TJpgDec.drawFsJpg(x, y, safePath.c_str(), SD_MMC);
    decodeMs = millis() - decodeMs;

    _imgDsc[nextBuf].header.w = screenW;
    _imgDsc[nextBuf].header.h = screenH;
    _imgDsc[nextBuf].data_size = _frameBufPixels * sizeof(uint16_t);
    lv_image_set_src(_imgObj[nextBuf], &_imgDsc[nextBuf]);
    lv_image_cache_drop(&_imgDsc[nextBuf]);
    lv_obj_invalidate(_imgObj[nextBuf]);

    const int prevBuf = 1 - nextBuf;
    lv_obj_move_foreground(_imgObj[nextBuf]);
    if (!_firstImageRendered) {
    // Ảnh đầu tiên: không có gì để crossfade từ đó cả, và màn hình
    // chưa từng được vẽ full frame -> fade sẽ blend với buffer rác/trắng.
    // Set thẳng opa COVER, không animate.
    lv_obj_set_style_opa(_imgObj[nextBuf], LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_invalidate(_imgObj[nextBuf]);
    lv_timer_handler();   // ép vẽ ngay 1 frame đầy đủ
    _firstImageRendered = true;
    } else {
    lv_anim_t animIn;
    lv_anim_init(&animIn);
    lv_anim_set_var(&animIn, _imgObj[nextBuf]);
    lv_anim_set_values(&animIn, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_duration(&animIn, 500);
    lv_anim_set_exec_cb(&animIn, [](void *obj, int32_t value) {
        lv_obj_set_style_opa(static_cast<lv_obj_t *>(obj), static_cast<lv_opa_t>(value),
                             LV_PART_MAIN);
    });
    lv_anim_start(&animIn);

    const uint32_t startedAt = millis();
    while (_active && millis() - startedAt < 500) {
        lv_timer_handler();
        delay(5);
    }
}
    if (!_active) return;
    lv_obj_set_style_opa(_imgObj[prevBuf], LV_OPA_TRANSP, LV_PART_MAIN);

    Serial.printf("[Gallery] %s | %ux%u -> scale=%u (%ux%u) rot=%u | decode %ums\n",
                  safePath.c_str(), w, h, scale, scaledW, scaledH, targetRotation,
                  static_cast<unsigned>(decodeMs));
    _lastShownPath = safePath;
}

void JpgGallery::tick()
{
    if (!_active) return;

    const uint32_t now = millis();

    if (_pendingFirstImage) {
        if (now - _lastSwitchMs >= _firstImageDelayMs) {
            showNext();
            _lastSwitchMs = now;
            _pendingFirstImage = false;
        }
        return;
    }

    if (now - _lastSwitchMs >= _intervalMs) {
        showNext();
        _lastSwitchMs = now;
    }
}