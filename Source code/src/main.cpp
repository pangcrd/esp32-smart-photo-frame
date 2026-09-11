
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// PhotoFrame Project                                                        //
// LVGL V9.3                                                                 //
// Youtube:https://www.youtube.com/@pangcrd                                  //
// Github: https://github.com/pangcrd                                        //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

/*Using LVGL with Arduino requires some extra steps:
 *Be sure to read the docs here: https://docs.lvgl.io/master/integration/framework/arduino.html*/
 
 #include <Arduino.h>
 #include <lvgl.h>
 #include "drv/lvgl_port.h"
 #include "ui/ui.h"
 #include "drv/sd_card_conf.h"
 #include "jpg/jpg_gallery.h"
 #include "sys/system_monitor.h"
 #include "web/web_server_manager.h"
 
void setup()
{
    Serial.begin(115200);
    if (!sd.begin()) {
        Serial.println("SD card init failed!");
    }

    monitor.begin();
    webServerManager.begin();
    
    lvgl_port_init();
    ui_init();
    ui_events_init();
}

void loop()
{
    ui_events_update();
    lvgl_port_loop();

    monitor.update();
    webServerManager.update();

    gallery.setIntervalMs(webServerManager.slideIntervalMs());
    gallery.tick();
}

