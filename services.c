#include "stm32f10x.h"
#include "def_type_gpio.h"
#include "FreeRTOS.h"
#include "semphr.h" 
#include "services.h"

int identifiant[8];
uint32_t LED[32][3];
uint32_t TRAME_NEO[769];
BadgeData base_de_donnees[nombre_etudiant]; //20 students
int index_utilisateur_courant;
SemaphoreHandle_t pxLED_MUTEX;

void InitSystem(){
	Timer1_Init();
	init_timer2();
	InitSensorsGPIO();
	InitButtonsGPIO();
	TIM3_Init_Servos();
}

void Init_Database_Test(void){
	 // 假设有 3 个测试学生
    // 学生 0
    base_de_donnees[0].num_badge = 0x01;
    base_de_donnees[0].droits = 1;   // 可以借
    base_de_donnees[0].id_pince_coupante = 0;
    base_de_donnees[0].id_pince_denude = 0;
    base_de_donnees[0].tick_emprunt_coupante = 0;
    base_de_donnees[0].tick_emprunt_denude = 0;

    // 学生 1
    base_de_donnees[1].num_badge = 0x02;
    base_de_donnees[1].droits = 1;
	base_de_donnees[1].id_pince_coupante = 0;
    base_de_donnees[1].id_pince_denude = 2;
    base_de_donnees[1].tick_emprunt_coupante = 0;
    base_de_donnees[1].tick_emprunt_denude = 10;

    // 学生 2（可以模拟一个只能还的状态）
    base_de_donnees[2].num_badge = 0x3;
    base_de_donnees[2].droits = 2;   // 只能还
    base_de_donnees[2].id_pince_coupante = 3; // 假设借了 3 号剪线钳
	base_de_donnees[1].id_pince_denude = 4;
    base_de_donnees[2].tick_emprunt_coupante = 20; 
	base_de_donnees[2].tick_emprunt_denude = 40;
}

void BuildTrameNeo(void *pvParameters) {
    (void)pvParameters;
    int i, bit;
    int idx;
    
    while(1)
    {
        // 阻塞任务，等待来自 TIM3_IRQHandler 的通知
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if(xSemaphoreTake(pxLED_MUTEX, portMAX_DELAY) == pdPASS)
        {
            idx = 0;
            for(i = 0; i < 32; i++) {
                uint32_t green = LED[i][1];
                uint32_t red   = LED[i][0];
                uint32_t blue  = LED[i][2];

                for(bit = 7; bit >= 0; bit--) TRAME_NEO[idx++] = (green & (1 << bit)) ? 70 : 30;
                for(bit = 7; bit >= 0; bit--) TRAME_NEO[idx++] = (red & (1 << bit)) ? 70 : 30;
                for(bit = 7; bit >= 0; bit--) TRAME_NEO[idx++] = (blue & (1 << bit)) ? 70 : 30;
            }
            TRAME_NEO[idx] = 0; 
            
            xSemaphoreGive(pxLED_MUTEX);
            
            index_pwm = 0; 
            TIM2->DIER |= TIM_DIER_UIE; 
        }
        // 删除原来的 vTaskDelay(pdMS_TO_TICKS(100)); 
    }
}

void Update_74HCT573(uint32_t data) {
    uint8_t parts[4] = {
        data & 0xFF,          // 第 1 块 (1-8号电机)
        (data >> 8) & 0xFF,   // 第 2 块 (9-16号电机)
        (data >> 16) & 0xFF,  // 第 3 块 (17-24号电机)
        (data >> 24) & 0xFF   // 第 4 块 (25-32号电机)
    };

    for(int i = 0; i < 4; i++) {
        // 1. 输出 8 位数据到 PC0~PC7，同时保持 GPIOC 其他高位引脚状态不变
        GPIOC->ODR = (GPIOC->ODR & 0xFF00) | parts[i];
        
        // 2. 制造一个 LE 脉冲在 GPIOA 上 (拉高 -> 稍微等一下 -> 拉低)
        // 锁存引脚分别是 PA4, PA5, PA6, PA7，所以是 4 + i
        GPIOA->BSRR = (1 << (4 + i));       // 拉高，打开玻璃罩
        __NOP(); __NOP();                   // 等待数据锁存
        GPIOA->BSRR = (1 << (4 + i + 16));  // 拉低，关上玻璃罩
    }
}

uint32_t Read_74HC251_Inputs(void) {
    uint32_t etat_capteurs = 0; // 这是一个 32 位的“记事本”，用来记录 32 把钳子的状态

    // 我们有 8 个通道（0~7），单片机要循环 8 次去“点名”
    for (int i = 0; i < 8; i++) {
        
        // --- 业务动作 1：单片机“点名” ---
        // 逻辑：告诉 4 个摄像头：“请把你们第 i 号槽位的状态传过来！”
        // 做法：通过控制 PB8, PB9, PB10 这三个输出引脚，把 i 的值发出去。
        // 请在此处填补操作 GPIOB->ODR 的代码：
        GPIOB->ODR = (GPIOB->ODR & ~(0x07 << 8)) | ((i & 0x07) << 8);
        
        // 等待摄像头切换画面
        __NOP(); __NOP(); __NOP(); 

        // --- 业务动作 2：单片机“看画面” ---
        // 逻辑：一次性读取整个 GPIOB 端口的输入电平。
        uint32_t port_in = GPIOB->IDR;

        // --- 业务动作 3：单片机“做记录” ---
        // 逻辑：
        // PB12 传回的是第 1 个摄像头（管 0~7 号钳子）的画面，把它记在记事本的第 i 位。
        // PB13 传回的是第 2 个摄像头（管 8~15 号钳子）的画面，把它记在记事本的第 i+8 位。
        // PB14 传回的是第 3 个摄像头（管 16~23 号钳子）的画面，记在第 i+16 位。
        // PB15 传回的是第 4 个摄像头（管 24~31 号钳子）的画面，记在第 i+24 位。
        
        if (port_in & (1 << 12)) etat_capteurs |= (1 << i);         // 0~7号
        if (port_in & (1 << 13)) etat_capteurs |= (1 << (i + 8));   // 8~15号
        if (port_in & (1 << 14)) etat_capteurs |= (1 << (i + 16));  // 16~23号
        if (port_in & (1 << 15)) etat_capteurs |= (1 << (i + 24));  // 24~31号
    }
    
    // 把记满 32 把钳子状态的记事本交出去，给“借出任务”去判断！
    return etat_capteurs;
}

void gestion_lecture_TAG_OSe(void)
{ ONEWIRE_RESET_OS(); // A EXTRAIRE POUR DISTINGUER BADGE PRESENT ABSENT
	ONEWIRE_ENVOI_OCTET_OS(0x33);	
	identifiant[0]=ONEWIRE_READ_OCTET_OS();
	identifiant[1]=ONEWIRE_READ_OCTET_OS();
	identifiant[2]=ONEWIRE_READ_OCTET_OS();
	identifiant[3]=ONEWIRE_READ_OCTET_OS();
	identifiant[4]=ONEWIRE_READ_OCTET_OS();
	identifiant[5]=ONEWIRE_READ_OCTET_OS();
	identifiant[6]=ONEWIRE_READ_OCTET_OS();
	identifiant[7]=ONEWIRE_READ_OCTET_OS();
}

