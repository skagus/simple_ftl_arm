
#pragma once

#include "sim_conf.h"
#include "types.h"
#include "macro.h"

//////////////////////////////////////

#if defined(EN_SIM)
void SIM_PowerDown();
uint64 SIM_GetTick();
uint32 SIM_GetCycle();
void SIM_Print(const char *szFormat, ...);

uint32 SIM_GetRand(uint32 nMod);
uint32 SIM_GetSeqNo();

void SIM_Init(uint32 nSeed, uint32 nBrkNo = 0);
void SIM_Run();	// infinite running.
#else
#define SIM_PowerDown(...)
#define SIM_GetTick(...)			(0)
#define SIM_GetCycle(...)			(0)
#define SIM_Print(...)

#define SIM_GetRand(...)			(0)
#define SIM_GetSeqNo(...)			(0)
#endif
