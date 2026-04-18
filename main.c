#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "myTasks.h"

int main(void){	
	Init_Database_Test();
	vInit_myTasks(10);
}
