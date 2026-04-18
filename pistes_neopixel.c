#include <stdlib.h>
#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "services.h"
#include "def_type_gpio.h"

uint32_t index_pwm = 0;
void TIM2_IRQHandler(void) { //TIM2¸øLEDÓÃµÄÖÐ¶Ï
	TIM2->SR &= ~TIM_SR_UIF;  // Effacer le flag d'interruption (UIF)
 
	TIM2->CCR1 = TRAME_NEO[index_pwm++];//choix du rapport cyclique en fonction du bit
  
	if(index_pwm==769){
		TIM2->DIER &= ~TIM_DIER_UIE;  
		index_pwm=0;
	}
}

#define TIM_CCMR1_OC1M_PWM1 6<<4
void init_timer2(void) { //TIM2³õÊ¼»¯
    // Activer l'horloge du timer 2
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
	 init_GPIOx(GPIOA, 0, GPIO_MODE_AF_PP_50MHz);  // Choisissez une vitesse appropriée (10 MHz, 2 MHz, ou 50 MHz)

  	TIM2->CR1 &= ~TIM_CR1_CEN;  // Assurez-vous que le timer est arrêté avant de le configurer
    // Configuration du prescaler et de la période
    TIM2->PSC = 0;            // Pas de prescaler
    TIM2->ARR = 100;       // Période de 1,25 µs (arrondi à 88)
    TIM2->EGR = TIM_EGR_UG; // Générer un update event immédiatement

    // Mode PWM sur le canal 1
    TIM2->CCMR1 &= ~TIM_CCMR1_OC1M; // Effacer le mode actuel
    TIM2->CCMR1 |= TIM_CCMR1_OC1M_PWM1; // Mode PWM1 (le rapport cyclique détermine la largeur d'impulsion)
    // Activer le préchargement de comparaison sur le canal 1
    TIM2->CCMR1 |= TIM_CCMR1_OC1PE; // Active la précharge du registre CCR1
    // Le rapport cyclique est actualisé au reload de la période
    // Activer la sortie PWM sur le canal 1
    TIM2->CCER |= TIM_CCER_CC1E; // Activer la sortie du canal 1
  // Définir le rapport cyclique à 0% 
    TIM2->CCR1 = 0;  // 0% de la période (debut reset)

 // Configurer IT TIMER

    TIM2->DIER |= TIM_DIER_UIE; //  Activer IT Timer
		NVIC_SetPriority(TIM2_IRQn, 7); //niveau a definir
    NVIC_EnableIRQ(TIM2_IRQn); // Activer l'interruption TIM2

    // Démarrer le timer
    TIM2->CR1 |= TIM_CR1_CEN; // Activer le timer
		
}
