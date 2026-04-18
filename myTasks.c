/* Scheduler include files. */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

/* le fichier .h du TP  */
#include "services.h"
#include "myTasks.h"

// ==========================================
// 按键宏定义 (请根据实际引脚表修改端口和数字!)
// 假设按下时为低电平 (== 0)，如果是高电平有效则改为 (!= 0)
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

// ========== 互斥锁（services.c 中已定义，此处 extern） ==========
extern SemaphoreHandle_t pxLED_MUTEX;

// ========== 刷卡任务（保持不变，只修复句柄名） ==========
void Task_iButton(void *pvParameters) {
    uint64_t current_badge_id;
    int index_trouve;
    
    while (1) {
        if (session_en_cours == 0) {
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
                    
                    if (droit == 1) {
                        if (xTask_Emprunt_Coupante_Handle != NULL)
                            xTaskNotifyGive(xTask_Emprunt_Coupante_Handle);
                        if (xTask_Emprunt_Denude_Handle != NULL)
                            xTaskNotifyGive(xTask_Emprunt_Denude_Handle);
                    } else if (droit == 2) {
                        if (xTask_Retour_Coupante_Handle != NULL)
                            xTaskNotifyGive(xTask_Retour_Coupante_Handle);
                        if (xTask_Retour_Denude_Handle != NULL)
                            xTaskNotifyGive(xTask_Retour_Denude_Handle);
                    }
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

// ========== 借剪线钳任务 ==========
static void Task_Emprunt_Coupante(void *pvParameters) {
    uint32_t etat_capteurs;
    uint32_t free_mask;
    int slot_choisi;

    while (1) {
        // 1. 等待刷卡任务通知
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        // 2. 读取所有微动开关状态
        etat_capteurs = Read_74HC251_Inputs();

        // 3. 找出 0~15 号槽位中空闲的（微动开关未被压下，假设空闲时对应位为 0）
        free_mask = 0;
        for (int i = 0; i < 16; i++) {
            if (!(etat_capteurs & (1 << i))) {
                free_mask |= (1 << i);
            }
        }

        // 4. 点亮空闲槽位 LED（绿色），占用槽位熄灭
        if (xSemaphoreTake(pxLED_MUTEX, portMAX_DELAY) == pdPASS) {
            for (int i = 0; i < 16; i++) {
                if (free_mask & (1 << i)) {
                    LED[i][0] = 0;    // R
                    LED[i][1] = 255;  // G
                    LED[i][2] = 0;    // B
                } else {
                    LED[i][0] = 0;
                    LED[i][1] = 0;
                    LED[i][2] = 0;
                }
            }
            xSemaphoreGive(pxLED_MUTEX);
        }

		// 5. 轮询等待按钮（10 秒超时），并处理闪烁
        int time_elapsed_ms = 0;
        slot_choisi = -1;
        int blink_counter = 0;

        // 假设 def_type_gpio.h 里定义了宏，比如：
        // #define BP_EMPRUNT_COUPE_PRESSED() ((GPIOB->IDR & (1<<0)) == 0)
        
        while (time_elapsed_ms < 10000) {
            // 如果用户按下了借出键，且还没分配槽位
            if (slot_choisi == -1 && BP_EMPRUNT_COUPE_PRESSED()) {
                // 从 free_mask 里随便挑一个空闲的槽位 (比如找最低位的1)
                for (int i = 0; i < 16; i++) {
                    if (free_mask & (1 << i)) {
                        slot_choisi = i;
                        break;
                    }
                }

                if (slot_choisi != -1) {
                    // 7. 开锁
                    taskENTER_CRITICAL();
                    etat_verrous |= (1 << slot_choisi);
                    taskEXIT_CRITICAL();
                    
                    // 8. 更新数据库
                    base_de_donnees[index_utilisateur_courant].id_pince_coupante = slot_choisi + 1;
                    base_de_donnees[index_utilisateur_courant].tick_emprunt_coupante = xTaskGetTickCount();
                }
            }

            // 如果已经选好了槽位，就让那个绿灯 2Hz 闪烁 (每 250ms 翻转一次)
            if (slot_choisi != -1) {
                blink_counter++;
                if (blink_counter >= 5) { // 5 * 50ms = 250ms
                    blink_counter = 0;
                    if (xSemaphoreTake(pxLED_MUTEX, 0) == pdPASS) {
                        // 异或操作：如果是 255 就变 0，如果是 0 就变 255
                        LED[slot_choisi][1] ^= 255; 
                        xSemaphoreGive(pxLED_MUTEX);
                    }
                }
            }

            vTaskDelay(pdMS_TO_TICKS(50));
            time_elapsed_ms += 50;
        }

        // 9. 清理该区域所有 LED
        if (xSemaphoreTake(pxLED_MUTEX, portMAX_DELAY) == pdPASS) {
            for (int i = 0; i < 16; i++) {
                LED[i][0] = 0; LED[i][1] = 0; LED[i][2] = 0;
            }
            xSemaphoreGive(pxLED_MUTEX);
        }

        // 10. 释放会话锁（简单实现，后续可改为引用计数）
        session_en_cours = 0;
    }
}

// ========== 借剥线钳任务 ==========
static void Task_Emprunt_Denude(void *pvParameters) {
    uint32_t etat_capteurs;
    uint32_t free_mask;
    uint32_t notification_val;
    int slot_choisi;

    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        etat_capteurs = Read_74HC251_Inputs();

        // 剥线钳区域：16~31 号槽位
        free_mask = 0;
        for (int i = 16; i < 32; i++) {
            if (!(etat_capteurs & (1 << i))) {
                free_mask |= (1 << i);
            }
        }

        if (xSemaphoreTake(pxLED_MUTEX, portMAX_DELAY) == pdPASS) {
            for (int i = 16; i < 32; i++) {
                if (free_mask & (1 << i)) {
                    LED[i][0] = 0; LED[i][1] = 255; LED[i][2] = 0;
                } else {
                    LED[i][0] = 0; LED[i][1] = 0; LED[i][2] = 0;
                }
            }
            xSemaphoreGive(pxLED_MUTEX);
        }

        if (xTaskNotifyWait(0, 0, &notification_val, pdMS_TO_TICKS(10000)) == pdPASS) {
            slot_choisi = (int)notification_val;
            etat_capteurs = Read_74HC251_Inputs();
            if (!(etat_capteurs & (1 << slot_choisi))) {
                taskENTER_CRITICAL();
                etat_verrous |= (1 << slot_choisi);
                taskEXIT_CRITICAL();

                // 剥线钳编号 1~16（槽位 16~31）
                base_de_donnees[index_utilisateur_courant].id_pince_denude = slot_choisi - 16 + 1;
                base_de_donnees[index_utilisateur_courant].tick_emprunt_denude = xTaskGetTickCount();
            }
        }

        if (xSemaphoreTake(pxLED_MUTEX, portMAX_DELAY) == pdPASS) {
            for (int i = 16; i < 32; i++) {
                LED[i][0] = 0; LED[i][1] = 0; LED[i][2] = 0;
            }
            xSemaphoreGive(pxLED_MUTEX);
        }

        session_en_cours = 0;
    }
}

// ========== 还剪线钳任务 ==========
static void Task_Retour_Coupante(void *pvParameters) {
    uint8_t id_pince;
    int slot;
    uint32_t etat_capteurs;
    uint32_t notification_val;

    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        id_pince = base_de_donnees[index_utilisateur_courant].id_pince_coupante;
        if (id_pince == 0) {
            session_en_cours = 0;
            continue;
        }
        slot = id_pince - 1;

        // 点亮对应槽位 LED（橙色：R+G）
        if (xSemaphoreTake(pxLED_MUTEX, portMAX_DELAY) == pdPASS) {
            LED[slot][0] = 255;   // R
            LED[slot][1] = 100;   // G
            LED[slot][2] = 0;     // B
            xSemaphoreGive(pxLED_MUTEX);
        }

        if (xTaskNotifyWait(0, 0, &notification_val, pdMS_TO_TICKS(10000)) == pdPASS) {
            etat_capteurs = Read_74HC251_Inputs();
            // 检查工具是否已被拔出（空闲条件）
            if (!(etat_capteurs & (1 << slot))) {
                taskENTER_CRITICAL();
                etat_verrous &= ~(1 << slot);   // 关锁
                taskEXIT_CRITICAL();

                base_de_donnees[index_utilisateur_courant].id_pince_coupante = 0;
                base_de_donnees[index_utilisateur_courant].tick_emprunt_coupante = 0;
            }
        }

        // 清理 LED
        if (xSemaphoreTake(pxLED_MUTEX, portMAX_DELAY) == pdPASS) {
            LED[slot][0] = 0; LED[slot][1] = 0; LED[slot][2] = 0;
            xSemaphoreGive(pxLED_MUTEX);
        }

        session_en_cours = 0;
    }
}

// ========== 还剥线钳任务 ==========
static void Task_Retour_Denude(void *pvParameters) {
    uint8_t id_pince;
    int slot;
    uint32_t etat_capteurs;
    uint32_t notification_val;

    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        id_pince = base_de_donnees[index_utilisateur_courant].id_pince_denude;
        if (id_pince == 0) {
            session_en_cours = 0;
            continue;
        }
        slot = id_pince + 15;  // 编号 1~16 对应槽位 16~31

        if (xSemaphoreTake(pxLED_MUTEX, portMAX_DELAY) == pdPASS) {
            LED[slot][0] = 255;
            LED[slot][1] = 100;
            LED[slot][2] = 0;
            xSemaphoreGive(pxLED_MUTEX);
        }

        if (xTaskNotifyWait(0, 0, &notification_val, pdMS_TO_TICKS(10000)) == pdPASS) {
            etat_capteurs = Read_74HC251_Inputs();
            if (!(etat_capteurs & (1 << slot))) {
                taskENTER_CRITICAL();
                etat_verrous &= ~(1 << slot);
                taskEXIT_CRITICAL();

                base_de_donnees[index_utilisateur_courant].id_pince_denude = 0;
                base_de_donnees[index_utilisateur_courant].tick_emprunt_denude = 0;
            }
        }

        if (xSemaphoreTake(pxLED_MUTEX, portMAX_DELAY) == pdPASS) {
            LED[slot][0] = 0; LED[slot][1] = 0; LED[slot][2] = 0;
            xSemaphoreGive(pxLED_MUTEX);
        }

        session_en_cours = 0;
    }
}

// ========== 系统初始化函数 ==========
void vInit_myTasks( UBaseType_t uxPriority )
{
    // 1. 互斥锁创建
	pxLED_MUTEX = xSemaphoreCreateMutex();
	
    // 2. 创建 NeoPixel 刷新任务
    xTaskCreate(BuildTrameNeo,
                "NeoPixel",
                stack_BuildTrameNeo_size,
                NULL,
                uxPriority + 1,
                &xTask_NeoPixel_Handle);

    // 3. 创建刷卡任务
    xTaskCreate(Task_iButton,
                "iButton",
                stack_Task_iButton_size,
                NULL,
                uxPriority,
                &xTask_iButton_Handle);

    // 4. 创建四个借/还任务
    xTaskCreate(Task_Emprunt_Coupante,
                "EmpCoup",
                256,
                NULL,
                uxPriority + 2,
                &xTask_Emprunt_Coupante_Handle);

    xTaskCreate(Task_Emprunt_Denude,
                "EmpDenu",
                256,
                NULL,
                uxPriority + 2,
                &xTask_Emprunt_Denude_Handle);

    xTaskCreate(Task_Retour_Coupante,
                "RetCoup",
                256,
                NULL,
                uxPriority + 2,
                &xTask_Retour_Coupante_Handle);

    xTaskCreate(Task_Retour_Denude,
                "RetDenu",
                256,
                NULL,
                uxPriority + 2,
                &xTask_Retour_Denude_Handle);

    // 5. 硬件初始化
    InitSystem();
}
