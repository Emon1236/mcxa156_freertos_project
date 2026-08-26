#ifndef UI_PLATFORM_H
#define UI_PLATFORM_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
//保存和读取平台设置
bool UI_Platform_SettingsLoad(void *data, uint32_t size);
bool UI_Platform_SettingsSave(const void *data, uint32_t size);

bool UI_Platform_ResetSettings(void);   // 获取电压采样值，单位mV

bool UI_Platform_ReadVoltageMillivolts(uint16_t *millivolts);
//USB HID接口
bool UI_Platform_HIDVolmeUp(void);
bool UI_Platform_HIDVolmeDown(void);
bool UI_Platform_HIDBrightnessUp(void);
bool UI_Platform_HIDBrightnessDown(void);
bool UI_Platform_HIDSendKey(uint8_t KeyCode);


#ifdef __cplusplus
}
#endif

#endif // UI_PLATFORM_H