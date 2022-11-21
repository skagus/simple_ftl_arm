//#include "coroutine.h"
#include "os.h"
#include "sim.h"
#include "cpu.h"
//#include "timer.h"

#define EN_DBG_OS	(0)
#if defined(__arm__)
typedef void* TCB;
#else
typedef SimTaskId TCB;
#endif
struct OS_Info
{
	uint32	_curTID;		///< 현재 해당 CPU에서 돌고있는 task ID.
	uint32	_bmRdyTask;		///< Ready task bitmap, (_aBlockBy보다 우선순위 높음)
	uint32	_nTick;					///< Just tick counter

	TCB		_aTCB[MAX_TASK];	///< TCB 상당의 무엇인데, sim에서는 fiber id와 대응됨.
	uint32	_aWaitEvt[MAX_TASK];	///< Task가 기다리는 event, wakeup상태에서도 의미있음.
	int32	_aExpire[MAX_TASK];		///< Remainning tick to ready.(only for tick event)
	uint8	_numTask;		///< Number of Task.

//	bool	_bInCritical;	///< The OS in critical section.(Never context switch in critical section)
	Task	_pfTask[MAX_TASK];	///< Task entry.
	void*	_pParam[MAX_TASK];	///< Task parameter.
	void*	pStkTop[MAX_TASK];
	const char* aszName[MAX_TASK];
#if (EN_DBG_OS)
	uint32	anCntSched[MAX_TASK];
#endif
} gstOS;


volatile uint32 gnAsyncTick;
volatile uint32 gbmAsyncEvt;

#if defined(__arm__)
void* pCurTCB;
#endif

void	os_handleAsyncEvt();
void	os_applyEvt(uint32 bmNewEvt);


static void os_SetNextTask()
{
//	ASSERT(!gstOS._bInCritical);
	uint32 nPrvTID = gstOS._curTID;
	uint32 nNxtTID = gstOS._curTID;

	os_handleAsyncEvt();
	uint32 bmRdy = gstOS._bmRdyTask;
	while (1)
	{
		++nNxtTID;
		if (nNxtTID >= gstOS._numTask)
		{
			nNxtTID = 0;
		}

		if (bmRdy & BIT(nNxtTID))
		{
			break;
		}
		else if (nPrvTID == nNxtTID)
		{
			ASSERT(0 == bmRdy);
			// Ready event가 하나도 없음.
			// interrupt를 기다려야 한다. (interrupt 대신 event)
			// interrupt대신 event를 더 자주 깨어나서 overhead증가하지만, 
			// simulation의 정확도 측면에선 차이가 없다.
#if defined(__arm__) 
			__asm__ __volatile__("wfi");
#else
			CPU_Sleep();
#endif
			os_handleAsyncEvt();
			bmRdy = gstOS._bmRdyTask;
		}
	}
	gstOS._curTID = nNxtTID;
#if (EN_DBG_OS)
	gstOS.anCntSched[nNxtTID]++;
#endif
#if defined(__arm__)
	pCurTCB = &(gstOS._aTCB[nNxtTID]);
#endif
}


uint8 OS_CreateTask(Task pfEntry, void* pStkTop, void* nParam, const char* szName)
{
	uint32 nTaskID = gstOS._numTask;
	ASSERT(nTaskID < MAX_TASK);
	gstOS._aExpire[nTaskID] = 0;
	gstOS._bmRdyTask |= BIT(nTaskID); // initial state = ready.
	gstOS._pfTask[nTaskID] = pfEntry;
	gstOS._pParam[nTaskID] = nParam;
	gstOS.aszName[nTaskID] = szName;
	gstOS.pStkTop[nTaskID] = (uint32*)pStkTop - 1;
	gstOS._numTask++;
	return nTaskID;
}


#if defined(__arm__)
void* os_InitStk(Task pfTask, void* pInStkTop, void* pParam)
{
	uint32* pStk = (uint32*)pInStkTop;

	pStk--; *pStk = (uint32)pfTask;	/* R14 */
#if 1
	pStk -= 12;
#else
	pStk--; *pStk = 0xAC;	/* R12 */
	pStk--; *pStk = 0xAB;	/* R11 */
	pStk--; *pStk = 0xAA;	/* R10 */
	pStk--; *pStk = 0xA9;	/* R9 */
	pStk--; *pStk = 0xA8;	/* R8 */
	pStk--; *pStk = 0xA7;	/* R7 */
	pStk--; *pStk = 0xA6;	/* R6 */
	pStk--; *pStk = 0xA5;	/* R5 */
	pStk--; *pStk = 0xA4;	/* R4 */
	pStk--; *pStk = 0xA3;	/* R3 */
	pStk--; *pStk = 0xA2;	/* R2 */
	pStk--; *pStk = 0xA1;	/* R1 */
#endif
	pStk--; *pStk = (uint32)pParam;	/* R0 */
	return pStk;
}

__attribute__((naked)) void os_SwitchForARM()
{
	__asm__ __volatile__("stmdb sp!, {r0-r12, r14}");
	__asm__ __volatile__("ldr r3, =pCurTCB");
	__asm__ __volatile__("ldr r2, [r3]");
	__asm__ __volatile__("str sp, [r2]");	// store SP to Active TCB.

	os_SetNextTask();	// Should change the value of pCurTCB.

	__asm__ __volatile__("ldr r3, =pCurTCB");
	__asm__ __volatile__("ldr r2, [r3]");
	__asm__ __volatile__("ldr sp, [r2]");	// load SP from Active TCB.
	__asm__ __volatile__("ldmia sp!, {r0-r12, r14}");
	__asm__ __volatile__("isb");

	__asm__ __volatile__("bx r14");
}
#endif
/**
등록된 Task를 모두 fiber로 만든다. 

특히 0번 Task는 current thread를 이용해서 만드는데, 
이는 simulator환경에서는 guest CPU는 fiber로 변경되어 있기 때문이다.

*/
void OS_Start()
{
#if defined(EN_SIM)
	for (int i = 1; i < gstOS._numTask; i++)
	{
		gstOS._aTCB[i] = CO_RegTask(gstOS._pfTask[i], gstOS._pParam[i], (gstOS.pStkTop[i]));
	}
	gstOS._aTCB[0] = CO_GetCurTask();
#elif defined(__arm__)
	for (int i = 1; i < gstOS._numTask; i++)
	{
		gstOS._aTCB[i] = os_InitStk(gstOS._pfTask[i], gstOS.pStkTop[i], gstOS._pParam[i]);
	}
	pCurTCB = &(gstOS._aTCB[0]);
#endif

	// Call task 0 (not return).
	gnAsyncTick = 0;
	gstOS._curTID = 0;
	gstOS._pfTask[0](gstOS._pParam[0]);
	ASSERT(0);
}

/**
Wait multi event with timeout
@param bmEvent bitmap of event that the task wait.
@param nTO Timeout to wait.
@return Event that reached.
*/
uint32 OS_Wait(uint32 bmEvt, uint32 nTO)
{
//	ASSERT(!gstOS._bInCritical);
//	ASSERT(gstOS._bmRdyTask & BIT(gstOS._curTID));
	if (0 != nTO)	// Yield case.
	{
		BIT_CLR(gstOS._bmRdyTask, BIT(gstOS._curTID));
		gstOS._aExpire[gstOS._curTID] = nTO;
		gstOS._aWaitEvt[gstOS._curTID] = bmEvt;
	}

#if defined(EN_SIM)
	os_SetNextTask();
//	SIM_Print("Sched: %s\n", gstOS.aszName[gstOS._curTID]);
	CO_Switch(gstOS._aTCB[gstOS._curTID]);
	CPU_TimePass(SIM_USEC(50));
#elif defined(__arm__)
	os_SwitchForARM();
#endif

	return gstOS._aWaitEvt[gstOS._curTID];
}

void OS_Tick()
{
	gnAsyncTick++;
}

/**
Interrupt에 의해 만들어진 event의 경우 
ISR context에서 당장 하는게 아니라 event flag만 켜 둔다.
추후에 main thread가 처리할 수 있을 때 처리한다.

@remark 다른 CPU나 HW에서 호출되는 것이 일반적이며, 
		이 경우, CPU id가 잘못지정되어 있을 수 있지만, 
		ISR에서 current CPU ID를 바꿔준 다음에 call해주기 때문에, 
		CPU ID는 정상값을 가지게 된다.
*/
void OS_AsyncEvt(uint32 bmEvt)
{
	gbmAsyncEvt |= bmEvt;
}

/**
하나의 event를 처리한다.
*/
void inline os_applyEvt(uint32 bmNewEvt)
{
	for (int nTaskId = 0; nTaskId < gstOS._numTask; nTaskId++)
	{
		if (unlikely(gstOS._aWaitEvt[nTaskId] & bmNewEvt))
		{
			gstOS._bmRdyTask |= BIT(nTaskId);
			BIT_CLR(gstOS._aWaitEvt[nTaskId], bmNewEvt);
		}
	}
}

void os_handleAsyncEvt()
{
	CPU_TimePass(0);

	if (gnAsyncTick > 0)
	{
		uint32 nNewTick = gnAsyncTick;
		gnAsyncTick = 0;
		gstOS._nTick += nNewTick;
		for (uint32 i = 0; i < gstOS._numTask; i++)
		{
			if (gstOS._aExpire[i] > 0)
			{
				gstOS._aExpire[i] -= nNewTick;
				if (gstOS._aExpire[i] <= 0)
				{
					gstOS._bmRdyTask |= BIT(i);
					gstOS._aExpire[i] = 0;
				}
			}
		}
	}

	if (0 != gbmAsyncEvt)
	{
		uint32 bmEvts = gbmAsyncEvt;
		gbmAsyncEvt = 0;
		os_applyEvt(bmEvts);
	}
}

uint32 OS_GetTick()
{
	return gstOS._nTick;
};

/**
Sync event는 switching 하지 않는 것으로...
Sync event는 자기 자신은 제외해야하는 것 아닌가? 
왜냐하면, 본인 event로 스스로 깨어나는 것은 논리적으로 말이 안됨.
*/
uint32 OS_SyncEvt(uint32 bmEvt)
{
	os_applyEvt(bmEvt);
	return 0;
}

/**
모든 task가 schedule이 될 수 없도록 한다.
물론, 이후에 새로운 event를 생성하는 경우에는 running이 가능하다.
*/
void OS_Stop(uint32 bmTask)
{
	BIT_CLR(bmTask, BIT(gstOS._curTID));
	BIT_CLR(gstOS._bmRdyTask, bmTask);
	for (int i = 0; i < gstOS._numTask; i++)
	{
		if (bmTask & BIT(i))
		{
			gstOS._aWaitEvt[i] = 0;
			gstOS._aExpire[i] = 0;
		}
	}
}


/////////////////////////////

/**
OS운영을 위한 기본 Timer.
원칙적으로 ISR은 CPU time을 잡아먹어야 함.
TODO: 현재로선 ISR에서 CPU Time을 계산하는 기능은 없음.
	이 경우, Switch는 일어나지않고, Time만 증가시키는 방향으로 해야함.
	이런 경우 ISR이 계속 호출될 경우 main task는 실행될 시간이 없을 수 있음..!!
*/
void os_TickISR(uint32 nTag, uint32 nResult)
{
	OS_Tick();
	CPU_Wakeup(CPU_FTL);
}

void OS_Init()
{
	memset((void*)&gstOS, 0x00, sizeof(gstOS));
#if defined(EN_SIM)
	TMR_Init();
	TMR_Add(0, TICK_SIM_PER_OS, os_TickISR, true);
#endif
}
