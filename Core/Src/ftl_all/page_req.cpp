
#include "templ.h"
#include "buf.h"
#include "os.h"
#include "io.h"
#include "page_gc.h"
#include "page_meta.h"

#define PRINTF			SIM_Print
#define CMD_PRINTF

extern Queue<ReqInfo*, SIZE_REQ_QUE> gstReqQ;

/// Requested
struct RunInfo
{
	ReqInfo* pReq;
	uint32 nIssued;	///< Issued count.
	uint32 nDone; ///< Done count.
	uint32 nTotal;
};
Queue<uint32, SIZE_REQ_QUE> gstReqInfoPool;
RunInfo gaIssued[SIZE_REQ_QUE];


CbfReq gfCbf;

void REQ_SetCbf(CbfReq pfCbf)
{
	gfCbf = pfCbf;
}

static void req_Done(NCmd eCmd, uint32 nTag)
{
	RunInfo* pRun = gaIssued + nTag;
	ReqInfo* pReq = pRun->pReq;
	uint32* pnVal = (uint32*)BM_GetSpare(pReq->nBuf);
	pRun->nDone++;

	if (likely(MARK_ERS != *pnVal))
	{
		ASSERT(pReq->nLPN == *pnVal);
	}

	// Calls CPU_WORK cpu function --> treat as ISR.
	if (pRun->nDone == pRun->nTotal)
	{
		gfCbf(pReq);
		gstReqInfoPool.PushTail(nTag);
	}
}

static void req_Write_OS(ReqInfo* pReq, uint8 nTag)
{
	uint32 nLPN = pReq->nLPN;

	bool bRet = false;
	OpenBlk* pDst = META_GetOpen(OPEN_USER);
	if (unlikely(nullptr == pDst) || unlikely(pDst->stNextVA.nWL >= NUM_WL))
	{
		uint16 nBN = GC_ReqFree_Blocking(OPEN_USER);
		ASSERT(FF16 != nBN);
		GC_BlkErase_OS(OPEN_USER, nBN);
		META_SetOpen(OPEN_USER, nBN);
	}
	*(uint32*)BM_GetSpare(pReq->nBuf) = pReq->nLPN;
#if (EN_COMPARE == 1)
	ASSERT(pReq->nLPN == *(uint32*)BM_GetMain(pReq->nBuf));
#endif
	JnlRet eJRet;
	while(true)
	{
		eJRet = META_Update(pReq->nLPN, pDst->stNextVA, OPEN_USER);
		if (likely(JR_Busy != eJRet))
		{
			break;
		}
		OS_Wait(BIT(EVT_META), LONG_TIME);
	}

	CmdInfo* pCmd = IO_Alloc(IOCB_User);
	IO_Program(pCmd, pDst->stNextVA.nBN, pDst->stNextVA.nWL, pReq->nBuf, nTag);

	pDst->stNextVA.nWL++;
	
	if (unlikely(JR_Filled == eJRet))
	{
		META_ReqSave(false);	// wait till meta save.
	}
}

/**
* Unmap read인 경우, sync response, 
* normal read는 nand IO done에서 async response.
*/
static bool req_Read_OS(ReqInfo* pReq, uint8 nTag)
{
	uint32 nLPN = pReq->nLPN;
	VAddr stAddr = META_GetMap(nLPN);

	if (likely(FF32 != stAddr.nDW))
	{
		CmdInfo* pCmd = IO_Alloc(IOCB_User);
		IO_Read(pCmd, stAddr.nBN, stAddr.nWL, pReq->nBuf, nTag);
	}
	else
	{
		uint32* pnVal = (uint32*)BM_GetSpare(pReq->nBuf);
		*pnVal = nLPN;
		req_Done(NC_READ, nTag);
	}
	return true;
}

/**
* Shutdown command는 항상 sync로 처리한다.
*/
static void req_Shutdown_OS(ReqInfo* pReq, uint8 nTag)
{
	PRINTF("[SD] %d\n", pReq->eOpt);
	IO_SetStop(CbKey::IOCB_Mig, true);
	OS_Idle(OS_MSEC(5));

	if (SD_Safe == pReq->eOpt)
	{
		META_ReqSave(true);
	}

	gfCbf(pReq);
	gstReqInfoPool.PushTail(nTag);
	PRINTF("[SD] Done\n");
}

void req_Run(void* pParam)
{
	while (false == META_Ready())
	{
		OS_Wait(BIT(EVT_OPEN), LONG_TIME);
	}

	while (true)
	{
		if (gstReqQ.Count() <= 0)
		{
			OS_Wait(BIT(EVT_USER_CMD), LONG_TIME);
			continue;
		}

		uint32 nCurSlot = gstReqInfoPool.PopHead();
		RunInfo* pRun = gaIssued + nCurSlot;
		ReqInfo* pReq = gstReqQ.PopHead();
		pRun->pReq = pReq;
		pRun->nIssued = 0;
		pRun->nDone = 0;
		pRun->nTotal = 1;
		switch (pReq->eCmd)
		{
			case CMD_READ:
			{
				req_Read_OS(pReq, nCurSlot);
				break;
			}
			case CMD_WRITE:
			{
				req_Write_OS(pReq, nCurSlot);
				break;
			}
			case CMD_SHUTDOWN:
			{
				req_Shutdown_OS(pReq, nCurSlot);
				break;
			}
			default:
			{
				ASSERT(false);
			}
		}
	}
}

/**
* Error는 response task에서 처리하도록 하자.
*/
void reqResp_Run(void* pParam)
{
	while (true)
	{
		CmdInfo* pCmd = IO_PopDone(IOCB_User);
		if (nullptr == pCmd)
		{
			OS_Wait(BIT(EVT_NAND_CMD), LONG_TIME);
		}
		else
		{
			if (NC_READ == pCmd->eCmd)
			{
				req_Done(pCmd->eCmd, pCmd->nTag);
			}
			else
			{
				if (unlikely(pCmd->nWL == (NUM_DATA_PAGE - 1)))
				{
					META_SetBlkState(pCmd->anBBN[0], BS_Closed);
				}
				req_Done(pCmd->eCmd, pCmd->nTag);
			}
			IO_Free(pCmd);
			OS_Wait(0, 0);
		}
	}
}

#define STK_DW_SIZE_REQ		(90)
static uint32 aReqStk[STK_DW_SIZE_REQ];
#define STK_DW_SIZE_RSP		(64)
static uint32 aRespStk[STK_DW_SIZE_RSP];

void REQ_Init()
{
	gstReqInfoPool.Init();
	for (uint32 nIdx = 0; nIdx < SIZE_REQ_QUE; nIdx++)
	{
		gstReqInfoPool.PushTail(nIdx);
	}
	memset(aReqStk, 0xCD, sizeof(aReqStk));
	memset(aRespStk, 0xCD, sizeof(aRespStk));
	OS_CreateTask(req_Run, aReqStk + STK_DW_SIZE_REQ, nullptr, "req");
	OS_CreateTask(reqResp_Run, aRespStk + STK_DW_SIZE_RSP, nullptr, "req_resp");
}
