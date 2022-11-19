#include "templ.h"
#include "cpu.h"
#include "buf.h"
#include "scheduler.h"
#include "io.h"
#include "page_gc.h"
#include "page_req.h"
#include "page_meta.h"

#define PRINTF		//	SIM_Print

Queue<ReqInfo*, SIZE_REQ_QUE> gstReqQ;

/**
* Called by other CPU.
*/
void FTL_Request(ReqInfo* pReq)
{
	pReq->nSeqNo = SIM_GetSeqNo();
	gstReqQ.PushTail(pReq);
	
#if (EN_WORK_GEN == 1)
	Sched_TrigAsyncEvt(BIT(EVT_USER_CMD));
	CPU_TimePass(SIM_USEC(5));
#else
	Sched_TrigAsyncEvt(BIT(EVT_USER_CMD)); 
	CPU_TimePass(SIM_USEC(5));
	CPU_Wakeup(CPU_FTL, SIM_USEC(1));
#endif
}

/**
* Called by other CPU.
*/
uint32 FTL_GetNumLPN(CbfReq pfCbf)
{
	REQ_SetCbf(pfCbf);
	return NUM_LPN;
}

void FTL_Main(void* pParam)
{
	BM_Init();

	Sched_Init();

	gstReqQ.Init();
#if (EN_WORK_GEN == 1)
#include "test.h"
	TEST_Init();
#endif
	IO_Init();
	REQ_Init();
	META_Init();
	GC_Init();
	PRINTF("[FTL] Init done\n");
	Sched_Run();
}

