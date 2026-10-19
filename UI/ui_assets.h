#ifndef UI_ASSETS_H
#define UI_ASSETS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 一级菜单图标尺寸。
 *
 * 图像采用XBM格式：
 * - 每行30个像素
 * - 每行占4字节
 * - 共30行
 * - 单个图标大小为4 × 30 = 120字节
 */
#define UI_MAIN_ICON_WIDTH       30U
#define UI_MAIN_ICON_HEIGHT      30U
#define UI_MAIN_ICON_BYTES       120U
#define UI_MAIN_ICON_COUNT       5U

/*
 * 一级菜单图标数组。
 *
 * 第一维：图标索引
 * 第二维：单个图标的XBM数据
 */
extern const uint8_t g_uiMainIcons[UI_MAIN_ICON_COUNT][UI_MAIN_ICON_BYTES];

#ifdef __cplusplus
}
#endif

#endif /* UI_ASSETS_H */