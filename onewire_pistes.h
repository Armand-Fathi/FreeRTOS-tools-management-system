#pragma once
void Timer1_Init(void);
void ONEWIRE_RESET_OS(void);
void ONEWIRE_WRITE_BIT_OS(unsigned char x);
unsigned char ONEWIRE_READ_BIT_OS(void);
void ONEWIRE_ENVOI_OCTET_OS(unsigned char in);
unsigned char ONEWIRE_READ_OCTET_OS(void);
