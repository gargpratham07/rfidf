/* main.c - Campus Wallet with vendor 5s timeout
   Wiring assumed:
   - LCD: PB0..PB5 (D4..D7, RS, EN)
   - RC522: PA4=CS, PA5=SCK, PA6=MISO, PA7=MOSI, PB12=RST
   - CONFIRM: PC13 (active LOW)
   - EXIT: PC14 (active LOW)
   - BUZZER: PA8 (active buzzer recommended)
*/

#include "main.h"
#include "mfrc522.h"
#include "lcd.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* CubeMX prototypes (replace with your generated ones if present) */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART2_UART_Init(void);
void Error_Handler(void);

/* Peripheral handles */
SPI_HandleTypeDef hspi1;
UART_HandleTypeDef huart2;

/* route printf to UART2 */
int __io_putchar(int ch) {
    HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    return ch;
}

/* ---------------- CONFIG ---------------- */
#define MAX_STUDENTS    2
#define MAX_HISTORY     10
#define ITEMS_PER_SHOP  3
#define VENDOR_COUNT    2
#define LOW_BAL         20
#define INACTIVITY_MS   7000    /* general no-activity => ready */
#define STUDENT_MENU_TIMEOUT_MS 5000 /* student menu timeout (5s) */
#define VENDOR_TIMEOUT_MS 5000 /* vendor card selection timeout (5s) */
#define CONFIRM_TIMEOUT_MS 5000

/* Vendor UIDs (from you) */
static const uint8_t vendor_uids[VENDOR_COUNT][5] = {
    {0xB7,0xC8,0x64,0xB2,0xA9}, /* vendor 0 (canteen) */
    {0xF7,0x88,0x60,0xB2,0xAD}  /* vendor 1 (stationery) */
};
static const int vendor_shop_map[VENDOR_COUNT] = {0,1};

/* Shops / Items / Prices */
enum { SHOP_CANTEEN=0, SHOP_STATIONERY=1 };
static const char *shop_names[] = {"CANTEEN","STATIONERY"};
static const char *items[2][ITEMS_PER_SHOP] = {
    {"SAMOSA","MAGGI","FRUITY"},
    {"PEN","NOTE","ERASR"}
};
static const int prices[2][ITEMS_PER_SHOP] = {
    {10,25,15},
    {5,20,3}
};

/* Students (UIDs provided) */
static uint8_t student_uids[MAX_STUDENTS][5] = {
    {0x67,0x5B,0x93,0xB2,0x1D},
    {0x82,0x82,0x5A,0x30,0x6A}
};
static const char *student_name[MAX_STUDENTS] = {"Saumya","Ravi"};
static const char *student_roll[MAX_STUDENTS] = {"R123","R124"};
static const char *student_snu[MAX_STUDENTS]  = {"SNU001","SNU002"};
static uint32_t student_bal[MAX_STUDENTS] = {200,150};

/* History */
typedef struct { char txt[48]; uint32_t t; } hist_t;
static hist_t shop_history[MAX_HISTORY];
static int shop_hist_count = 0;
static hist_t student_history[MAX_STUDENTS][MAX_HISTORY];
static int student_hist_count[MAX_STUDENTS] = {0};

/* ---------- helpers ---------- */
static inline int uid_eq(const uint8_t *a,const uint8_t *b){
    for(int i=0;i<5;i++) if(a[i]!=b[i]) return 0;
    return 1;
}
static int vendor_index_for_uid(const uint8_t *u){
    for(int i=0;i<VENDOR_COUNT;i++) if(uid_eq(u, vendor_uids[i])) return i;
    return -1;
}
static int find_student(const uint8_t *u){
    for(int i=0;i<MAX_STUDENTS;i++) if(uid_eq(u, student_uids[i])) return i;
    return -1;
}
static void push_shop_history(const char *s){
    int idx = shop_hist_count % MAX_HISTORY;
    snprintf(shop_history[idx].txt, sizeof(shop_history[idx].txt), "%s", s);
    shop_history[idx].t = HAL_GetTick();
    shop_hist_count++;
}
static void push_student_history(int stu, const char *s){
    if(stu<0 || stu>=MAX_STUDENTS) return;
    int idx = student_hist_count[stu] % MAX_HISTORY;
    snprintf(student_history[stu][idx].txt, sizeof(student_history[stu][idx].txt), "%s", s);
    student_history[stu][idx].t = HAL_GetTick();
    student_hist_count[stu]++;
}

/* buzzer helpers (active buzzer) */
static void beep_pulse(uint16_t ms){
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_SET);
    HAL_Delay(ms);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET);
}
static void beep_detect(){ beep_pulse(40); }
static void beep_success(){ beep_pulse(180); }
static void beep_lowbal(){ beep_pulse(400); }
static void beep_error(){ beep_pulse(120); HAL_Delay(80); beep_pulse(120); }

/* LCD helpers */
static void show_ready(void){
    lcd_clear();
    lcd_print_line(1,"SNU RFID Wallet");
    lcd_print_line(2,"is ready");
}
static void show_shop_idle(void){ show_ready(); }
static void show_student_basic(int s){
    char l2[17];
    lcd_clear();
    lcd_print_line(1, student_name[s]);
    snprintf(l2, sizeof(l2), "Bal:%lu", (unsigned long)student_bal[s]);
    lcd_print_line(2, l2);
}
static void show_add_prompt(void){
    lcd_clear();
    lcd_print_line(1,"ADD +100 ?");
    lcd_print_line(2,"Press CONFIRM");
}
static void show_history_student(int s){
    int cnt = student_hist_count[s] > MAX_HISTORY ? MAX_HISTORY : student_hist_count[s];
    if(cnt == 0){
        lcd_clear(); lcd_print_line(1,"HISTORY:"); lcd_print_line(2,"No records");
        HAL_Delay(1100); return;
    }
    int start = student_hist_count[s] - cnt;
    for(int i=0;i<cnt;i++){
        int idx = (start + i) % MAX_HISTORY;
        lcd_clear();
        lcd_print_line(1,"HISTORY:");
        char ln[17]; strncpy(ln, student_history[s][idx].txt, 16); ln[16]=0;
        lcd_print_line(2, ln);
        HAL_Delay(1200);
    }
    /* === ADDED: 2 second beep AFTER showing student's history === */
    beep_pulse(2000);
}

/* ---------------- MAIN ---------------- */
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_SPI1_Init();
    MX_USART2_UART_Init();

    printf("UART OK\r\n");

    MFRC522_Init();
    lcd_init();
    lcd_clear();
    HAL_Delay(50);

    /* startup beep */
    beep_pulse(80);

    show_shop_idle();

    /* state */
    uint8_t uid[5];
    int card_present_prev = 0;
    uint32_t last_activity = HAL_GetTick();     /* any activity */
    uint32_t last_student_activity = 0;         /* student menu timestamp */
    uint32_t last_vendor_activity = 0;          /* vendor selection timestamp */

    int selectedShop = -1;
    int vendor_cycle_idx = -1;
    int selectedItem = -1;

    int menu_student = -1;
    int menu_state = 0; /* 0 balance,1 add,2 history,3 exit */

    while (1)
    {
        int chk = MFRC522_Check(uid); /* MI_OK if present */
        int card_present = (chk == MI_OK) ? 1 : 0;
        int new_tap = (card_present && !card_present_prev);
        int removed = (!card_present && card_present_prev);

        if (card_present) {
            last_activity = HAL_GetTick();
            if (menu_student >= 0) last_student_activity = HAL_GetTick();
            if (selectedItem >= 0) last_vendor_activity = HAL_GetTick();
        }

        /* General inactivity -> main ready (7s) */
        if (!card_present && (HAL_GetTick() - last_activity >= INACTIVITY_MS)
            && menu_student < 0 && selectedItem < 0)
        {
            selectedItem = -1; selectedShop = -1; vendor_cycle_idx = -1;
            menu_student = -1; menu_state = 0;
            show_shop_idle();
            last_activity = HAL_GetTick();
        }

        /* Student-menu-specific timeout (5s) */
        if (menu_student >= 0) {
            if (HAL_GetTick() - last_student_activity >= STUDENT_MENU_TIMEOUT_MS) {
                menu_student = -1; menu_state = 0;
                show_shop_idle();
                last_activity = HAL_GetTick();
            }
        }

        /* Vendor selection timeout (5s): if vendor selected but no vendor activity, reset */
        if (selectedItem >= 0) {
            if (HAL_GetTick() - last_vendor_activity >= VENDOR_TIMEOUT_MS) {
                selectedItem = -1;
                selectedShop = -1;
                vendor_cycle_idx = -1;
                show_shop_idle();
                last_activity = HAL_GetTick();
            }
        }

        /* NEW TAP handling */
        if (new_tap)
        {
            last_activity = HAL_GetTick();
            beep_detect();

            /* vendor card? */
            int vidx = vendor_index_for_uid(uid);
            if (vidx >= 0)
            {
                selectedShop = vendor_shop_map[vidx];
                if (vendor_cycle_idx < 0) vendor_cycle_idx = 0;
                else vendor_cycle_idx = (vendor_cycle_idx + 1) % ITEMS_PER_SHOP;
                selectedItem = vendor_cycle_idx;
                last_vendor_activity = HAL_GetTick();

                char b2[17];
                snprintf(b2, sizeof(b2), "%s %d", items[selectedShop][selectedItem], prices[selectedShop][selectedItem]);
                lcd_clear(); lcd_print_line(1, shop_names[selectedShop]); lcd_print_line(2, b2);
                printf("Vendor: %s | Item: %s (%d)\r\n", shop_names[selectedShop], items[selectedShop][selectedItem], prices[selectedShop][selectedItem]);
                HAL_Delay(400);
                card_present_prev = card_present;
                continue;
            }

            /* student card? */
            int stu = find_student(uid);
            if (stu < 0)
            {
                /* unknown card during payment attempt -> cancel vendor selection and go idle */
                if (selectedItem >= 0 && selectedShop >= 0)
                {
                    lcd_clear(); lcd_print_line(1,"UNKNOWN CARD"); lcd_print_line(2,"Payment Cancel");
                    printf("Unregistered attempted payment UID: %02X %02X %02X %02X %02X\r\n", uid[0],uid[1],uid[2],uid[3],uid[4]);
                    beep_error();
                    selectedItem = -1; selectedShop = -1; vendor_cycle_idx = -1;
                    show_shop_idle();
                    HAL_Delay(900);
                    while (MFRC522_Check(uid) == MI_OK) HAL_Delay(80);
                    card_present_prev = 0; last_activity = HAL_GetTick();
                    continue;
                }

                /* otherwise unknown */
                lcd_clear(); lcd_print_line(1,"UNKNOWN CARD"); lcd_print_line(2,"Contact Admin");
                beep_error(); HAL_Delay(900);
                card_present_prev = card_present; last_activity = HAL_GetTick();
                continue;
            }

            /* if vendor selected => payment flow */
            if (selectedItem >= 0 && selectedShop >= 0)
            {
                int price = prices[selectedShop][selectedItem];
                char l2[17]; snprintf(l2, sizeof(l2), "Pay %d Rs", price);
                lcd_clear(); lcd_print_line(1, items[selectedShop][selectedItem]); lcd_print_line(2, l2);
                printf("Await CONFIRM for %s to pay %d\n", student_name[stu], price);

                uint32_t t0 = HAL_GetTick(); int confirmed = 0;
                while (HAL_GetTick() - t0 < CONFIRM_TIMEOUT_MS)
                {
                    if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET) { confirmed = 1; break; }
                    HAL_Delay(40);
                }

                if (!confirmed)
                {
                    lcd_clear(); lcd_print_line(1,"PAY CANCEL"); lcd_print_line(2,"Timeout");
                    HAL_Delay(1200);
                    selectedItem = -1; selectedShop = -1; vendor_cycle_idx = -1;
                    show_shop_idle();
                    card_present_prev = card_present; last_activity = HAL_GetTick();
                    continue;
                }

                /* do payment */
                if (student_bal[stu] >= (uint32_t)price)
                {
                    student_bal[stu] -= price;
                    char rec[128];
                    snprintf(rec, sizeof(rec), "%s paid %s %d Rs Bal:%lu (%s,%s)",
                             student_name[stu], items[selectedShop][selectedItem], price,
                             (unsigned long)student_bal[stu], student_roll[stu], student_snu[stu]);
                    push_shop_history(rec); push_student_history(stu, rec);

                    lcd_clear(); lcd_print_line(1,"PAID OK"); lcd_print_line(2, items[selectedShop][selectedItem]);
                    printf("RECEIPT: %s\r\n", rec);
                    beep_success();
                    HAL_Delay(1800);

                    if (student_bal[stu] < LOW_BAL)
                    {
                        beep_lowbal();
                        lcd_clear(); lcd_print_line(1,"LOW BALANCE"); lcd_print_line(2,"Please Topup");
                        HAL_Delay(2000);
                    }
                }
                else
                {
                    lcd_clear(); lcd_print_line(1,"INSUFFICIENT"); lcd_print_line(2,"TOP-UP REQ");
                    beep_error();
                    HAL_Delay(1800);
                }

                selectedItem = -1; selectedShop = -1; vendor_cycle_idx = -1;
                show_shop_idle();
                while (MFRC522_Check(uid) == MI_OK) HAL_Delay(80);
                card_present_prev = 0; last_activity = HAL_GetTick();
                continue;
            }

            /* student menu cycling */
            if (menu_student != stu)
            {
                menu_student = stu; menu_state = 0;
                show_student_basic(stu);
                last_student_activity = HAL_GetTick();
                card_present_prev = card_present; last_activity = HAL_GetTick();
                HAL_Delay(300);
                continue;
            }
            else
            {
                menu_state = (menu_state + 1) % 4;
                if (menu_state == 0) show_student_basic(stu);
                else if (menu_state == 1) show_add_prompt();
                else if (menu_state == 2) show_history_student(stu);
                else { menu_student = -1; menu_state = 0; show_shop_idle(); }
                last_student_activity = HAL_GetTick();
                card_present_prev = card_present; last_activity = HAL_GetTick();
                HAL_Delay(300);
                continue;
            }
        } /* end new_tap */

        /* Non-blocking confirm for ADD page */
        if (menu_student >= 0 && menu_state == 1)
        {
            if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET)
            {
                HAL_Delay(80);
                if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET)
                {
                    int stu = menu_student;
                    student_bal[stu] += 100;
                    char rec[128];
                    snprintf(rec, sizeof(rec), "Topup +100 by %s Bal:%lu (%s,%s)",
                             student_name[stu], (unsigned long)student_bal[stu], student_roll[stu], student_snu[stu]);
                    push_shop_history(rec); push_student_history(stu, rec);

                    lcd_clear(); lcd_print_line(1,"TOPUP DONE"); lcd_print_line(2,"+100 Added");
                    beep_success();
                    HAL_Delay(1400);

                    menu_state = 2;
                    while (MFRC522_Check(uid) == MI_OK) HAL_Delay(80);
                    card_present_prev = 0; last_activity = HAL_GetTick(); last_student_activity = HAL_GetTick();
                    continue;
                }
            }
        }

        /* Exit (PC14) resets UI */
        if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_14) == GPIO_PIN_RESET)
        {
            selectedItem = -1; selectedShop = -1; vendor_cycle_idx = -1;
            menu_student = -1; menu_state = 0;
            show_shop_idle();
            HAL_Delay(300); last_activity = HAL_GetTick();
        }

        card_present_prev = card_present;
        HAL_Delay(60);
    } /* main loop */

    return 0;
}

/* ---------------- CubeMX-stubbed peripheral functions ---------------- */
/* Use your generated functions if you have them. */

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  RCC_OscInitStruct.PLL.PREDIV = RCC_PREDIV_DIV1;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) Error_Handler();

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK|
                                RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK) Error_Handler();

  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART2;
  PeriphClkInit.Usart2ClockSelection = RCC_USART2CLKSOURCE_PCLK1;
  /* == FIXED: use single-argument call here == */
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) Error_Handler();
}

static void MX_SPI1_Init(void)
{
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 7;
  hspi1.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi1.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
  if (HAL_SPI_Init(&hspi1) != HAL_OK) Error_Handler();
}

static void MX_USART2_UART_Init(void)
{
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK) Error_Handler();
}

static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* LCD PB0..PB5 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
  HAL_GPIO_WritePin(GPIOB, GPIO_InitStruct.Pin, GPIO_PIN_RESET);

  /* SPI CS (PA4) and BUZZER (PA8) */
  GPIO_InitStruct.Pin = GPIO_PIN_4|GPIO_PIN_8;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4|GPIO_PIN_8, GPIO_PIN_RESET);

  /* RC522 RST PB12 */
  GPIO_InitStruct.Pin = GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET);

  /* Buttons PC13 (CONFIRM) and PC14 (EXIT) as input pull-up */
  GPIO_InitStruct.Pin = GPIO_PIN_13|GPIO_PIN_14;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) { }
}
