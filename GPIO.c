#include "def_type_gpio.h"

void init_GPIOx(GPIO_TypeDef *GPIOx, uint8_t num_bit, uint8_t quartet) {
    // Vérifier si le numéro de bit est dans la plage valide (0-15)
    if (num_bit > 15) {return; }// Gestion d'erreur, le numéro de bit doit être entre 0 et 15

    RCC->APB2ENR |= (7 << 2);//pour  GPIOA GPIOB GPIOC
    // Déterminer si le bit est dans CRL ou CRH
    if (num_bit < 8) {
        // CRL pour les bits 0 ?7
        uint32_t mask = 0xF << (num_bit * 4);     // Masque pour effacer les 4 bits correspondants
        uint32_t value = (uint32_t)quartet << (num_bit * 4); // Valeur ?définir
        GPIOx->CRL = (GPIOx->CRL & ~mask) | value;
    } else {
        // CRH pour les bits 8 ?15
        uint32_t mask = 0xF << ((num_bit - 8) * 4);   // Masque pour effacer les 4 bits correspondants
        uint32_t value = (uint32_t)quartet << ((num_bit - 8) * 4); // Valeur ?définir
        GPIOx->CRH = (GPIOx->CRH & ~mask) | value;
    }
}
