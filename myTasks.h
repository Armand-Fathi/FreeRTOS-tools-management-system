#pragma once
#include "FreeRTOS.h"
#include "Task.h"
#include "services.h"

void vInit_myTasks( UBaseType_t uxPriority );

// 任务句柄
extern TaskHandle_t xTask_iButton_Handle;
extern TaskHandle_t xTask_Emprunt_Coupante_Handle;
extern TaskHandle_t xTask_Emprunt_Denude_Handle;
extern TaskHandle_t xTask_Retour_Coupante_Handle;
extern TaskHandle_t xTask_Retour_Denude_Handle;
extern TaskHandle_t xTask_NeoPixel_Handle;

// 会话锁
extern volatile int session_en_cours;
