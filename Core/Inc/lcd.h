#ifndef __LCD_H
#define __LCD_H

#include "stm32f3xx_hal.h"
#include <stdint.h>

/* ------------------------------------------------------------------
   Provide sane defaults / aliasing so different naming conventions work.
   If your project already defines any of these in a different header,
   it will not be redefined here.
   ------------------------------------------------------------------ */

/* Preferred names used by lcd.c */
#ifndef LCD_RS_PORT
  #ifdef LCD_RS_GPIO_Port
    #define LCD_RS_PORT LCD_RS_GPIO_Port
  #else
    #define LCD_RS_PORT GPIOB
  #endif
#endif

#ifndef LCD_RS_PIN
  #ifdef LCD_RS_Pin
    #define LCD_RS_PIN LCD_RS_Pin
  #else
    #define LCD_RS_PIN GPIO_PIN_0
  #endif
#endif

/* Enable / E pin alias */
#ifndef LCD_EN_PORT
  #ifdef LCD_E_GPIO_Port
    #define LCD_EN_PORT LCD_E_GPIO_Port
  #elif defined(LCD_E_PORT)
    #define LCD_EN_PORT LCD_E_PORT
  #else
    #define LCD_EN_PORT GPIOB
  #endif
#endif

#ifndef LCD_EN_PIN
  #ifdef LCD_E_Pin
    #define LCD_EN_PIN LCD_E_Pin
  #elif defined(LCD_E_PIN)
    #define LCD_EN_PIN LCD_E_PIN
  #else
    #define LCD_EN_PIN GPIO_PIN_1
  #endif
#endif

/* Data pins D4..D7 */
#ifndef LCD_D4_PORT
  #ifdef LCD_D4_GPIO_Port
    #define LCD_D4_PORT LCD_D4_GPIO_Port
  #else
    #define LCD_D4_PORT GPIOB
  #endif
#endif
#ifndef LCD_D4_PIN
  #ifdef LCD_D4_Pin
    #define LCD_D4_PIN LCD_D4_Pin
  #else
    #define LCD_D4_PIN GPIO_PIN_2
  #endif
#endif

#ifndef LCD_D5_PORT
  #ifdef LCD_D5_GPIO_Port
    #define LCD_D5_PORT LCD_D5_GPIO_Port
  #else
    #define LCD_D5_PORT GPIOB
  #endif
#endif
#ifndef LCD_D5_PIN
  #ifdef LCD_D5_Pin
    #define LCD_D5_PIN LCD_D5_Pin
  #else
    #define LCD_D5_PIN GPIO_PIN_3
  #endif
#endif

#ifndef LCD_D6_PORT
  #ifdef LCD_D6_GPIO_Port
    #define LCD_D6_PORT LCD_D6_GPIO_Port
  #else
    #define LCD_D6_PORT GPIOB
  #endif
#endif
#ifndef LCD_D6_PIN
  #ifdef LCD_D6_Pin
    #define LCD_D6_PIN LCD_D6_Pin
  #else
    #define LCD_D6_PIN GPIO_PIN_4
  #endif
#endif

#ifndef LCD_D7_PORT
  #ifdef LCD_D7_GPIO_Port
    #define LCD_D7_PORT LCD_D7_GPIO_Port
  #else
    #define LCD_D7_PORT GPIOB
  #endif
#endif
#ifndef LCD_D7_PIN
  #ifdef LCD_D7_Pin
    #define LCD_D7_PIN LCD_D7_Pin
  #else
    #define LCD_D7_PIN GPIO_PIN_5
  #endif
#endif

/* Public API */
void lcd_init(void);
void lcd_clear(void);
void lcd_print(const char *str);
void lcd_print_line(uint8_t line, const char *str);

#endif /* __LCD_H */
