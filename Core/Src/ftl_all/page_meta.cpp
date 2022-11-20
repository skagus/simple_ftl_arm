
#include "types.h"
#include "config.h"
#include "templ.h"
#include "macro.h"
#include "buf.h"

#include "scheduler.h"
#include "io.h"
#include "page_gc.h"
#include "page_meta.h"

#define PRINTF			SIM_Print
#define MAP_PRINTF

#define PAGE_PER_META	(1)

Meta gstMeta;
bool gbRequest;
OpenBlk gaOpen[NUM_OPEN];
MetaCtx gstMetaCtx;
JnlSet gstJnlSet;

struct MtSaveStk
{
	enum MtSaveStep
	{
		Init,
		Erase,
		Program,
		Done,
	};
	MtSaveStep eStep;
	uint8 nIssue;	///< count of issued NAND operation.
	uint8 nDone;	///< count of done NAND operation.
};



struct MtStk
{
	enum MtStep
	{
		Mt_Init,
		Mt_Open,		///< In openning.
		Mt_Format,	///< In formatting.
		Mt_Ready,
		Mt_Saving,
	};
	MtStep eStep;
};
MtStk* gpMtStk;


static uint16 meta_MtBlk2PBN(uint16 nMetaBN)
{
	return nMetaBN + BASE_META_BLK;
}

struct FmtCtx
{
	enum Step
	{
		Memset,
		Save,
		Done,
	};
	Step eStep;
};
static bool meta_Format(FmtCtx* pFmtCtx, bool b1st)
{
	bool bRet = false;

	if (b1st)
	{
		pFmtCtx->eStep = FmtCtx::Memset;
	}
	switch (pFmtCtx->eStep)
	{
		case FmtCtx::Memset:
		{
			for (uint16 nIdx = 0; nIdx < NUM_USER_BLK; nIdx++)
			{
				gstMeta.astBI[nIdx].eState = BS_Closed;
				gstMeta.astBI[nIdx].nVPC = 0;
			}
			pFmtCtx->eStep = FmtCtx::Done;
			bRet = true;
			gstMetaCtx.nNextWL = 0;
			gstMetaCtx.nAge = 1;
			gstMetaCtx.nCurBO = 0;
			gstMetaCtx.nNextSlice = 0;
			break;
		}
		case FmtCtx::Save:
		{
			// TODO: Map save.
			bRet = true;
			break;
		}
		default:
		{
			assert(false);
		}
	}
	return bRet;
}

static void dbg_MapIntegrity()
{
	uint16 anVPC[NUM_USER_BLK];
	MEMSET_ARRAY(anVPC, 0x0);
	for (uint32 nLPN = 0; nLPN < NUM_LPN; nLPN++)
	{
		if (gstMeta.astL2P[nLPN].nBN < NUM_USER_BLK)
		{
			anVPC[gstMeta.astL2P[nLPN].nBN]++;
		}
	}
	for (uint32 nBN = 0; nBN < NUM_USER_BLK; nBN++)
	{
		ASSERT(gstMeta.astBI[nBN].nVPC == anVPC[nBN]);
	}
}

bool META_Ready()
{
	return (gpMtStk->eStep == MtStk::Mt_Ready);
}

VAddr META_GetMap(uint32 nLPN)
{
	if (nLPN < NUM_LPN)
	{
		return gstMeta.astL2P[nLPN];
	}
	else
	{
		VAddr stAddr;
		stAddr.nDW = FF32;
		return stAddr;
	}
}

BlkInfo* META_GetFree(uint16* pnBN, bool bFirst)
{
	for (uint32 nIdx = 0; nIdx < NUM_USER_BLK; nIdx++)
	{
		BlkInfo* pBI = gstMeta.astBI + nIdx;
		if ((BS_Closed == pBI->eState)
			&& (0 == pBI->nVPC))
		{
			if (NOT(bFirst))
			{
				bFirst = true;
				continue;
			}
			if (nullptr != pnBN)
			{
				*pnBN = nIdx;
			}
			return pBI;
		}
	}
	return nullptr;
}

void META_SetOpen(OpenType eType, uint16 nBN, uint16 nWL)
{
	OpenBlk* pOpen = gaOpen + eType;
	pOpen->stNextVA.nBN = nBN;
	pOpen->stNextVA.nWL = nWL;
	gstMeta.astBI[nBN].eState = BS_Open;
	if (0 == nWL)
	{
		gstMeta.astBI[nBN].nEC++;
	}
}

void META_SetBlkState(uint16 nBN, BlkState eState)
{
	gstMeta.astBI[nBN].eState = eState;
}

JnlRet META_AddErbJnl(OpenType eOpen, uint16 nBN)
{
	return gstJnlSet.AddErase(nBN, eOpen);
}

void META_StartJnl(OpenType eOpen, uint16 nBN)
{
	gstJnlSet.Start(eOpen, nBN);
}

JnlRet META_Update(uint32 nLPN, VAddr stNew, OpenType eOpen, bool bOnOpen)
{
	JnlRet eJRet = JR_Done;
	if (nLPN < NUM_LPN)
	{
		VAddr stOld = gstMeta.astL2P[nLPN];
		if (false == bOnOpen)
		{
			eJRet = gstJnlSet.AddWrite(nLPN, stNew, eOpen);
		}
		if (JR_Busy != eJRet)
		{
			gstMeta.astL2P[nLPN] = stNew;
			if (FF32 != stOld.nDW)
			{
				gstMeta.astBI[stOld.nBN].nVPC--;
			}
			if (FF32 != stNew.nDW)
			{
				gstMeta.astBI[stNew.nBN].nVPC++;
			}
		}
		MAP_PRINTF("[MAP] %X --> {%X,%X}\n", nLPN, stNew.nBN, stNew.nWL);
	}
#if defined(EN_SIM)
	if (false == bOnOpen)
	{
		dbg_MapIntegrity();
	}
#endif
	return eJRet;
}

BlkInfo* META_GetMinVPC(uint16* pnBN)
{
	uint16 nMinVPC = FF16;
	uint16 nMinBlk = FF16;
	for (uint16 nIdx = 0; nIdx < NUM_USER_BLK; nIdx++)
	{
		BlkInfo* pBI = gstMeta.astBI + nIdx;
		if ((BS_Closed == pBI->eState)
			&& (0 != pBI->nVPC)
			&& (nMinVPC > pBI->nVPC))
		{
			nMinBlk = nIdx;
			nMinVPC = pBI->nVPC;
		}
	}
	if (nullptr != pnBN)
	{
		*pnBN = nMinBlk;
	}
	return gstMeta.astBI + nMinBlk;
}

/***************************************************************************
* Meta Open/Save sequence.
***************************************************************************/

OpenBlk* META_GetOpen(OpenType eOpen)
{
	return gaOpen + eOpen;
}



static bool meta_Save_SM(MtSaveStk* pCtx)
{
	if (MtSaveStk::Init == pCtx->eStep)
	{
		pCtx->nIssue = 0;
		pCtx->nDone = 0;

		if (0 == gstMetaCtx.nNextWL)
		{
			pCtx->eStep = MtSaveStk::Erase;
		}
		else
		{
			pCtx->eStep = MtSaveStk::Program;
		}
	}

	bool bRet = false;
	CmdInfo* pDone;
	while (pDone = IO_PopDone(IOCB_Meta))
	{
		pCtx->nDone++;
		if (NC_ERB == pDone->eCmd)
		{
			ASSERT(MtSaveStk::Erase == pCtx->eStep);
			pCtx->nIssue = 0;
			pCtx->nDone = 0;
			pCtx->eStep = MtSaveStk::Program;
		}
		else // PGM done.
		{
			if (PAGE_PER_META == pCtx->nDone)
			{
				gstMetaCtx.nAge++;
				gstMetaCtx.nNextSlice++;
				if (gstMetaCtx.nNextSlice >= NUM_MAP_SLICE)
				{
					gstMetaCtx.nNextSlice = 0;
				}
				gstMetaCtx.nNextWL += PAGE_PER_META;
				if (gstMetaCtx.nNextWL >= NUM_WL)
				{
					gstMetaCtx.nNextWL = 0;
					gstMetaCtx.nCurBO++;
					if (gstMetaCtx.nCurBO >= NUM_META_BLK)
					{
						gstMetaCtx.nCurBO = 0;
					}
				}
				pCtx->eStep = MtSaveStk::Done;
				bRet = true;
			}
			BM_Free(pDone->stPgm.anBufId[0]);
		}
		IO_Free(pDone);
	}
	
	CmdInfo* pCmd;
	if (MtSaveStk::Erase == pCtx->eStep)
	{
		if (0 == pCtx->nIssue)
		{
			gstMetaCtx.nCurBN = meta_MtBlk2PBN(gstMetaCtx.nCurBO);
			PRINTF("[MT] ERS BO:%X, BN:%X\n", gstMetaCtx.nCurBO, gstMetaCtx.nCurBN);
			pCmd = IO_Alloc(IOCB_Meta);
			IO_Erase(pCmd, gstMetaCtx.nCurBN, 0);
			pCtx->nIssue++;
		}
	}
	else if (MtSaveStk::Program == pCtx->eStep)
	{
		if (PAGE_PER_META > pCtx->nIssue)
		{
			uint16 nBuf = BM_Alloc();
			uint32* pSpare = (uint32*)BM_GetSpare(nBuf);
			pSpare[0] = gstMetaCtx.nAge;
			pSpare[1] = gstMetaCtx.nNextSlice;
			uint8* pDst = BM_GetMain(nBuf);
			if (0 == pCtx->nIssue)
			{
				memcpy(pDst, &gstJnlSet, sizeof(gstJnlSet));
				pDst += sizeof(gstJnlSet);
				uint8* pSrc = (uint8*)(&gstMeta) + (gstMetaCtx.nNextSlice * SIZE_MAP_PER_SAVE);
				memcpy(pDst, pSrc, SIZE_MAP_PER_SAVE);
			}
			else
			{
				assert(false);
			}

			uint16 nWL = gstMetaCtx.nNextWL + pCtx->nIssue;
			pCmd = IO_Alloc(IOCB_Meta);
			IO_Program(pCmd, gstMetaCtx.nCurBN, nWL, nBuf, 0);
			PRINTF("[MT%X] \t==== PGM (%X,%X) Age:%X, Slice: %X\n", pCmd->nDbgSN, gstMetaCtx.nCurBN, nWL, gstMetaCtx.nAge, gstMetaCtx.nNextSlice);
			pCtx->nIssue++;
		}
	}
	if (pCtx->nIssue > pCtx->nDone)
	{
		Sched_Wait(BIT(EVT_NAND_CMD), LONG_TIME);
	}

	return bRet;
}
// =========================================
struct DataScanStk
{
	enum ScanState
	{
		Init,
		Run,
	};
	ScanState eState;
	OpenType eOpen;
	uint16 nBN;
	uint16 nNextWL;
	uint16 nErasedWL;
	uint16 nRun;		///< Count of running nand command.
};

static bool open_UserScan_SM(DataScanStk* pCtx)
{
	bool bRet = false;
	if (DataScanStk::Init == pCtx->eState)
	{
		OpenBlk* pOpen = META_GetOpen(pCtx->eOpen);
		pCtx->eState = DataScanStk::Run;
		pCtx->nRun = 0;
		pCtx->nErasedWL = FF16;
		pCtx->nBN = pOpen->stNextVA.nBN;
		pCtx->nNextWL = pOpen->stNextVA.nWL;
		pCtx->nErasedWL = FF16;
	}

	CmdInfo* pDone;
	if (nullptr != (pDone = IO_PopDone(CbKey::IOCB_Meta)))
	{
		pCtx->nRun--;
		uint16 nBuf = pDone->stRead.anBufId[0];
		uint32* pnSpare = (uint32*)BM_GetSpare(nBuf);

		if (MARK_ERS != *pnSpare)
		{
			uint32 nLPN = *pnSpare;
			VAddr stCur(0, pDone->anBBN[0], pDone->nWL);
			PRINTF("[SCAN] MapUpdate: LPN:%X to (%X, %X), %c\n",
				*pnSpare, pDone->anBBN[0], pDone->nWL, pCtx->eOpen == OPEN_GC ? 'G' : 'U');
			META_Update(*pnSpare, stCur, pCtx->eOpen);
		}
		else if (FF16 == pCtx->nErasedWL)
		{
			PRINTF("[SCAN] Erased detect: (%X, %X), %c\n",
				pDone->anBBN[0], pDone->nWL, pCtx->eOpen == OPEN_GC ? 'G' : 'U');
			pCtx->nErasedWL = pDone->nWL;
		}
		BM_Free(nBuf);
		IO_Free(pDone);
	}

	while ((FF16 == pCtx->nErasedWL) && (pCtx->nNextWL < NUM_WL) && (pCtx->nRun < 2))
	{
		uint16 nBuf = BM_Alloc();
		CmdInfo* pCmd = IO_Alloc(IOCB_Meta);
		IO_Read(pCmd, pCtx->nBN, pCtx->nNextWL, nBuf, pCtx->nNextWL);
		pCtx->nNextWL++;
		pCtx->nRun++;
	}

	if (pCtx->nRun > 0)
	{
		Sched_Wait(BIT(EVT_NAND_CMD), LONG_TIME);
	}
	else if ((pCtx->nNextWL >= NUM_WL) || (FF16 != pCtx->nErasedWL)) // All done.
	{
		OpenBlk* pOpen = META_GetOpen(pCtx->eOpen);
		if (FF16 != pCtx->nErasedWL)
		{
			pOpen->stNextVA.nWL = pCtx->nErasedWL;
		}
		else
		{
			pOpen->stNextVA.nWL = NUM_WL;
		}
		bRet = true;
	}
	return bRet;
}

// =================== Meta Page Scan ========================
struct MtPgStk
{
	enum State
	{
		Init,
		Run,
	};
	State eState;
	uint16 nMaxBO;	// Input
	uint16 nMaxBN;	// == meta_MtBlk2PBN(nMaxBO)
	uint16 nCPO;	// Output.
	uint16 nIssued;	// Internal.
	uint16 nDone;	// Internal.
};

static bool open_PageScan_SM(MtPgStk* pCtx)
{
	bool bRet = false;
	if (MtPgStk::Init == pCtx->eState)
	{
		pCtx->nCPO = NUM_WL;
		pCtx->nIssued = 0;
		pCtx->nDone = 0;
		pCtx->eState = MtPgStk::Run;
	}

	// Check phase.
	CmdInfo* pDone;
	while (nullptr != (pDone = IO_PopDone(CbKey::IOCB_Meta)))
	{
		pCtx->nDone++;
		uint16 nBuf = pDone->stRead.anBufId[0];
		uint32* pnSpare = (uint32*)BM_GetSpare(nBuf);
		PRINTF("[OPEN] PageScan (%X,%X) -> Age:%X, Slice:%X\n",
			pDone->anBBN[0], pDone->nWL, pnSpare[0], pnSpare[1]);
		if (MARK_ERS != *pnSpare)
		{
			ASSERT(NUM_WL == pCtx->nCPO);
			gstMetaCtx.nAge = pnSpare[0];
			gstMetaCtx.nNextSlice = (pnSpare[1] + NUM_MAP_SLICE + 1) % NUM_MAP_SLICE;
		}
		else if (NUM_WL == pCtx->nCPO)
		{
			pCtx->nCPO = pDone->nTag * PAGE_PER_META;
		}
		BM_Free(nBuf);
		IO_Free(pDone);
	}

	while ((NUM_WL == pCtx->nCPO)
		&& (pCtx->nIssued < NUM_WL / PAGE_PER_META)
		&& ((pCtx->nIssued - pCtx->nDone) < 2))
	{
		uint16 nWL = pCtx->nIssued * PAGE_PER_META;
		uint16 nBuf = BM_Alloc();
		CmdInfo* pCmd = IO_Alloc(IOCB_Meta);
		IO_Read(pCmd, pCtx->nMaxBN, nWL, nBuf, pCtx->nIssued);
		pCtx->nIssued++;
	}

	if (pCtx->nIssued > pCtx->nDone)
	{
		Sched_Wait(BIT(EVT_NAND_CMD), LONG_TIME);
	}
	else if (((pCtx->nCPO != NUM_WL) || (pCtx->nDone >= NUM_WL / PAGE_PER_META)))
	{
		PRINTF("[OPEN] Clean MtPage (%X,%X))\n", pCtx->nMaxBN, pCtx->nCPO);
		bRet = true;
	}

	return bRet;
}

static void open_ReplayJnl(JnlSet* pJnlSet, uint32 nAge)
{
	PRINTF("[OPEN] Replay Jnl Age:%d, Cnt: %d, start with %X\n", nAge, pJnlSet->nCnt, pJnlSet->aJnl[0].Com.nValue);
	for (uint16 nIdx = 0; nIdx < pJnlSet->nCnt; nIdx++)
	{
		Jnl* pJnl = pJnlSet->aJnl + nIdx;
		switch (pJnl->Com.eJType)
		{
			case Jnl::JT_UserW:
			{
				META_Update(pJnl->Wrt.nLPN, pJnl->Wrt.stAddr, OpenType::OPEN_USER, true);
				OpenBlk* pOpen = META_GetOpen(OpenType::OPEN_USER);
				pOpen->stNextVA = pJnl->Wrt.stAddr;
				pOpen->stNextVA.nWL++;
				break;
			}
			case Jnl::JT_GcW:
			{
				META_Update(pJnl->Wrt.nLPN, pJnl->Wrt.stAddr, OpenType::OPEN_GC, true);
				OpenBlk* pOpen = META_GetOpen(OpenType::OPEN_GC);
				pOpen->stNextVA = pJnl->Wrt.stAddr;
				pOpen->stNextVA.nWL++;
				break;
			}
			case Jnl::JT_ERB:
			{
				OpenBlk* pOpen;
				if (OpenType::OPEN_GC == pJnl->Erb.eOpenType)
				{
					pOpen = META_GetOpen(OpenType::OPEN_GC);
				}
				else
				{
					pOpen = META_GetOpen(OpenType::OPEN_USER);
				}
				pOpen->stNextVA.nBN = pJnl->Erb.nBN;
				pOpen->stNextVA.nWL = 0;
				break;
			}
			default:
			{
				ASSERT(false);
			}
		}
	}
}

static bool open_MtLoad_SM(MtPgStk* pCtx)
{
	bool bRet = false;
	if (MtPgStk::Init == pCtx->eState)
	{
		if (pCtx->nCPO >= NUM_MAP_SLICE)
		{
			pCtx->nCPO -= NUM_MAP_SLICE;
		}
		else
		{
			pCtx->nCPO = (pCtx->nCPO + NUM_WL - NUM_MAP_SLICE) % NUM_WL;
			pCtx->nMaxBO = (pCtx->nMaxBO + NUM_META_BLK - 1) % NUM_META_BLK;
			pCtx->nMaxBN = meta_MtBlk2PBN(pCtx->nMaxBO);
		}
		pCtx->nIssued = 0;
		pCtx->nDone = 0;
		pCtx->eState = MtPgStk::Run;
		MEMSET_OBJ(gstMeta, 0xFF);	// initialize as FF32;
	}

	// Check phase.
	CmdInfo* pDone;
	while (nullptr != (pDone = IO_PopDone(CbKey::IOCB_Meta)))
	{
		pCtx->nDone++;
		uint16 nBuf = pDone->stRead.anBufId[0];
		uint8* pMain = BM_GetMain(nBuf);
		uint32* pSpare = (uint32*)BM_GetSpare(nBuf);
		uint32 nSlice = pSpare[1];
		PRINTF("[OPEN] Mt Loaded {%X,%X} (%d,%d)\n", pDone->anBBN[0], pDone->nWL, pSpare[0], pSpare[1]);
		ASSERT(nSlice < NUM_MAP_SLICE);
		open_ReplayJnl((JnlSet*)pMain, pSpare[0]);
		uint8* pSrc = pMain + sizeof(JnlSet);
		uint8* pDst = (uint8*)(&gstMeta) + (nSlice * SIZE_MAP_PER_SAVE);
		uint32 nSize = SIZE_MAP_PER_SAVE;
		if (nSlice >= (NUM_MAP_SLICE - 1))
		{
			nSize = sizeof(gstMeta) - (nSlice * SIZE_MAP_PER_SAVE);
		}
		memcpy(pDst, pSrc, nSize);
		BM_Free(nBuf);
		IO_Free(pDone);
	}

	while ((pCtx->nIssued < NUM_MAP_SLICE) && (pCtx->nIssued - pCtx->nDone < 2))
	{
		uint16 nWL = pCtx->nCPO + pCtx->nIssued;
		uint16 nBN = pCtx->nMaxBN;
		if (nWL >= NUM_WL)
		{
			nWL = nWL % NUM_WL;
			nBN = meta_MtBlk2PBN((pCtx->nMaxBO + 1) % NUM_META_BLK);
		}
		uint16 nBuf = BM_Alloc();
		CmdInfo* pCmd = IO_Alloc(IOCB_Meta);
		IO_Read(pCmd, nBN, nWL, nBuf, pCtx->nIssued);
		pCtx->nIssued++;
		Sched_Wait(BIT(EVT_NAND_CMD), LONG_TIME);
	}

	if (pCtx->nDone >= NUM_MAP_SLICE)
	{
		bRet = true;
	}
	else if (pCtx->nIssued > pCtx->nDone)
	{
		Sched_Wait(BIT(EVT_NAND_CMD), LONG_TIME);
	}
	return bRet;
}


// ========================== Meta Block Scan ====================================
struct MtBlkScanStk
{
	enum State
	{
		Init,
		Run,
	};
	State eState;
	uint16 nMaxBO;	// for return.
	uint8 nIssued;
	uint8 nDone;
	uint32 nMaxAge;
};

static bool open_BlkScan_SM(MtBlkScanStk* pCtx)
{
	bool bRet = false;
	if (MtBlkScanStk::Init == pCtx->eState)
	{
		pCtx->nMaxBO = INV_BN;
		pCtx->nIssued = 0;
		pCtx->nDone = 0;
		pCtx->nMaxAge = 0;
		pCtx->eState = MtBlkScanStk::Run;
	}
	// Issue phase.
	while ((pCtx->nIssued < NUM_META_BLK) && ((pCtx->nIssued - pCtx->nDone) < 2))
	{
		uint16 nBuf = BM_Alloc();
		CmdInfo* pCmd;
		uint16 nBN = meta_MtBlk2PBN(pCtx->nIssued);
		pCmd = IO_Alloc(IOCB_Meta);
		IO_Read(pCmd, nBN, 0, nBuf, pCtx->nIssued);
		PRINTF("[OPEN] BlkScan Issue %X\n", pCtx->nIssued);
		pCtx->nIssued++;
	}
	// Check phase.
	CmdInfo* pDone;
	while (nullptr != (pDone = IO_PopDone(CbKey::IOCB_Meta)))
	{
		pCtx->nDone++;
		uint16 nBuf = pDone->stRead.anBufId[0];
		uint32* pnSpare = (uint32*)BM_GetSpare(nBuf);

		PRINTF("[OPEN] BlkScan BO:%2X, SPR:%2X\n", pDone->nTag, *pnSpare);

		if ((*pnSpare > pCtx->nMaxAge) && (*pnSpare != MARK_ERS))
		{
			pCtx->nMaxAge = *pnSpare;
			pCtx->nMaxBO = pDone->nTag;
		}
		BM_Free(nBuf);
		IO_Free(pDone);
	}

	if (NUM_META_BLK == pCtx->nDone)	// All done.
	{
		PRINTF("[OPEN] Latest Blk Offset: %X\n", pCtx->nMaxBO);
		bRet = true;
	}
	else if (pCtx->nIssued > pCtx->nDone)
	{
		Sched_Wait(BIT(EVT_NAND_CMD), LONG_TIME);
	}
	else
	{
		Sched_Yield();
	}

	return bRet;
}


// =====================================================


static void open_PostMtLoad()
{
	uint16 anVPC[NUM_USER_BLK];
	MEMSET_ARRAY(anVPC, 0x0);
	for (uint32 nLPN = 0; nLPN < NUM_LPN; nLPN++)
	{
		if (gstMeta.astL2P[nLPN].nBN < NUM_USER_BLK)
		{
			anVPC[gstMeta.astL2P[nLPN].nBN]++;
		}
	}
	for (uint16 nBN = 0; nBN < NUM_USER_BLK; nBN++)
	{
//		assert(gstMeta.astBI[nBN].nVPC == anVPC[nBN]);
		gstMeta.astBI[nBN].nVPC = anVPC[nBN];
		gstMeta.astBI[nBN].eState = BlkState::BS_Closed;
	}
	for (uint32 nOpen = 0; nOpen < NUM_OPEN; nOpen++)
	{
		if (gaOpen[nOpen].stNextVA.nWL < NUM_WL)
		{
			META_SetOpen((OpenType)nOpen, gaOpen[nOpen].stNextVA.nBN, gaOpen[nOpen].stNextVA.nWL);
		}
	}
}

struct OpenStk
{
	enum MetaStep
	{
		Init,
		BlkScan,
		PageScan,
		MtLoad,
		GcScan,
		UserScan,
	};
	MetaStep eOpenStep;
	uint16 nMaxBO;	// for return.
};

static bool meta_Open_SM(OpenStk* pCtx)
{
	bool bRet = false;

	switch (pCtx->eOpenStep)
	{
		case OpenStk::Init:
		{
			MtBlkScanStk* pChildCtx = (MtBlkScanStk*)(pCtx + 1);
			pChildCtx->eState = MtBlkScanStk::Init;
			open_BlkScan_SM(pChildCtx);
			MEMSET_OBJ(gstMeta, 0xFF);
			pCtx->nMaxBO = INV_BN;
			pCtx->eOpenStep = OpenStk::BlkScan;
			break;
		}
		case OpenStk::BlkScan:
		{
			MtBlkScanStk* pChildCtx = (MtBlkScanStk*)(pCtx + 1);
			if (open_BlkScan_SM(pChildCtx))
			{
				pCtx->nMaxBO = pChildCtx->nMaxBO;
				if (INV_BN != pCtx->nMaxBO)
				{
					MtPgStk* pNextChild = (MtPgStk*)(pCtx + 1);
					pNextChild->nMaxBN = meta_MtBlk2PBN(pCtx->nMaxBO);
					pNextChild->eState = MtPgStk::Init;
					open_PageScan_SM(pNextChild);
					pCtx->eOpenStep = OpenStk::PageScan;
				}
				else
				{	// All done in this function.
					bRet = true;
				}
			}
			break;
		}
		case OpenStk::PageScan:
		{
			MtPgStk* pChildCtx = (MtPgStk*)(pCtx + 1);
			if (open_PageScan_SM(pChildCtx))
			{
				if (pChildCtx->nCPO != NUM_WL)
				{
					gstMetaCtx.nCurBO = pChildCtx->nMaxBO;
					gstMetaCtx.nCurBN = meta_MtBlk2PBN(gstMetaCtx.nCurBO);
					gstMetaCtx.nNextWL = pChildCtx->nCPO;
				}
				else
				{
					gstMetaCtx.nCurBO = (pChildCtx->nMaxBO + 1) % NUM_META_BLK;
					gstMetaCtx.nCurBN = meta_MtBlk2PBN(gstMetaCtx.nCurBO);
					gstMetaCtx.nNextWL = 0;
					PRINTF("[OPEN] Mt Blk boundary {%X,%X}\n", gstMetaCtx.nCurBN, gstMetaCtx.nNextWL);
				}

				pChildCtx->eState = MtPgStk::Init;
				open_MtLoad_SM(pChildCtx);
				pCtx->eOpenStep = OpenStk::MtLoad;
			}
			break;
		}
		case OpenStk::MtLoad:
		{
			MtPgStk* pChildCtx = (MtPgStk*)(pCtx + 1);
			if (open_MtLoad_SM(pChildCtx))
			{
				open_PostMtLoad();
				gstJnlSet.Start(OPEN_USER, 0);
				DataScanStk* pNextChild = (DataScanStk*)(pCtx + 1);
				pNextChild->eState = DataScanStk::Init;
				pNextChild->eOpen = OpenType::OPEN_GC;
				if (true == open_UserScan_SM(pNextChild))
				{
					Sched_Yield(); // No scan target.
				}
				pCtx->eOpenStep = OpenStk::GcScan;
			}
			break;
		}
		case OpenStk::GcScan:
		{
			if (open_UserScan_SM((DataScanStk*)(pCtx + 1)))
			{
				DataScanStk* pNextChild = (DataScanStk*)(pCtx + 1);
				pNextChild->eState = DataScanStk::Init;
				pNextChild->eOpen = OpenType::OPEN_USER;
				if (true == open_UserScan_SM(pNextChild))
				{
					Sched_Yield(); // No scan target.
				}
				pCtx->eOpenStep = OpenStk::UserScan;
			}
			break;
		}
		case OpenStk::UserScan:
		{
			if (open_UserScan_SM((DataScanStk*)(pCtx + 1)))
			{
				bRet = true;	// all done.
			}
		}
	}
	return bRet;
}


void meta_Run(void* pParam)
{
	MtStk* pMtStk = (MtStk*)pParam;
	switch (pMtStk->eStep)
	{
		case MtStk::Mt_Init:
		{
			OpenStk* pOpenStk = (OpenStk*)(pMtStk + 1);
			pOpenStk->eOpenStep = OpenStk::Init;
			meta_Open_SM(pOpenStk);
			pMtStk->eStep = MtStk::Mt_Open;
			break;
		}

		case MtStk::Mt_Open:
		{
			OpenStk* pOpenStk = (OpenStk*)(pMtStk + 1);
			if (meta_Open_SM(pOpenStk))
			{
				if (INV_BN == pOpenStk->nMaxBO)
				{
					FmtCtx* pNextCtx = (FmtCtx*)(pMtStk + 1);
					if (meta_Format(pNextCtx, true))
					{
						pMtStk->eStep = MtStk::Mt_Ready;
						Sched_TrigSyncEvt(BIT(EVT_OPEN));
						Sched_Yield();
					}
					else
					{
						pMtStk->eStep = MtStk::Mt_Format;
					}
					Sched_Yield();
				}
				else
				{
					pMtStk->eStep = MtStk::Mt_Ready;
					Sched_TrigSyncEvt(BIT(EVT_OPEN));
					Sched_Yield();
				}
			}
			break;
		}
		case MtStk::Mt_Format:
		{
			FmtCtx* pFmtStk = (FmtCtx*)(pMtStk + 1);
			if(meta_Format(pFmtStk, false))
			{
				pMtStk->eStep = MtStk::Mt_Ready;
				Sched_TrigSyncEvt(BIT(EVT_OPEN));
				Sched_Yield();
			}
			break;
		}
		case MtStk::Mt_Ready:
		{
			if (gbRequest)
			{
				gbRequest = false;
				MtSaveStk* pSaveStk = (MtSaveStk*)(pMtStk + 1);
				pSaveStk->eStep = MtSaveStk::Init;
				meta_Save_SM(pSaveStk);
				pMtStk->eStep = MtStk::Mt_Saving;
			}
			else
			{
				Sched_Wait(BIT(EVT_META), LONG_TIME);
			}
			break;
		}
		case MtStk::Mt_Saving:
		{
			MtSaveStk* pSaveStk = (MtSaveStk*)(pMtStk + 1);
			if (meta_Save_SM(pSaveStk))
			{
				META_StartJnl(OPEN_GC, 0);
				Sched_TrigSyncEvt(BIT(EVT_META));
				pMtStk->eStep = MtStk::Mt_Ready;
				Sched_Yield();
			}
			break;
		}
		default:
		{
			assert(false);
		}
	}
}

uint32 META_GetAge()
{
	return gstMetaCtx.nAge;
}

uint32 META_ReqSave()
{
	gbRequest = true;
	Sched_TrigSyncEvt(BIT(EVT_META));
	return gstMetaCtx.nAge;
}

static uint8 aMtStack[0x40];		///< Stack like meta context.
void META_Init()
{
	MEMSET_ARRAY(aMtStack, 0xCD);
	gpMtStk = (MtStk*)aMtStack;
	gpMtStk->eStep = MtStk::Mt_Init;

	MEMSET_ARRAY(gaOpen, 0xFF);

	MEMSET_ARRAY(gstMeta.astBI, 0);
	MEMSET_ARRAY(gstMeta.astL2P, 0xFF);
	MEMSET_OBJ(gstMetaCtx, 0);
	Sched_Register(TID_META, meta_Run, aMtStack, BIT(MODE_NORMAL));
}
