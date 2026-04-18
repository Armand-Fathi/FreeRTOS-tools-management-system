#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "Task.h"
#include "myTasks.h"
#include "def_type_gpio.h"
#include "services.h"

// --- 舵机控制相关宏定义 (单位：微秒 us) ---
#define PERIODE_SERVO       20000  // 舵机标准周期 20ms
#define DUREE_FERME         1000   // 锁死时间 (1ms -> 0°)
#define DELTA_DUREE_OUVRE   500    // 开锁额外增加的时间差 (1.5ms - 1ms = 0.5ms)


// 记录 32 个锁的目标状态：每一位(bit)代表一个锁，0表示锁死，1表示打开
volatile uint32_t etat_verrous = 0x00000000; 

// 记录当前中断进行到了第几个阶段 (1, 2, 3)
volatile uint8_t phase_servo = 1; 

// 记录执行了多少个完整周期，用于每 5 次唤醒一次 Neopixel
volatile uint8_t compteur_cycle_servo = 0;

void TIM3_Init_Servos(void) {
    // 1. 开启 TIM3 时钟
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
    
    // 2. 初始化相关 GPIO (PB0~PB11) 为通用推挽输出 (代码略，调用你写好的 init_GPIOx 即可)
    init_GPIOx(GPIOA, 4, GPIO_MODE_OUTPUT_PP_50MHz);
	init_GPIOx(GPIOA, 5, GPIO_MODE_OUTPUT_PP_50MHz);
	init_GPIOx(GPIOA, 6, GPIO_MODE_OUTPUT_PP_50MHz);
	init_GPIOx(GPIOA, 7, GPIO_MODE_OUTPUT_PP_50MHz);
	
	init_GPIOx(GPIOC, 0, GPIO_MODE_OUTPUT_PP_50MHz);
	init_GPIOx(GPIOC, 1, GPIO_MODE_OUTPUT_PP_50MHz);
	init_GPIOx(GPIOC, 2, GPIO_MODE_OUTPUT_PP_50MHz);	
	init_GPIOx(GPIOC, 3, GPIO_MODE_OUTPUT_PP_50MHz);
	init_GPIOx(GPIOC, 4, GPIO_MODE_OUTPUT_PP_50MHz);
	init_GPIOx(GPIOC, 5, GPIO_MODE_OUTPUT_PP_50MHz);
	init_GPIOx(GPIOC, 6, GPIO_MODE_OUTPUT_PP_50MHz);
	init_GPIOx(GPIOC, 7, GPIO_MODE_OUTPUT_PP_50MHz);
	
    
    // 3. 配置定时器基础参数
    TIM3->CR1 &= ~TIM_CR1_CEN;   // 先关闭定时器
    TIM3->PSC = 71;              // 72MHz / (71+1) = 1MHz -> 1个 tick 是 1us
    TIM3->ARR = PERIODE_SERVO;           // 初始周期设为 20ms
    TIM3->EGR = TIM_EGR_UG;      // 强制更新
    
    // 4. 配置并开启 TIM3 的更新中断 (Update Interrupt)
    TIM3->DIER |= TIM_DIER_UIE;  
    
    // 设置中断优先级 (注意：FreeRTOS环境下，优先级必须受控，比如设为 5 到 15 之间)
    NVIC_SetPriority(TIM3_IRQn, 6); 
    NVIC_EnableIRQ(TIM3_IRQn);
    
    // 5. 启动定时器
    TIM3->CR1 |= TIM_CR1_CEN;
}

void TIM3_IRQHandler(void) {
    // 操作系统要求的：用于标记是否需要进行任务切换
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (TIM3->SR & TIM_SR_UIF) {
        TIM3->SR &= ~TIM_SR_UIF; // 清除中断标志位

        if (phase_servo == 1) {
            Update_74HCT573(0xFFFFFFFF); 
            TIM3->ARR = DUREE_FERME; 
            phase_servo = 2;
        } 
        else if (phase_servo == 2) {
            Update_74HCT573(~etat_verrous); 
            TIM3->ARR = DELTA_DUREE_OUVRE; 
            phase_servo = 3; 
        } 
        else if (phase_servo == 3) {
            Update_74HCT573(0x0); 
            TIM3->ARR = PERIODE_SERVO - DELTA_DUREE_OUVRE - DUREE_FERME; 
            phase_servo = 1; 
            
            ++compteur_cycle_servo;
            if(compteur_cycle_servo == 5){
                compteur_cycle_servo = 0; // 别忘了把计数器清零！
                
                // 每5次中断，向 NeoPixel 任务发送通知将其唤醒 [cite: 184]
                if(xTask_NeoPixel_Handle != NULL) {
                    vTaskNotifyGiveFromISR(xTask_NeoPixel_Handle, &xHigherPriorityTaskWoken);
                }
            }
        }
    }
    // 如果唤醒了更高优先级的任务，立刻执行上下文切换
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}



