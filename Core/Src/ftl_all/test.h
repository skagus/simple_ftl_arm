
#pragma once
#include "config.h"

#if (EN_WORK_GEN == 0)
void TEST_Main(void* pParam);	// Stand alone test.
#else
void TEST_Init();		// for Workload generator.
#endif