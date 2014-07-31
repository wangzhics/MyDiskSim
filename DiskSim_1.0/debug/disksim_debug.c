/*
 * disksim_debug.c
 *
 *  Created on: Jul 5, 2014
 *      Author: WangZhi
 */
#include <stdio.h>
#include "disksim_exec.h"
#include "disksim_rms.h"

int main()
{
	printf("HP_C2247_validate (rms should be about 0.090)\n");
	disksim_exec("parv.hpc2247", "outv.hpc2247", "validate", "trace.hpc2247", "0");
	disksim_rms("outv.hpc2247", "outv.hpc2247", "-1", "-1", "1");
	return 0;
}
