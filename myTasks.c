/* Scheduler include files. */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

/* le fichier .h du TP  */
#include "services.h"
#include "myTasks.h"

// ==========================================
// 按键宏定义
// ==========================================
#define BP_EMPRUNT_COUPE_PRESSED()   ((GPIOB->IDR & (1 << 0)) == 0) // 借剪线钳
#define BP_RETOUR_COUPE_PRESSED()    ((GPIOB->IDR & (1 << 1)) == 0) // 还剪线钳
#define BP_EMPRUNT_DENUDE_PRESSED()  ((GPIOB->IDR & (1 << 2)) == 0) // 借剥线钳
#define BP_RETOUR_DENUDE_PRESSED()   ((GPIOB->IDR & (1 << 3)) == 0) // 还剥线钳
#define BP_FIN_PRESSED()             ((GPIOB->IDR & (1 << 4)) == 0) // 结束按钮

// ========== 前向声明 ==========
static void Task_Emprunt_Coupante(void *pvParameters);
static void Task_Emprunt_Denude(void *pvParameters);
static void Task_Retour_Coupante(void *pvParameters);
static void Task_Retour_Denude(void *pvParameters);

// ========== 任务句柄定义 ==========
TaskHandle_t xTask_iButton_Handle = NULL;
TaskHandle_t xTask_Emprunt_Coupante_Handle = NULL;
TaskHandle_t xTask_Emprunt_Denude_Handle = NULL;
TaskHandle_t xTask_Retour_Coupante_Handle = NULL;
TaskHandle_t xTask_Retour_Denude_Handle = NULL;
TaskHandle_t xTask_NeoPixel_Handle = NULL;

// ========== 会话锁 ==========
volatile int session_en_cours = 0;

// ========== 互斥锁 ==========
extern SemaphoreHandle_t pxLED_MUTEX;

// ========== 刷卡任务 ==========
void Task_iButton(void *pvParameters) {
    uint64_t current_badge_id;
    int index_trouve;
    
    while (1) {
        if (session_en_cours == 0) {
            // 提示：此处可调用 Set_LedIHM(0, 0, 0) 熄灭外部 RGB LED
            gestion_lecture_TAG_OSe();
            
            current_badge_id = 0;
            for (int i = 0; i < 8; i++) {
                current_badge_id |= ((uint64_t)identifiant[i] << (8 * i));
            }
            
            if (current_badge_id != 0) {
                index_trouve = -1;
                for (int i = 0; i < nombre_etudiant; i++) {
                    if (base_de_donnees[i].num_badge == current_badge_id) {
                        index_trouve = i;
                        break;
                    }
                }
                
                if (index_trouve >= 0) {
                    session_en_cours = 1;
                    index_utilisateur_courant = index_trouve;
                    
                    uint8_t droit = base_de_donnees[index_trouve].droits;
                    
                    if (droit == 1) { // 权限1：可借。面板亮绿灯
                        // 提示：此处调用 Set_LedIHM(0, 1, 0); 
                        if (xTask_Emprunt_Coupante_Handle != NULL) xTaskNotifyGive(xTask_Emprunt_Coupante_Handle);
                        if (xTask_Emprunt_Denude_Handle != NULL) xTaskNotifyGive(xTask_Emprunt_Denude_Handle);
                    } else if (droit == 2) { // 权限2：只能还。面板亮橙/黄灯
                        // 提示：此处调用 Set_LedIHM(1, 1, 0); 
                        if (xTask_Retour_Coupante_Handle != NULL) xTaskNotifyGive(xTask_Retour_Coupante_Handle);
                        if (xTask_Retour_Denude_Handle != NULL) xTaskNotifyGive(xTask_Retour_Denude_Handle);
                    }
                } else {
                    // 卡号未识别，亮红灯
                    // 提示：此处调用 Set_LedIHM(1, 0, 0); 
                    vTaskDelay(pdMS_TO_TICKS(1000)); 
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

// ========== 借剪线钳任务 ==========
static void Task_Emprunt_Coupante(void *pvParameters) {
    uint32_t etat_capteurs;
    uint32_t tool_present_mask;
    int slot_choisi;

    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        etat_capteurs = Read_74HC251_Inputs();

        // 寻找有工具的槽位（0代表微动开关被压下，即有工具）
        tool_present_mask = 0;
        for (int i = 0; i < 16; i++) {
            if (!(etat_capteurs & (1 << i))) tool_present_mask |= (1 << i);
        }

        if (xSemaphoreTake(pxLED_MUTEX, portMAX_DELAY) == pdPASS) {
            for (int i = 0; i < 16; i++) {
                if (tool_present_mask & (1 << i)) {
                    LED[i][0] = 0; LED[i][1] = 255; LED[i][2] = 0; // 绿灯：有工具可借
                } else {
                    LED[i][0] = 255; LED[i][1] = 0; LED[i][2] = 0; // 红灯：槽位空了
                }
            }
            xSemaphoreGive(pxLED_MUTEX);
        }

        int time_elapsed_ms = 0;
        slot_choisi = -1;
        int blink_counter = 0;

        while (time_elapsed_ms < 10000) {
            if (slot_choisi == -1 && BP_EMPRUNT_COUPE_PRESSED()) {
                for (int i = 0; i < 16; i++) {
                    if (tool_present_mask & (1 << i)) {
                        slot_choisi = i; break;
                    }
                }
                if (slot_choisi != -1) {
                    taskENTER_CRITICAL();
                    etat_verrous |= (1 << slot_choisi); // 开锁
                    taskEXIT_CRITICAL();
                }
            }

            if (slot_choisi != -1) {
                blink_counter++;
                if (blink_counter >= 5) {
                    blink_counter = 0;
                    if (xSemaphoreTake(pxLED_MUTEX, 0) == pdPASS) {
                        LED[slot_choisi][1] ^= 255; // 2Hz闪烁
                        xSemaphoreGive(pxLED_MUTEX);
                    }
                }
            }

            if (BP_FIN_PRESSED()) break; 

            vTaskDelay(pdMS_TO_TICKS(50));
            time_elapsed_ms += 50;
        }

        if (slot_choisi != -1) {
            etat_capteurs = Read_74HC251_Inputs();
            if (etat_capteurs & (1 << slot_choisi)) { // 状态1说明拔出了
                base_de_donnees[index_utilisateur_courant].id_pince_coupante = slot_choisi + 1;
                base_de_donnees[index_utilisateur_courant].tick_emprunt_coupante = xTaskGetTickCount();
            } else {
                taskENTER_CRITICAL();
                etat_verrous &= ~(1 << slot_choisi); // 没拿走就重新锁上
                taskEXIT_CRITICAL();
            }
        }

        if (xSemaphoreTake(pxLED_MUTEX, portMAX_DELAY) == pdPASS) {
            for (int i = 0; i < 16; i++) { LED[i][0] = 0; LED[i][1] = 0; LED[i][2] = 0; }
            xSemaphoreGive(pxLED_MUTEX);
        }
        session_en_cours = 0;
    }
}

// ========== 借剥线钳任务 ==========
static void Task_Emprunt_Denude(void *pvParameters) {
    uint32_t etat_capteurs;
    uint32_t tool_present_mask;
    int slot_choisi;

    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        etat_capteurs = Read_74HC251_Inputs();

        tool_present_mask = 0;
        for (int i = 16; i < 32; i++) {
            if (!(etat_capteurs & (1 << i))) tool_present_mask |= (1 << i);
        }

        if (xSemaphoreTake(pxLED_MUTEX, portMAX_DELAY) == pdPASS) {
            for (int i = 16; i < 32; i++) {
                if (tool_present_mask & (1 << i)) {
                    LED[i][0] = 0; LED[i][1] = 255; LED[i][2] = 0; // 绿灯：可借
                } else {
                    LED[i][0] = 255; LED[i][1] = 0; LED[i][2] = 0; // 红灯：槽位空了
                }
            }
            xSemaphoreGive(pxLED_MUTEX);
        }

        int time_elapsed_ms = 0;
        slot_choisi = -1;
        int blink_counter = 0;

        while (time_elapsed_ms < 10000) {
            if (slot_choisi == -1 && BP_EMPRUNT_DENUDE_PRESSED()) {
                for (int i = 16; i < 32; i++) {
                    if (tool_present_mask & (1 << i)) {
                        slot_choisi = i; break;
                    }
                }
                if (slot_choisi != -1) {
                    taskENTER_CRITICAL();
                    etat_verrous |= (1 << slot_choisi); 
                    taskEXIT_CRITICAL();
                }
            }

            if (slot_choisi != -1) {
                blink_counter++;
                if (blink_counter >= 5) {
                    blink_counter = 0;
                    if (xSemaphoreTake(pxLED_MUTEX, 0) == pdPASS) {
                        LED[slot_choisi][1] ^= 255; 
                        xSemaphoreGive(pxLED_MUTEX);
                    }
                }
            }

            if (BP_FIN_PRESSED()) break; 

            vTaskDelay(pdMS_TO_TICKS(50));
            time_elapsed_ms += 50;
        }

        if (slot_choisi != -1) {
            etat_capteurs = Read_74HC251_Inputs();
            if (etat_capteurs & (1 << slot_choisi)) {
                base_de_donnees[index_utilisateur_courant].id_pince_denude = slot_choisi - 16 + 1;
                base_de_donnees[index_utilisateur_courant].tick_emprunt_denude = xTaskGetTickCount();
            } else {
                taskENTER_CRITICAL();
                etat_verrous &= ~(1 << slot_choisi); 
                taskEXIT_CRITICAL();
            }
        }

        if (xSemaphoreTake(pxLED_MUTEX, portMAX_DELAY) == pdPASS) {
            for (int i = 16; i < 32; i++) { LED[i][0] = 0; LED[i][1] = 0; LED[i][2] = 0; }
            xSemaphoreGive(pxLED_MUTEX);
        }
        session_en_cours = 0;
    }
}

// ========== 还剪线钳任务 ==========
static void Task_Retour_Coupante(void *pvParameters) {
    uint8_t id_pince;
    int slot_cible;
    uint32_t etat_capteurs;

    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        id_pince = base_de_donnees[index_utilisateur_courant].id_pince_coupante;
        if (id_pince == 0) { session_en_cours = 0; continue; }
        slot_cible = id_pince - 1; 

        etat_capteurs = Read_74HC251_Inputs();

        if (xSemaphoreTake(pxLED_MUTEX, portMAX_DELAY) == pdPASS) {
            for(int i=0; i<16; i++) {
                if (etat_capteurs & (1 << i)) {
                    LED[i][0] = 255; LED[i][1] = 100; LED[i][2] = 0; // 橙：空位可还
                } else {
                    LED[i][0] = 255; LED[i][1] = 0; LED[i][2] = 0;   // 红：已被占
                }
            }
            xSemaphoreGive(pxLED_MUTEX);
        }

        int time_elapsed_ms = 0;
        int retour_en_cours = 0;

        while (time_elapsed_ms < 10000) {
            if (!retour_en_cours && BP_RETOUR_COUPE_PRESSED()) {
                retour_en_cours = 1;
                taskENTER_CRITICAL();
                etat_verrous |= (1 << slot_cible); 
                taskEXIT_CRITICAL();
            }

            if (BP_FIN_PRESSED()) break; 

            vTaskDelay(pdMS_TO_TICKS(50));
            time_elapsed_ms += 50;
        }

        if (retour_en_cours) {
            etat_capteurs = Read_74HC251_Inputs();
            if (!(etat_capteurs & (1 << slot_cible))) { 
                base_de_donnees[index_utilisateur_courant].id_pince_coupante = 0;
                base_de_donnees[index_utilisateur_courant].tick_emprunt_coupante = 0;
            }
            taskENTER_CRITICAL();
            etat_verrous &= ~(1 << slot_cible);
            taskEXIT_CRITICAL();
        }

        if (xSemaphoreTake(pxLED_MUTEX, portMAX_DELAY) == pdPASS) {
            for(int i=0; i<16; i++) { LED[i][0] = 0; LED[i][1] = 0; LED[i][2] = 0; }
            xSemaphoreGive(pxLED_MUTEX);
        }
        session_en_cours = 0;
    }
}

// ========== 还剥线钳任务 ==========
static void Task_Retour_Denude(void *pvParameters) {
    uint8_t id_pince;
    int slot_cible;
    uint32_t etat_capteurs;

    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        id_pince = base_de_donnees[index_utilisateur_courant].id_pince_denude;
        if (id_pince == 0) { session_en_cours = 0; continue; }
        slot_cible = id_pince + 15; 

        etat_capteurs = Read_74HC251_Inputs();

        if (xSemaphoreTake(pxLED_MUTEX, portMAX_DELAY) == pdPASS) {
            for(int i=16; i<32; i++) {
                if (etat_capteurs & (1 << i)) {
                    LED[i][0] = 255; LED[i][1] = 100; LED[i][2] = 0; 
                } else {
                    LED[i][0] = 255; LED[i][1] = 0; LED[i][2] = 0;   
                }
            }
            xSemaphoreGive(pxLED_MUTEX);
        }

        int time_elapsed_ms = 0;
        int retour_en_cours = 0;

        while (time_elapsed_ms < 10000) {
            if (!retour_en_cours && BP_RETOUR_DENUDE_PRESSED()) {
                retour_en_cours = 1;
                taskENTER_CRITICAL();
                etat_verrous |= (1 << slot_cible); 
                taskEXIT_CRITICAL();
            }

            if (BP_FIN_PRESSED()) break; 

            vTaskDelay(pdMS_TO_TICKS(50));
            time_elapsed_ms += 50;
        }

        if (retour_en_cours) {
            etat_capteurs = Read_74HC251_Inputs();
            if (!(etat_capteurs & (1 << slot_cible))) { 
                base_de_donnees[index_utilisateur_courant].id_pince_denude = 0;
                base_de_donnees[index_utilisateur_courant].tick_emprunt_denude = 0;
            }
            taskENTER_CRITICAL();
            etat_verrous &= ~(1 << slot_cible);
            taskEXIT_CRITICAL();
        }

        if (xSemaphoreTake(pxLED_MUTEX, portMAX_DELAY) == pdPASS) {
            for(int i=16; i<32; i++) { LED[i][0] = 0; LED[i][1] = 0; LED[i][2] = 0; }
            xSemaphoreGive(pxLED_MUTEX);
        }
        session_en_cours = 0;
    }
}

// 定义 4小时30分钟对应的 Tick 数 (4.5 * 3600 * 1000 毫秒)
#define TIMEOUT_4H30 16200000 

static void Task_Check_Stock(void *pvParameters) {
    while(1) {
        uint32_t current_tick = xTaskGetTickCount();
        
        for(int i = 0; i < nombre_etudiant; i++) {
            if(base_de_donnees[i].droits == 1) {
                if(base_de_donnees[i].id_pince_coupante != 0 && 
                  (current_tick - base_de_donnees[i].tick_emprunt_coupante) > TIMEOUT_4H30) {
                    base_de_donnees[i].droits = 2; // 降级为等待归还
                }
                else if(base_de_donnees[i].id_pince_denude != 0 && 
                  (current_tick - base_de_donnees[i].tick_emprunt_denude) > TIMEOUT_4H30) {
                    base_de_donnees[i].droits = 2; // 降级为等待归还
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// ========== 系统初始化函数 ==========
void vInit_myTasks( UBaseType_t uxPriority )
{
    pxLED_MUTEX = xSemaphoreCreateMutex();
    
    xTaskCreate(BuildTrameNeo, "NeoPixel", stack_BuildTrameNeo_size, NULL, uxPriority + 1, &xTask_NeoPixel_Handle);
    xTaskCreate(Task_iButton, "iButton", stack_Task_iButton_size, NULL, uxPriority, &xTask_iButton_Handle);
    xTaskCreate(Task_Emprunt_Coupante, "EmpCoup", 256, NULL, uxPriority + 2, &xTask_Emprunt_Coupante_Handle);
    xTaskCreate(Task_Emprunt_Denude, "EmpDenu", 256, NULL, uxPriority + 2, &xTask_Emprunt_Denude_Handle);
    xTaskCreate(Task_Retour_Coupante, "RetCoup", 256, NULL, uxPriority + 2, &xTask_Retour_Coupante_Handle);
    xTaskCreate(Task_Retour_Denude, "RetDenu", 256, NULL, uxPriority + 2, &xTask_Retour_Denude_Handle);
    xTaskCreate(Task_Check_Stock, "StockChk", 128, NULL, uxPriority, NULL);

    InitSystem();
}
