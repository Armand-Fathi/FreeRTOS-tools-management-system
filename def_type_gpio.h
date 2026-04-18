#pragma once
#include "stm32f10x.h"
// Entrées
#define GPIO_MODE_INPUT_ANALOG        0x0  // Entrée analogique
#define GPIO_MODE_INPUT_FLOATING      0x4  // Entrée flottante
#define GPIO_MODE_INPUT_PULL_UP_DOWN  0x8  // Entrée avec pull-up/pull-down

// Sorties en mode push-pull
#define GPIO_MODE_OUTPUT_PP_10MHz     0x1  // Sortie push-pull 10 MHz
#define GPIO_MODE_OUTPUT_PP_2MHz      0x2  // Sortie push-pull 2 MHz
#define GPIO_MODE_OUTPUT_PP_50MHz     0x3  // Sortie push-pull 50 MHz

// Sorties en mode open-drain
#define GPIO_MODE_OUTPUT_OD_10MHz     0x5  // Sortie open-drain 10 MHz
#define GPIO_MODE_OUTPUT_OD_2MHz      0x6  // Sortie open-drain 2 MHz
#define GPIO_MODE_OUTPUT_OD_50MHz     0x7  // Sortie open-drain 50 MHz

// Sorties en mode fonction alternative push-pull
#define GPIO_MODE_AF_PP_10MHz         0x9  // Fonction alternative push-pull 10 MHz
#define GPIO_MODE_AF_PP_2MHz          0xA  // Fonction alternative push-pull 2 MHz
#define GPIO_MODE_AF_PP_50MHz         0xB  // Fonction alternative push-pull 50 MHz

// Sorties en mode fonction alternative open-drain
#define GPIO_MODE_AF_OD_10MHz         0xD  // Fonction alternative open-drain 10 MHz
#define GPIO_MODE_AF_OD_2MHz          0xE  // Fonction alternative open-drain 2 MHz
#define GPIO_MODE_AF_OD_50MHz         0xF  // Fonction alternative open-drain 50 MHz

void init_GPIOx(GPIO_TypeDef *GPIOx, uint8_t num_bit, uint8_t quartet);
