#include "stm32f10x.h"
#include "def_type_gpio.h"

void InitSensorsGPIO(){
	init_GPIOx(GPIOB,8,GPIO_MODE_OUTPUT_PP_50MHz);
	init_GPIOx(GPIOB,9,GPIO_MODE_OUTPUT_PP_50MHz);
	init_GPIOx(GPIOB,10,GPIO_MODE_OUTPUT_PP_50MHz);
	
	init_GPIOx(GPIOB,12,GPIO_MODE_INPUT_FLOATING);
	init_GPIOx(GPIOB,13,GPIO_MODE_INPUT_FLOATING);
	init_GPIOx(GPIOB,14,GPIO_MODE_INPUT_FLOATING);
	init_GPIOx(GPIOB,15,GPIO_MODE_INPUT_FLOATING);
	
	init_GPIOx(GPIOB, 5, GPIO_MODE_OUTPUT_PP_2MHz); // LedIHM_R
	init_GPIOx(GPIOB, 6, GPIO_MODE_OUTPUT_PP_2MHz); // LedIHM_V
	init_GPIOx(GPIOB, 7, GPIO_MODE_OUTPUT_PP_2MHz); // LedIHM_B
}

// 在 sensors.c 中添加这个函数
void InitButtonsGPIO(void){
    // 初始化 5 个按键的引脚为输入模式 (这里假设使用带上下拉的输入模式)
    // ！！！请把下面的 GPIOB 和 0~4 换成你实际的引脚 ！！！
    init_GPIOx(GPIOB, 0, GPIO_MODE_INPUT_PULL_UP_DOWN); 
    init_GPIOx(GPIOB, 1, GPIO_MODE_INPUT_PULL_UP_DOWN);
    init_GPIOx(GPIOB, 2, GPIO_MODE_INPUT_PULL_UP_DOWN);
    init_GPIOx(GPIOB, 3, GPIO_MODE_INPUT_PULL_UP_DOWN);
    init_GPIOx(GPIOB, 4, GPIO_MODE_INPUT_PULL_UP_DOWN);
    
    // 如果硬件电路上没有外接上拉电阻，需要在单片机内部开启上拉：
    // 向 ODR 寄存器对应的位写 1，即可激活上拉电阻（保证没按下时是稳定的高电平 1）
    GPIOB->ODR |= (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4);
}
