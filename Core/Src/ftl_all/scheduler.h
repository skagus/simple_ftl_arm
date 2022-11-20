
#include <stdint.h>
#include "types.h"
#include "scheduler_conf.h"


typedef uint32 TaskBtm;
typedef uint32 Evts;

typedef void(*Entry)(void* pParam);

void Sched_Init();
void Sched_Register(uint32 nTaskID, Entry pfTask, void* pParam, uint32 bmRunMode); ///< Register tasks.
void Sched_SetMode(RunMode eMode);	// Not used.
RunMode Sched_GetMode();	// Not used.
void Sched_Run();
void Sched_Wait(Evts bmEvt, uint32 nTick);
void Sched_TrigSyncEvt(Evts bmEvt);
void Sched_TrigAsyncEvt(Evts bmEvt);

#define Sched_Yield()	Sched_Wait(0L, 0L)

