#pragma once

#include <Arduino.h>
#include <FS.h>
#include <SD_MMC.h>
#include <vector>
#include <drv/gpio_conf.h>


/**
 * @brief Abstraction layer cho SD_MMC (4-bit SDIO), 
 * Usage:
 *   sd.begin();
 *   sd.exists("/images");
 *   File f = sd.open("/data.txt");
 *   sd.remove("/old.txt");
 *   sd.mkdir("/logs");
 *   uint64_t free = sd.getFreeSpace();
 */
class SDCardManager {
public:
    /**
     * @brief Init SD_MMC với GPIO custom (ESP32-S3 N16R8)
     * @param oneBitMode true = chỉ dùng D0 (chậm hơn, ít chân hơn), false = 4-bit (mặc định)
     */
    bool begin(bool oneBitMode = sdcard_io::oneBitMode,
               uint32_t freqHz  = sdcard_io::freqHz);
    bool exists(const char *path);
    File open(const char *path, const char *mode = FILE_READ);
    bool remove(const char *path);
    bool mkdir(const char *path);
    bool rmdir(const char *path);
    bool rename(const char *pathFrom, const char *pathTo);

    uint64_t getTotalSpace();  // bytes
    uint64_t getUsedSpace();   // bytes
    uint64_t getFreeSpace();   // bytes = total - used
    uint8_t  cardType();
    bool     isReady() const { return _initialized; }
    std::vector<String> listFiles(const char *dir, const char *ext = nullptr);

    /**
     * @brief Quét thư mục, trả về danh sách file khớp phần mở rộng (không phân biệt hoa/thường).
     * Ví dụ: sd.listFiles("/", ".jpg") -> {"/cat.jpg", "/priscilla.jpg", ...}
     * @param ext nullptr = lấy tất cả file, không lọc theo đuôi
     */

private:
    bool _initialized = false;
    bool _hasExtension(const String &filename, const char *ext);
};

extern SDCardManager sd;