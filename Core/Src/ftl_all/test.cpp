
#include "config.h"
#include "sim.h"
#include "cpu.h"
#include "buf.h"
#if (EN_WORK_GEN == 1)
#include "os.h"
#else
#include "power.h"
#endif
#include "ftl.h"
#include "test.h"

#include <stdio.h>

#define PRINTF			SIM_Print
#define CMD_PRINTF		// SIM_Print

#if (EN_COMPARE == 1)
static uint32* gaDict;
#endif
static bool gbDone;

extern int __io_putchar(int ch);

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

#if (EN_WORK_GEN == 1)  // Workload generater in SSD.
void _WaitDone()
{
	while (false == gbDone)
	{
		OS_Wait(BIT(EVT_HOST), 0);
	}
}

void _CmdDone(ReqInfo* pReq)
{
	gbDone = true;
	OS_SyncEvt(BIT(EVT_HOST));
}

#else  // Separate CPU case (ie. Host PC)
void _WaitDone()
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
#endif

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
		_WaitDone();
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
		_WaitDone();
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
		_WaitDone();
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
		_WaitDone();
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
			_WaitDone();
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
	_WaitDone();
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
		uint32 nCnt = 3;
		while(nCnt-- > 0)
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

void sc_Short()
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

#include "stm32f1xx_hal.h"
inline void InitTick()
{
	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
	DWT->CYCCNT = 0;
	DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}
inline uint32_t GetTick(bool bReset)
{
	uint32_t nCnt = DWT->CYCCNT;
	if(bReset)
	{
		DWT->CYCCNT = 0;
	}
	return nCnt;
}

void sc_Test()
{
    uint32 nNumUserLPN = FTL_GetNumLPN(_CmdDone);
    int32 nTick;

    InitTick();
    tc_SeqRead(0, 1);
    nTick = GetTick(false);
    myPrintf("OPEN: %d\n", nTick);

    GetTick(true);
    tc_SeqWrite(0, nNumUserLPN);
    nTick = GetTick(false);
    myPrintf("SW: %d\n", nTick);

    GetTick(true);
    tc_SeqRead(0, nNumUserLPN);
    nTick = GetTick(false);
    myPrintf("SR: %d\n", nTick);

    GetTick(true);
    tc_RandRead(0, nNumUserLPN, nNumUserLPN);
    nTick = GetTick(false);
    myPrintf("RR: %d\n", nTick);

    GetTick(true);
    tc_RandWrite(0, nNumUserLPN, nNumUserLPN);
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
		sc_Test();
		while(1);
	} while (EN_WORK_GEN);
#if (EN_WORK_GEN == 0)
	tc_Shutdown((SIM_GetRand(10) < 5) ? SD_Sudden : SD_Safe);
	POWER_SwitchOff();
	END_RUN;
#endif
}

#if (EN_WORK_GEN == 1)
void TEST_Init()
{
	OS_CreateTask(TEST_Main, nullptr, nullptr, "test");
}
#endif

