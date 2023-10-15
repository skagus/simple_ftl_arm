	.syntax unified
	.cpu cortex-m3
	.fpu softvfp
	.thumb

.global os_SetNextTask
.global pCurTCB
.global os_SwitchForARM
/**
 * @brief  This is the code that gets called when the processor first
 *          starts execution following a reset event. Only the absolutely
 *          necessary set is performed, after which the application
 *          supplied main() routine is called.
 * @param  None
 * @retval : None
*/

  .section .text.os_SwitchForARM
  .type os_SwitchForARM, %function
os_SwitchForARM:
	stmdb sp!, {r0,r4-r11, r14}
	ldr r3, =pCurTCB
	ldr r2, [r3]
	str sp, [r2]

	bl os_SetNextTask	// Should change the value of pCurTCB.

	ldr r3, =pCurTCB
	ldr r2, [r3]
	ldr sp, [r2]
	ldmia sp!, {r0,r4-r11, r14}
	isb
	bx r14
