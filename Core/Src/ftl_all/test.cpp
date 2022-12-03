
#include "sim.h"
#include "config.h"
#include "cpu.h"
#include "buf.h"
#if (EN_WORK_GEN == 1)
#include "scheduler.h"
#else
#include "power.h"
#endif
#include "ftl.h"
#include "test.h"
#include <stdio.h>

#define PRINTF			// SIM_Print
#define CMD_PRINTF		// SIM_Print

#if (EN_COMPARE == 1)
static uint32* gaDict;
#endif
static bool gbDone;

#if (EN_COMPARE == 1)
void _FillData(uint16 nBuf, uint32 nLPN)
{
	gaDict[nLPN]++;
	uint32* pnData = (uint32*)BM_GetMain(nBuf);
	pnData[0] = nLPN;
	pnData[1] = gaDict[nLPN];
	uint32* pnSpare = (uint32*)BM_GetSpare(nBuf);
	pnSpare[1] = gaDict[nLPN];
}

void _CheckData(uint16 nBuf, uint32 nLPN)
{
	uint32* pnData = (uint32*)BM_GetMain(nBuf);
	if (gaDict[nLPN] > 0)
	{
		ASSERT(pnData[0] == nLPN);
		ASSERT(pnData[1] == gaDict[nLPN]);
	}
}
#else
#define _FillData(...)
#define _CheckData(...)
#endif

#if (EN_WORK_GEN == 0)
void _BusyWaitDone()
{
	while (false == gbDone)
	{
		CPU_Sleep();
	}
}

void _CmdDone(ReqInfo* pReq)
{
	gbDone = true;
	CPU_Wakeup(CPU_WORK, SIM_USEC(2)); // wake up me.
}

void tc_SeqWrite(uint32 nStart, uint32 nSize)
{
	PRINTF("[TC] =========== %s ==========\n", __FUNCTION__);
	ReqInfo stReq;
	stReq.eCmd = CMD_WRITE;
	uint32 nCur = nStart;
	for (uint32 nCur = nStart; nCur < nStart + nSize; nCur++)
	{
		stReq.nLPN = nCur;
		stReq.nBuf = BM_Alloc();
		_FillData(stReq.nBuf, stReq.nLPN);
		gbDone = false;
		CMD_PRINTF("[CMD] W %X\n", stReq.nLPN);
		FTL_Request(&stReq);
		_BusyWaitDone();
		BM_Free(stReq.nBuf);
	}
}

void tc_SeqRead(uint32 nStart, uint32 nSize)
{
	PRINTF("[TC] =========== %s ==========\n", __FUNCTION__);
	ReqInfo stReq;
	stReq.eCmd = CMD_READ;
	uint32 nCur = nStart;
	for (uint32 nCur = nStart; nCur < nStart + nSize; nCur++)
	{
		stReq.nLPN = nCur;
		stReq.nBuf = BM_Alloc();
		gbDone = false;
		CMD_PRINTF("[CMD] R %X\n", stReq.nLPN);
		FTL_Request(&stReq);
		_BusyWaitDone();
		_CheckData(stReq.nBuf, stReq.nLPN);
		BM_Free(stReq.nBuf);
	}
}


void tc_RandWrite(uint32 nBase, uint32 nRange, uint32 nCount)
{
	PRINTF("[TC] =========== %s ==========\n", __FUNCTION__);
	ReqInfo stReq;
	stReq.eCmd = CMD_WRITE;
	while(nCount--)
	{
		stReq.nLPN = nBase + SIM_GetRand(nRange);
		stReq.nBuf = BM_Alloc();
		_FillData(stReq.nBuf, stReq.nLPN);
		gbDone = false;
		CMD_PRINTF("[CMD] W %X\n", stReq.nLPN);
		FTL_Request(&stReq);
		_BusyWaitDone();
		BM_Free(stReq.nBuf);
	}
}


void tc_RandRead(uint32 nBase, uint32 nRange, uint32 nCount)
{
	PRINTF("[TC] =========== %s ==========\n", __FUNCTION__);
	ReqInfo stReq;
	stReq.eCmd = CMD_READ;

	while (nCount--)
	{
		stReq.nLPN = nBase + SIM_GetRand(nRange);
		stReq.nBuf = BM_Alloc();
		gbDone = false;
		CMD_PRINTF("[CMD] R %X\n", stReq.nLPN);
		FTL_Request(&stReq);
		_BusyWaitDone();
		_CheckData(stReq.nBuf, stReq.nLPN);
		BM_Free(stReq.nBuf);
	}
}

void tc_StreamWrite(uint32 nMaxLPN)
{
	PRINTF("[TC] =========== %s ==========\n", __FUNCTION__);
	uint32 anLPN[3];
	anLPN[0] = 0;
	anLPN[1] = nMaxLPN / 4;
	anLPN[2] = nMaxLPN / 2;

	ReqInfo stReq;
	stReq.eCmd = CMD_WRITE;
	uint32 nCnt = nMaxLPN / 4;
	for (uint32 nCur = 0; nCur < nCnt; nCur++)
	{
		for (uint32 nStream = 0; nStream < 3; nStream++)
		{
			stReq.nLPN = anLPN[nStream];
			anLPN[nStream]++;
			stReq.nBuf = BM_Alloc();
			_FillData(stReq.nBuf, stReq.nLPN);
			gbDone = false;
			CMD_PRINTF("[CMD] W %X\n", stReq.nLPN);
			FTL_Request(&stReq);
			_BusyWaitDone();
			BM_Free(stReq.nBuf);
		}
	}
}


void tc_Shutdown(ShutdownOpt eOpt)
{
	ReqInfo stReq;
	stReq.eCmd = CMD_SHUTDOWN;
	stReq.eOpt = eOpt;
	gbDone = false;
	CMD_PRINTF("[CMD] SD\n");
	FTL_Request(&stReq);
	_BusyWaitDone();
}

void sc_Long()
{
	uint32 nNumUserLPN = FTL_GetNumLPN(_CmdDone);

	for(uint32 nLoop = 0; nLoop < 2; nLoop++)
	{
		if (0 == (SIM_GetCycle() % 10))
		{
			tc_SeqWrite(0, nNumUserLPN);
		}
		tc_SeqRead(0, nNumUserLPN);
		while(true)
		{
			for (uint32 nLoop = 0; nLoop < 10; nLoop++)
			{
				tc_RandRead(0, nNumUserLPN, nNumUserLPN / 4);
				tc_RandWrite(0, nNumUserLPN, nNumUserLPN / 4);
			}
			tc_StreamWrite(nNumUserLPN);

			tc_SeqRead(0, nNumUserLPN);
		}
	}

	PRINTF("Test Done: %s\n", __FUNCTION__);
}

void sc_DefRunner()
{
	uint32 nNumUserLPN = FTL_GetNumLPN(_CmdDone);

	tc_SeqRead(0, nNumUserLPN);
	if (0 == SIM_GetCycle())
	{
		tc_SeqWrite(0, nNumUserLPN);
	}
	else
	{
		tc_RandWrite(0, nNumUserLPN, nNumUserLPN / 8);
	}

	PRINTF("Test Done: %s\n", __FUNCTION__);
}
/**
Workload 생성역할.
*/
void TEST_Main(void* pParam)
{
	uint32 nNumUserLPN = FTL_GetNumLPN(_CmdDone);
#if (EN_COMPARE == 1)
	if (nullptr == gaDict)
	{
		gaDict = new uint32[nNumUserLPN];
		memset(gaDict, 0, sizeof(uint32) * nNumUserLPN);
	}
#endif
	do
	{
		sc_DefRunner();
	} while (EN_WORK_GEN);
	tc_Shutdown((SIM_GetRand(10) < 5) ? SD_Sudden : SD_Safe);
	POWER_SwitchOff();
	END_RUN;
}

#else

/****************************************************
* FSM 기반의 Workload generator.
****************************************************/

#include "scheduler.h"

uint32 gnCntLBA;

void _CmdDone(ReqInfo* pReq)
{
	gbDone = true;
	Sched_TrigSyncEvt(BIT(EVT_HOST)); // wake up me.
}
/**
* 일반적인 Test수행을 위한 state 모음.
*/
struct SubStk
{
	bool bInit;
	uint32 nBaseLBA;	///< Start LBA.
	uint32 nEndLBAp1;	///< End LBA + 1.
	uint32 nAmount;		///< LBA to issue.

	uint32 nLBA;		///< runtime param.
	uint32 nLeft;		///< runtime param.
};

bool tc_SeqRead(SubStk* pStk)
{
	static ReqInfo stReq;
	if (false == pStk->bInit)
	{
		pStk->bInit = true;
		pStk->nLeft = pStk->nAmount;
		pStk->nLBA = pStk->nBaseLBA;

		PRINTF("[TC] =========== %s ==========\n", __FUNCTION__);

		stReq.eCmd = CMD_READ;
		stReq.nLPN = pStk->nLBA;
		stReq.nBuf = BM_Alloc();
		gbDone = false;
		CMD_PRINTF("[CMD] R %X\n", stReq.nLPN);
		FTL_Request(&stReq);
	}
	else if (true == gbDone)
	{
		BM_Free(stReq.nBuf);

		gbDone = false;
		pStk->nLeft--;
		if (pStk->nLeft <= 0)
		{
			return true;
		}
		pStk->nLBA++;

		stReq.eCmd = CMD_READ;
		stReq.nLPN = pStk->nLBA;
		stReq.nBuf = BM_Alloc();
		CMD_PRINTF("[CMD] R %X\n", stReq.nLPN);
		FTL_Request(&stReq);
	}
	Sched_Wait(BIT(EVT_HOST), LONG_TIME);
	return false;
}

bool tc_SeqWrite(SubStk* pStk)
{
	static ReqInfo stReq;
	if (false == pStk->bInit)
	{
		pStk->bInit = true;
		pStk->nLeft = pStk->nAmount;
		pStk->nLBA = pStk->nBaseLBA;

		PRINTF("[TC] =========== %s ==========\n", __FUNCTION__);

		stReq.eCmd = CMD_WRITE;
		stReq.nLPN = pStk->nLBA;
		stReq.nBuf = BM_Alloc();
		gbDone = false;
		CMD_PRINTF("[CMD] W %X\n", stReq.nLPN);
		FTL_Request(&stReq);
	}
	else if (true == gbDone)
	{
		BM_Free(stReq.nBuf);

		gbDone = false;
		pStk->nLeft--;
		if (pStk->nLeft <= 0)
		{
			return true;
		}
		pStk->nLBA++;

		stReq.eCmd = CMD_WRITE;
		stReq.nLPN = pStk->nLBA;
		stReq.nBuf = BM_Alloc();
		CMD_PRINTF("[CMD] R %X\n", stReq.nLPN);
		FTL_Request(&stReq);
	}
	Sched_Wait(BIT(EVT_HOST), LONG_TIME);
	return false;
}

bool tc_RandRead(SubStk* pStk)
{
	static ReqInfo stReq;
	if (false == pStk->bInit)
	{
		pStk->bInit = true;
		pStk->nLeft = pStk->nAmount;

		PRINTF("[TC] =========== %s ==========\n", __FUNCTION__);

		stReq.eCmd = CMD_READ;
		stReq.nLPN = pStk->nBaseLBA + SIM_GetRand(pStk->nEndLBAp1 - pStk->nBaseLBA);
		stReq.nBuf = BM_Alloc();
		gbDone = false;
		CMD_PRINTF("[CMD] R %X\n", stReq.nLPN);
		FTL_Request(&stReq);
	}
	else if (true == gbDone)
	{
		BM_Free(stReq.nBuf);

		gbDone = false;
		if (--pStk->nLeft <= 0)
		{
			return true;
		}

		stReq.eCmd = CMD_READ;
		stReq.nLPN = pStk->nBaseLBA + SIM_GetRand(pStk->nEndLBAp1 - pStk->nBaseLBA);
		stReq.nBuf = BM_Alloc();
		CMD_PRINTF("[CMD] R %X\n", stReq.nLPN);
		FTL_Request(&stReq);
	}
	Sched_Wait(BIT(EVT_HOST), LONG_TIME);
	return false;
}

bool tc_RandWrite(SubStk* pStk)
{
	static ReqInfo stReq;
	if (false == pStk->bInit)
	{
		pStk->bInit = true;
		pStk->nLeft = pStk->nAmount;

		PRINTF("[TC] =========== %s ==========\n", __FUNCTION__);

		gbDone = false;
		stReq.eCmd = CMD_WRITE;
		stReq.nLPN = pStk->nBaseLBA + SIM_GetRand(pStk->nEndLBAp1 - pStk->nBaseLBA);
		stReq.nBuf = BM_Alloc();
		CMD_PRINTF("[CMD] W %X\n", stReq.nLPN);
		FTL_Request(&stReq);
	}
	else if (true == gbDone)
	{
		BM_Free(stReq.nBuf);
		gbDone = false;

		if (--pStk->nLeft <= 0)
		{
			return true;
		}

		stReq.eCmd = CMD_WRITE;
		stReq.nLPN = pStk->nBaseLBA + SIM_GetRand(pStk->nEndLBAp1 - pStk->nBaseLBA);
		stReq.nBuf = BM_Alloc();
		CMD_PRINTF("[CMD] W %X\n", stReq.nLPN);
		FTL_Request(&stReq);
	}
	Sched_Wait(BIT(EVT_HOST), LONG_TIME);
	return false;
}

typedef bool (*TestRun)(SubStk* pStk);
struct TestDef
{
	TestRun pfRun;
	uint32 nBaseLBA;
	uint32 nEndLBAp1;	// FF32 : End of LBA.
	uint32 nTotal;		// FF32 : end of LBA
};

struct ScStk
{
	enum Step
	{
		Init,
		Prepare,
		Run,
	};
	Step eStep;
	uint32 nTestIdx;
};

#include "stm32f1xx_hal.h"
inline void InitTick()
{
	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
	DWT->CYCCNT = 0;
	DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}
inline uint32_t GetTick(bool bReset)
{
	uint32_t nTick = DWT->CYCCNT;
	if(bReset)
	{
		DWT->CYCCNT = 0;
	}
	return nTick;
}

const TestDef gaShortSet[] =
{
	{tc_SeqRead, 0, FF32, 1},	///< for Wait open.
	{tc_SeqWrite, 0, FF32, FF32},
	{tc_SeqRead, 0, FF32, FF32},
	{tc_RandRead, 0, FF32, FF32},
	{tc_RandWrite, 0, FF32, 48},
	{tc_RandWrite, 0, FF32, 48},
	{tc_RandWrite, 0, FF32, 48},
	{tc_RandWrite, 0, FF32, 48},
	{tc_RandWrite, 0, FF32, 48},
	{tc_RandWrite, 0, FF32, 48},
	{tc_RandWrite, 0, FF32, 48},
	{tc_RandWrite, 0, FF32, 48},
};

void sc_DefRunner(void* pParam)
{
	ScStk* pStk = (ScStk*) pParam;
	SubStk* pTStk = (SubStk*)(pStk + 1);
	static uint32 nTick;
	switch (pStk->eStep)
	{
		case ScStk::Init:
		{
			gnCntLBA = FTL_GetNumLPN(_CmdDone);
			srand(0);
			pStk->eStep = ScStk::Prepare;
			pStk->nTestIdx = 0;
			InitTick();
			// Go through;
		}

		case ScStk::Prepare:
		{
			pStk->eStep = ScStk::Run;
			memset(pTStk, 0x00, sizeof(SubStk));
			pTStk->nBaseLBA = gaShortSet[pStk->nTestIdx].nBaseLBA;
			pTStk->nEndLBAp1 = (FF32 == gaShortSet[pStk->nTestIdx].nEndLBAp1)
				? gnCntLBA : gaShortSet[pStk->nTestIdx].nEndLBAp1;
			pTStk->nAmount = (FF32 == gaShortSet[pStk->nTestIdx].nTotal)
				? gnCntLBA : gaShortSet[pStk->nTestIdx].nTotal;
			pTStk->bInit = false;
			Sched_Yield();
			GetTick(true);		// Tick...
			break;
		}

		case ScStk::Run:
		{
			if (gaShortSet[pStk->nTestIdx].pfRun(pTStk))
			{
				char aBuf[32];
				nTick = GetTick(false);
				myPrintf("TC: %d, %d\n", pStk->nTestIdx, nTick);

				pStk->nTestIdx++;
				if (pStk->nTestIdx < sizeof(gaShortSet) / sizeof(gaShortSet[0]))
				{
					pStk->eStep = ScStk::Prepare;
					Sched_Yield();
				}
				else
				{
					myPrintf("End Of TC\n");
				}
			}
		}
	}
}

static uint8 aTestStack[0x80];

void TEST_Init()
{
	MEMSET_ARRAY(aTestStack, 0xCD);

	ScStk* pStk = (ScStk*)aTestStack;
	pStk->eStep = ScStk::Init;
	pStk->nTestIdx = 0;
	Sched_Register(TID_TEST, sc_DefRunner, aTestStack, BIT(MODE_NORMAL));
}
#endif

