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
	uint16  nTimeOut;
	Evts    bmWaitEvt;         // Events to wait...
	void*	pParam;
#if (DBG_SCHEDULER)
	uint32	nCntRun;
#endif
} TaskInfo;

TaskInfo astTask[NUM_TASK];

uint8 nCurTask;		///< Current running task.
TaskBtm bmRdyTask;	///< Ready to run.

TaskBtm gabmModeRun[NUM_MODE];
RunMode geRunMode;
std::atomic_flag gnIntEnable = ATOMIC_FLAG_INIT;

Evts bmSyncEvt;	///< Sync envent that isn't handled.
volatile Evts bmAsyncEvt;	///< ISR에서 등록된 event bitmap.
volatile uint16 nAsyncTick;	///< Running중 호출된 Tick ISR의 횟수.
volatile uint32 nCurTick;

void sched_TickISR(uint32 tag, uint32 result)
{
	nAsyncTick++;
	nCurTick++;
}

/**
Get async event into scheduler.
This function called under interrupt disabled.
@return need to run.
*/
TaskBtm sched_HandleEvt(Evts bmEvt, uint16 nTick)
{
	TaskBtm bmNewRdy = 0;
	for (uint8 nTaskId = 0; nTaskId < NUM_TASK; nTaskId++)
	{
		TaskInfo* pTask = astTask + nTaskId;
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
	while (gnIntEnable.test_and_set());
	bmAsyncEvt |= bmEvt;
	gnIntEnable.clear();
}

/**
* This function is called by non-ISR.
* Just set sync event.
*/
void Sched_TrigSyncEvt(Evts bmEvt)
{
	bmSyncEvt |= bmEvt;
}

/**
* Wait events till nTime.
*/
void Sched_Wait(Evts bmEvt, uint16 nTime)
{
	if (0 == bmEvt && 0 == nTime)
	{
		bmRdyTask |= BIT(nCurTask);
	}
	astTask[nCurTask].bmWaitEvt = bmEvt;
	astTask[nCurTask].nTimeOut = nTime;
}

/*
* Register Task.
* Task ID is pre-defined.
*/
void Sched_Register(uint8 nTaskID, Entry task, void* pParam, uint8 bmRunMode) ///< Register tasks.
{
	TaskInfo* pTI = astTask + nTaskID;
	pTI->pfTask = task;
	pTI->pParam = pParam;
	bmRdyTask |= BIT(nTaskID);

	for (uint8 eMode = 0; eMode < NUM_MODE; eMode++)
	{
		if (bmRunMode & BIT(eMode))
		{
			gabmModeRun[eMode] |= BIT(nTaskID);
		}
	}
}

void Sched_SetMode(RunMode eMode)
{
	geRunMode = eMode;
}

RunMode Sched_GetMode()
{
	return geRunMode;
}


void Sched_Init()
{
	nCurTask = 0;
	geRunMode = RunMode::MODE_NORMAL;
	gnIntEnable.clear();

	bmSyncEvt = 0;
	bmAsyncEvt = 0;
	nAsyncTick = 0;
	nCurTick = 0;
	bmRdyTask = 0;

//	TMR_Init();
//	TMR_Add(0, SIM_MSEC(MS_PER_TICK), sched_TickISR, true);
}

/**
* Run all ready tasks.
*/
void Sched_Run()
{
	while (true)
	{
		// disable interrupt.
		while (gnIntEnable.test_and_set());
		Evts bmEvt = bmAsyncEvt;
		bmAsyncEvt = 0;
		gnIntEnable.clear();
		uint16 nTick = nAsyncTick;
		nAsyncTick = 0;
		// enable interrupt.
		bmEvt |= bmSyncEvt;
		bmSyncEvt = 0;
		TaskBtm bmRdy = bmRdyTask | sched_HandleEvt(bmEvt, nTick);
		bmRdyTask = 0;
		if (bmRdy != 0)
		{
			while (bmRdy & gabmModeRun[geRunMode])
			{
				if (BIT(nCurTask) & bmRdy & gabmModeRun[geRunMode])
				{
					TaskInfo* pTask = astTask + nCurTask;
					Evts bmEvt = pTask->bmWaitEvt;
					pTask->bmWaitEvt = 0;
					pTask->pfTask(pTask->pParam);	// paramter is triggered event.
					CPU_TimePass(SIM_USEC(50));
#if DBG_SCHEDULER
					pTask->nCntRun++;
					ASSERT(pTask->bmWaitEvt || pTask->nTimeOut || (bmRdyTask & BIT(nCurTask)));
#endif
					bmRdy &= ~BIT(nCurTask);
				}
				nCurTask = (nCurTask + 1) % NUM_TASK;
			}
			// Mode에 따라 실행되지 않은 task는 이후에 mode가 복귀했을 때 실행할 것.
			bmRdyTask |= bmRdy;
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

