
#include "UI/ui_platform.h"
#include <stddef.h>

bool UI_Platform_SettingsLoad(void *data, uint32_t size){
    (void)data;
    (void)size;
    //预留位置，从内部flash读取，校验版本号，校验CRC，数据正确时返回true
    return false;
}

bool UI_Platform_SettingsSave(const void *data, uint32_t size){
    (void)data;
    (void)size;

    /*
     * 预留位置：
     * 不建议直接把结构体裸写入Flash。
     * 正式实现时应增加magic、版本号和CRC。
     */
    return false;    
}

bool UI_Platform_ReadVoltageMillivolts(uint16_t *millivolts){
    if (millivolts != NULL)
    {
        *millivolts = 0U;
    }

    /* 将来在这里调用MCUXpresso ADC驱动。 */
    return false;
}

bool UI_Platform_HIDVolumeUp(void)
{
    return false;
}

bool UI_Platform_HIDVolumeDown(void)
{
    return false;
}

bool UI_Platform_HIDBrightnessUp(void)
{
    return false;
}

bool UI_Platform_HIDBrightnessDown(void)
{
    return false;
}

bool UI_Platform_HIDSendKey(uint8_t keyCode)
{
    (void)keyCode;
    return false;
}
