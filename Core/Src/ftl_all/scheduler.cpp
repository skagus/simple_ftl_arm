#include <atomic>
#include "types.h"
#include "sim.h"
#include "cpu.h"
//#include "timer.h"
#include "macro.h"
#include "scheduler.h"
#include "scheduler_conf.h"
#define DBG_SCHEDULER		(0)

typedef struct
{
	Entry	pfTask;
	uint32  nTimeOut;
	Evts    bmWaitEvt;         // Events to wait...
	void*	pParam;
#if (DBG_SCHEDULER)
	uint32	nCntRun;
#endif
} TaskInfo;

struct Sched
{
	TaskInfo astTask[NUM_TASK];

	TaskBtm bmRdyTask;	///< Ready to run.
	Evts bmSyncEvt;	///< Sync envent that isn't handled.
	uint32 nCurTick;
	uint32 nCurTask;		///< Current running task.
	TaskBtm abmModeBackup[NUM_MODE];
	TaskBtm bmCurRunnable;
	RunMode geRunMode;
} gSched;

//std::atomic_flag gnIntEnable = ATOMIC_FLAG_INIT;

volatile Evts bmAsyncEvt;	///< ISR에서 등록된 event bitmap.
volatile uint32 nAsyncTick;	///< Running중 호출된 Tick ISR의 횟수.

void sched_TickISR(uint32 tag, uint32 result)
{
	nAsyncTick++;
	gSched.nCurTick++;
}

/**
Get async event into scheduler.
This function called under interrupt disabled.
@return need to run.
*/
static TaskBtm sched_HandleEvt(Evts bmEvt, uint32 nTick)
{
	TaskBtm bmNewRdy = 0;
	for (uint32 nTaskId = 0; nTaskId < NUM_TASK; nTaskId++)
	{
		TaskInfo* pTask = gSched.astTask + nTaskId;
		if (pTask->bmWaitEvt & bmEvt)
		{
			bmNewRdy |= BIT(nTaskId);
			pTask->bmWaitEvt &= ~bmEvt;
			pTask->nTimeOut = 0;
		}
		else if ((nTick > 0) && (pTask->nTimeOut > 0))
		{
			if (pTask->nTimeOut <= nTick)
			{
				pTask->bmWaitEvt = 0;
				pTask->nTimeOut = 0;
				bmNewRdy |= BIT(nTaskId);
			}
			else
			{
				pTask->nTimeOut -= nTick;
			}
		}
	}
	return bmNewRdy;
}


/**
* This function is called by ISR.
* Just set async event.
*/
void Sched_TrigAsyncEvt(Evts bmEvt)
{
//	while (gnIntEnable.test_and_set());
	bmAsyncEvt |= bmEvt;
//	gnIntEnable.clear();
}

/**
* This function is called by non-ISR.
* Just set sync event.
*/
void Sched_TrigSyncEvt(Evts bmEvt)
{
	gSched.bmSyncEvt |= bmEvt;
}

/**
* Wait events till nTime.
*/
void Sched_Wait(Evts bmEvt, uint32 nTime)
{
	if (0 == bmEvt && 0 == nTime)
	{
		gSched.bmRdyTask |= BIT(gSched.nCurTask);
	}
	gSched.astTask[gSched.nCurTask].bmWaitEvt = bmEvt;
	gSched.astTask[gSched.nCurTask].nTimeOut = nTime;
}

/*
* Register Task.
* Task ID is pre-defined.
*/
void Sched_Register(uint32 nTaskID, Entry task, void* pParam, uint32 bmRunMode) ///< Register tasks.
{
	TaskInfo* pTI = gSched.astTask + nTaskID;
	pTI->pfTask = task;
	pTI->pParam = pParam;
	gSched.bmRdyTask |= BIT(nTaskID);

	for (uint8 eMode = 0; eMode < NUM_MODE; eMode++)
	{
		if (bmRunMode & BIT(eMode))
		{
			gSched.abmModeBackup[eMode] |= BIT(nTaskID);
		}
	}
}

void Sched_SetMode(RunMode eMode)
{
	gSched.bmCurRunnable = gSched.abmModeBackup[eMode];
	gSched.geRunMode = eMode;
}

RunMode Sched_GetMode()
{
	return gSched.geRunMode;
}


void Sched_Init()
{
	gSched.nCurTask = 0;
	gSched.geRunMode = RunMode::MODE_NORMAL;
//	gnIntEnable.clear();

	gSched.bmSyncEvt = 0;
	bmAsyncEvt = 0;
	nAsyncTick = 0;
	gSched.nCurTick = 0;
	gSched.bmRdyTask = 0;

//	TMR_Init();
//	TMR_Add(0, SIM_MSEC(MS_PER_TICK), sched_TickISR, true);
}

/**
* Run all ready tasks.
*/
void Sched_Run()
{
	Sched_SetMode(MODE_NORMAL);
	while (true)
	{
		// disable interrupt.
//		while (gnIntEnable.test_and_set());
		Evts bmEvt = bmAsyncEvt;
		bmAsyncEvt = 0;
//		gnIntEnable.clear();
		uint32 nTick = nAsyncTick;
		nAsyncTick = 0;
		// enable interrupt.
		bmEvt |= gSched.bmSyncEvt;
		gSched.bmSyncEvt = 0;
		TaskBtm bmRdy = gSched.bmRdyTask | sched_HandleEvt(bmEvt, nTick);
		gSched.bmRdyTask = 0;
		if (bmRdy != 0)
		{
			while (bmRdy & gSched.bmCurRunnable)
			{
				if (BIT(gSched.nCurTask) & bmRdy & gSched.bmCurRunnable)
				{
					TaskInfo* pTask = gSched.astTask + gSched.nCurTask;
					pTask->bmWaitEvt = 0;
					pTask->pfTask(pTask->pParam);	// paramter is triggered event.
					CPU_TimePass(SIM_USEC(50));
#if DBG_SCHEDULER
					Evts bmEvt = pTask->bmWaitEvt;
					pTask->nCntRun++;
					ASSERT(pTask->bmWaitEvt || pTask->nTimeOut || (gSched.bmRdyTask & BIT(gSched.nCurTask)));
#endif
					bmRdy &= ~BIT(gSched.nCurTask);
				}
				gSched.nCurTask = (gSched.nCurTask == NUM_TASK-1) ? 0 : gSched.nCurTask + 1;
			}
			// Mode에 따라 실행되지 않은 task는 이후에 mode가 복귀했을 때 실행할 것.
			gSched.bmRdyTask |= bmRdy;
		}
		else
		{
			ASSERT(false);
		}
	}
}


/****************************
	while(true)
	{
		__disable_interrupt();
		bmRdy = sched_HandleEvt();
		if(0 == bmRdy)
		{
//			port_out(0, BIT(5));
			asm("WFI"); // Stop core with interrupt enabled.
//			port_out(0, 0);
			continue;
		}
		__enable_interrupt();

		Sched_Run();
	}
****************************/

