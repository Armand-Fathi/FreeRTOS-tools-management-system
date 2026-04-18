#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "def_type_gpio.h"
#include "Task.h"
#include "myTasks.h"

static int etat_onewire=1;

// ---------------- ²¹³äÈ±Ê§µÄ¶¨ÒåºÍº¯Êý ----------------

// ¶¨Òå 1Î¢Ãë ¶ÔÓ¦µÄ¶¨Ê±Æ÷¼ÆÊýÖµ
// ÒòÎªÔ¤·ÖÆµÆ÷ PSC = 71£¬ËùÒÔ 1 µÎ´ð = 1 Î¢Ãë
#define TIM1CLK_1US 1

// Æô¶¯¶¨Ê±Æ÷²úÉú 1-Wire Ê±ÐòÂö³åµÄµ×²ãÇý¶¯º¯Êý
void Timer1_Start(uint16_t temps_bas, uint16_t temps_total, uint16_t temps_echantillon) {
    // 1. ÉèÖÃ CH1£º¿ØÖÆ×ÜÏßÀ­µÍµÄÊ±¼ä (·¢Âö³å)
    TIM1->CCR1 = temps_bas;
    
    // 2. ÉèÖÃ CH2£º¿ØÖÆÕû¸öÖÜÆÚµÄ×ÜÊ±¼ä (µ½Ê±¼ä½ø CH2 ÖÐ¶Ï»½ÐÑÈÎÎñ)
    TIM1->CCR2 = temps_total;
    
    // 3. ÉèÖÃ CH3£º¿ØÖÆ²ÉÑùµÄÊ±¼äµã (µ½Ê±¼ä½ø CH3 ÖÐ¶Ï¶ÁÒý½Å×´Ì¬)
    TIM1->CCR3 = temps_echantillon;
    
    // 4. ÇåÁã¼ÆÊýÆ÷ºÍÖÐ¶Ï±êÖ¾Î»£¬·ÀÖ¹Ò»Æô¶¯¾ÍÁ¢¿ÌÎó´¥·¢ÀÏµÄÖÐ¶Ï
    TIM1->CNT = 0;
    TIM1->SR = 0;
    
    // 5. ¿Û¶¯°â»ú£¬Æô¶¯¶¨Ê±Æ÷£¡
    TIM1->CR1 |= TIM_CR1_CEN;
}

// --------------------------------------------------------

// Fonction d'initialisation GPIOA
void init_gpioA(unsigned char num_bit, unsigned int quartet_config) {
    // Calculez la position du quartet de bits à configurer dans CRL ou CRH
    unsigned char bit_ref = (num_bit * 4) & 31;
    // Activer l'horloge pour GPIOA : Bit 2 dans RCC->APB2ENR doit être mis à 1
    RCC->APB2ENR |= (1 << 2);
    // Limiter quartet_config à 4 bits
    quartet_config &= 0xF;
    // Configurer le registre CRL si le numéro de bit est inférieur à 8
    if (num_bit < 8) {
        // Effacer les anciens bits du quartet correspondant dans CRL
        GPIOA->CRL &= ~(0xF << bit_ref);
        // Configurer les nouveaux bits du quartet dans CRL
        GPIOA->CRL |= (quartet_config << bit_ref);
    } else {
        // Effacer les anciens bits du quartet correspondant dans CRH
        GPIOA->CRH &= ~(0xF << bit_ref);
        // Configurer les nouveaux bits du quartet dans CRH
        GPIOA->CRH |= (quartet_config << bit_ref);
    }
}

// ATTENTION CE N EST PAS LE MEME TIMER QUE CELUI DU TP DE UC2
// ATTENTION CE NE SONT PAS LES MEME CANAUX QU EN TP DE UC2

void TIM1_CC_IRQHandler(void) { //¸øibuttonÓÃµÄTIM1ÖÐ¶Ï(1-Wire ¶Á¿¨Æ÷)
	//  ON NE COMMUNIQUERA PAS PAR FLAG MAIS PAR NOTIFICATION FROM ISR
    // Interruption CH2 (arrêt du timer) IT fin motif
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (TIM1->SR & TIM_SR_CC2IF) {
        TIM1->SR &= ~TIM_SR_CC2IF;  // Effacer le flag CH2
        TIM1->CR1 &= ~TIM_CR1_CEN; // Arrêter le timer
        // Action à effectuer après CH2 (arrêt du timer)
			 GPIOA->BSRR = 0x01<< (9+16) ;//effacement PA9 
		if(xTask_iButton_Handle != NULL) {
            vTaskNotifyGiveFromISR(xTask_iButton_Handle, &xHigherPriorityTaskWoken);
        }
    }

    // Interruption CH3 (indépendante)
    if (TIM1->SR & TIM_SR_CC3IF) {
        TIM1->SR &= ~TIM_SR_CC3IF;  // Effacer le flag CH3
        // Action à effectuer après CH3
			 GPIOA->BSRR = 0x04<< (9+16) ;//effacement  PA11
			 if(GPIOA->IDR & (1<<8)) {etat_onewire=1;} else {etat_onewire=0;}
			
    }
	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void Timer1_Init(void) {  //TIM1³õÊ¼»¯
    // Activer l'horloge pour Timer 1 et GPIOA
    RCC->APB2ENR |= RCC_APB2ENR_TIM1EN ;
    
    // Configurer PA8 en collecteur ouvert
	   init_gpioA(8, 0x0D); //fonction alternate  pour CH1 collecteur ouvert mettre pull up externe
     init_gpioA(9, 3); //sortie espion
     init_gpioA(10, 9); //quartet 9 pour config CRH en fonction alternate pour CH3  push pull
     init_gpioA(11, 3); //sortie espion
	   GPIOA->BSRR = 0x05<< (9+16) ;//effacement PA9 et PA11
    // Configurer la fréquence du timer
	  TIM1->CR1 &= ~TIM_CR1_CEN;
    TIM1->PSC = 71; // Divise l'horloge système à 1MHz (72 MHz / 72)
    TIM1->ARR = 2000;//0xFFFF;
	  TIM1->CNT =0x0000;
	  TIM1->EGR = TIM_EGR_UG;// permet de générer l evenement de reload
    // Configurer CH1 (Output Compare pour le pulse niveau bas)
    TIM1->CCMR1 &= ~TIM_CCMR1_OC1M; // Clear mode bits pour CH1
    TIM1->CCMR1 |= TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2; // Mode PWM1
    TIM1->CCER  |= TIM_CCER_CC1E;   // Activer la sortie CH1
    TIM1->CCER  |= TIM_CCER_CC1P;   // Polarité active bas

    // Configurer CH2 (One Pulse, arrête le timer à un certain temps)
    TIM1->CCMR1 &= ~TIM_CCMR1_OC2M; // Clear mode bits pour CH2
    TIM1->CCMR1 |= TIM_CCMR1_OC2M_1 | TIM_CCMR1_OC2M_2; // Mode PWM1
    TIM1->CCER |= TIM_CCER_CC2E;   // Activer la sortie CH2

    // Configurer CH3 (IT indépendante pour une durée définie)
    TIM1->CCMR2 &= ~TIM_CCMR2_OC3M; // Clear mode bits pour CH3
    TIM1->CCMR2 |= TIM_CCMR2_OC3M_1 | TIM_CCMR2_OC3M_2; // Mode PWM1
    TIM1->CCER |= TIM_CCER_CC3E;   // Activer la sortie CH3

    // Activer One Pulse Mode (OPM) pour CH2
    TIM1->CR1 |= TIM_CR1_OPM;
		

    // Activer le Main Output Enable pour permettre la sortie sur les pins
    TIM1->BDTR |= TIM_BDTR_MOE;
		
   // Activer les interruptions sur CH2 et CH3
    TIM1->DIER |= TIM_DIER_CC2IE | TIM_DIER_CC3IE;

    // Activer les interruptions Timer 1
    NVIC_EnableIRQ(TIM1_CC_IRQn);
}


//1-Wire×èÈûÐÍ¿ØÖÆº¯Êý£¬ÐèÒªÈ«²¿¸Ä³Énotification from IT
void ONEWIRE_RESET_OS(void){ 
	Timer1_Start( 500*TIM1CLK_1US,1000*TIM1CLK_1US,565*TIM1CLK_1US); 
		 
	ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
}
	 
void ONEWIRE_WRITE_BIT_OS(unsigned char x) 
   {
     if(x){Timer1_Start( 8 *TIM1CLK_1US,64*TIM1CLK_1US,100*TIM1CLK_1US);}
     else {Timer1_Start( 60*TIM1CLK_1US,64*TIM1CLK_1US,100*TIM1CLK_1US);}
	 ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
	 }	 
unsigned char ONEWIRE_READ_BIT_OS(void)
	  { 
			Timer1_Start( 8*TIM1CLK_1US,64*TIM1CLK_1US,13*TIM1CLK_1US);
		  ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
		return etat_onewire;
	  }
void ONEWIRE_ENVOI_OCTET_OS(unsigned char in)	
{ ONEWIRE_WRITE_BIT_OS(in&0x01);
  ONEWIRE_WRITE_BIT_OS(in&0x02);
  ONEWIRE_WRITE_BIT_OS(in&0x04);
  ONEWIRE_WRITE_BIT_OS(in&0x08);
	ONEWIRE_WRITE_BIT_OS(in&0x10);
	ONEWIRE_WRITE_BIT_OS(in&0x20);
	ONEWIRE_WRITE_BIT_OS(in&0x40);
	ONEWIRE_WRITE_BIT_OS(in&0x80);
}	
unsigned char ONEWIRE_READ_OCTET_OS(void)
{unsigned char rep=0;
	if(ONEWIRE_READ_BIT_OS()) {rep|=0x01;}	
	if(ONEWIRE_READ_BIT_OS()) {rep|=0x02;}
	if(ONEWIRE_READ_BIT_OS()) {rep|=0x08;}
	if(ONEWIRE_READ_BIT_OS()) {rep|=0x04;}
	if(ONEWIRE_READ_BIT_OS()) {rep|=0x10;}
	if(ONEWIRE_READ_BIT_OS()) {rep|=0x20;}
	if(ONEWIRE_READ_BIT_OS()) {rep|=0x40;}
	if(ONEWIRE_READ_BIT_OS()) {rep|=0x80;}

	return rep;
}



