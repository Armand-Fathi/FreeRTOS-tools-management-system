#pragma once
#include "pistes_neopixel.h"
#include "onewire_pistes.h"
#include "sm.h"
#include "sensors.h"
//传入xTaskCreate()的是word数，不是byte数，32位机器中1word==4bytes
#define stack_BuildTrameNeo_size 128
#define stack_Task_iButton_size 128
#define nombre_etudiant 20
extern uint32_t LED[32][3];
extern uint32_t TRAME_NEO[769];
extern int identifiant[8];
typedef struct {
    uint64_t num_badge;        // 64位卡号
    uint8_t droits;            // 权限：1=autorisé, 2=attente rendu, 0=bloqué
    uint8_t id_pince_coupante; // 借出的剪线钳编号(1-16), 0=无
    uint8_t id_pince_denude;   // 借出的剥线钳编号(17-32), 0=无
    uint32_t tick_emprunt_coupante; // 剪线钳借出时间
    uint32_t tick_emprunt_denude;   // 剥线钳借出时间
} BadgeData;
extern BadgeData base_de_donnees[nombre_etudiant];
extern int index_utilisateur_courant; // 记录当前是谁在操作
void InitSystem(void);
void Init_Database_Test(void);
void Update_74HCT573(uint32_t data);
void BuildTrameNeo(void *pvParameters);
uint32_t Read_74HC251_Inputs(void);
void gestion_lecture_TAG_OSe(void);
