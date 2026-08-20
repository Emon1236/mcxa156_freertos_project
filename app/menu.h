//
#ifndef APP_MENU_H
#define APP_MENU_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum{
    MENU_KEY_UP = 0U,
    MENU_KEY_DOWN,
    MENU_KEY_OK,
    MENU_KEY_BACK
}menu_key_t;

void Menu_Init(void);

bool Menu_HandleKey(menu_key_t key);

void Menu_Render(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_MENU_H */
