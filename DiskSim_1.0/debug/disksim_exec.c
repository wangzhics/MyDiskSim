/*
 * disksim_exec.c
 *
 *  Created on: Jul 4, 2014
 *      Author: WangZhi
 */
#include "disksim_exec.h"
#include "../src/disksim_global.h"
#include "../src/disksim_ioface.h"
#include "../src/disksim_pfface.h"

int external_control = 0;
void (*external_io_done_notify)(ioreq_event *curr) = NULL;

extern void intr_acknowledge();
extern void iotrace_initialize_file();
extern void iotrace_set_format();

extern int closedios;
extern double closedthinktime;

int endian = _LITTLE_ENDIAN;
int traceformat = ASCII;
int traceendian = _LITTLE_ENDIAN;
int traceheader = TRUE;
int print_tracefile_header = FALSE;

FILE *parfile = NULL;
FILE *statdeffile = NULL;
FILE *iotracefile = NULL;
FILE *outputfile = NULL;
FILE *outios = NULL;

FILE *debugFile = NULL;

int iotrace = 0;
int synthgen = 0;

event *intq = NULL;
event *extraq = NULL;
int intqlen = 0;
int extraqlen = 0;

double simtime = 0.0;
int stop_sim = FALSE;

int seedval;

int warmup_iocnt = 0;
double warmuptime = 0.0;
timer_event *warmup_event = NULL;


/*
#define NULL_EVENT      	0
#define PF_MIN_EVENT    	1
#define PF_MAX_EVENT    	97
#define INTR_EVENT			98
#define INTEND_EVENT		99
#define IO_MIN_EVENT		100
#define IO_MAX_EVENT		120
#define TIMER_EXPIRED		121

#define IO_REQUEST_ARRIVE			100
#define IO_ACCESS_ARRIVE			101
#define IO_INTERRUPT_ARRIVE			102
#define IO_RESPOND_TO_DEVICE			103
#define IO_ACCESS_COMPLETE			104
#define IO_INTERRUPT_COMPLETE			105
#define DISK_OVERHEAD_COMPLETE			106
#define DISK_PREPARE_FOR_DATA_TRANSFER		107
#define DISK_DATA_TRANSFER_COMPLETE		108
#define DISK_BUFFER_SEEKDONE			109
#define DISK_BUFFER_TRACKACC_DONE		110
#define DISK_BUFFER_SECTOR_DONE			111
#define DISK_GOT_REMAPPED_SECTOR		112
#define DISK_GOTO_REMAPPED_SECTOR		113
#define BUS_OWNERSHIP_GRANTED			114
#define BUS_DELAY_COMPLETE			115
#define CONTROLLER_DATA_TRANSFER_COMPLETE	116
#define TIMESTAMP_LOGORG			117
#define IO_TRACE_REQUEST_START			118
#define IO_QLEN_MAXCHECK			119

#define SLEEP_EVENT             3
#define WAKEUP_EVENT            5
#define IOREQ_EVENT             9
#define IOACC_EVENT             10

#define CPU_EVENT	        50
#define SYNTHIO_EVENT		51
#define IDLELOOP_EVENT		52
#define CSWITCH_EVENT		53
 *
 */
char* getEventType(int code) {
	switch(code){
		// >=IO_MIN_EVENT && <= IO_MAX_EVENT
		case 100: return "IO_REQUEST_ARRIVE";
		case 101: return "IO_ACCESS_ARRIVE";
		case 102: return "IO_INTERRUPT_ARRIVE";
		case 103: return "IO_RESPOND_TO_DEVICE";
		case 104: return "IO_ACCESS_COMPLETE";
		case 105: return "IO_INTERRUPT_COMPLETE";
		case 106: return "DISK_OVERHEAD_COMPLETE";
		case 107: return "DISK_PREPARE_FOR_DATA_TRANSFER";
		case 108: return "DISK_DATA_TRANSFER_COMPLETE";
		case 109: return "DISK_BUFFER_SEEKDONE";
		case 110: return "DISK_BUFFER_TRACKACC_DONE";
		case 111: return "DISK_BUFFER_SECTOR_DONE";
		case 112: return "DISK_GOT_REMAPPED_SECTOR";
		case 113: return "DISK_GOTO_REMAPPED_SECTOR";
		case 114: return "BUS_OWNERSHIP_GRANTED";
		case 115: return "BUS_DELAY_COMPLETE";
		case 116: return "CONTROLLER_DATA_TRANSFER_COMPLETE";
		case 117: return "TIMESTAMP_LOGORG";
		case 118: return "IO_TRACE_REQUEST_START";
		case 119: return "IO_QLEN_MAXCHECK";
		// >= PF_MIN_EVENT && <= PF_MAX_EVENT
		case 3: return "SLEEP_EVENT";
		case 5: return "WAKEUP_EVENT";
		case 9: return "IOREQ_EVENT";
		case 10: return "IOACC_EVENT";
		case 50: return "CPU_EVENT";
		case 51: return "SYNTHIO_EVENT";
		case 52: return "IDLELOOP_EVENT";
		case 53: return "CSWITCH_EVENT";
		// OTHERS
		case 0: return "NULL_EVENT";
		case 98: return "INTR_EVENT";
		case 99: return "INTEND_EVENT";
		case 121: return "TIMER_EXPIRED";
		default: return "";
	}
}


void allocateextra() {
	int i;
	event *temp = NULL;

	StaticAssert (sizeof(event) == DISKSIM_EVENT_SIZE);
	if ((temp = (event *)malloc(ALLOCSIZE)) == NULL) {
		fprintf (stderr, "Error allocating space for events\n");
		exit(0);
	}
	for (i=0; i<((ALLOCSIZE/DISKSIM_EVENT_SIZE)-1); i++) {
		temp[i].next = &temp[i+1];
	}
	temp[((ALLOCSIZE/DISKSIM_EVENT_SIZE)-1)].next = NULL;
	extraq = temp;
	extraqlen = ALLOCSIZE / DISKSIM_EVENT_SIZE;
}

void addtoextraq(temp)
	event *temp; {

	fprintf(debugFile, "addtoextraq:\t %s\t%s\t%f\t%d\t%d\n", getEventType(temp->type), temp->space, temp->time, temp->temp, temp->type);
	fflush (debugFile);

	if (temp == NULL) {
		return;
	}
	temp->next = extraq;
	temp->prev = NULL;
	extraq = temp;
	extraqlen++;
}

event * getfromextraq() {
	event *temp = NULL;

	temp = extraq;
	if (extraqlen == 0) {
		allocateextra();
		temp = extraq;
		extraq = extraq->next;
	} else if (extraqlen == 1) {
		extraq = NULL;
	} else {
		extraq = extraq->next;
	}
	extraqlen--;
	temp->next = NULL;
	temp->prev = NULL;
	return (temp);
}

void addlisttoextraq(headptr)
	event **headptr; {
	event *tmp;

	while ((tmp = *headptr)) {
		*headptr = tmp->next;
		addtoextraq(tmp);
	}
	*headptr = NULL;
}

event *event_copy(orig)
	event *orig; {
	event *new = getfromextraq();
	bcopy((char *) orig, (char *) new, DISKSIM_EVENT_SIZE);
	return ((event *) new);
}

void printintq() {
	event *tmp;
	int i = 0;

	tmp = intq;
	while (tmp != NULL) {
		i++;
		fprintf(outputfile, "Item #%d: time %f, type %d\n", i, tmp->time,
				tmp->type);
		tmp = tmp->next;
	}
}

void addtointq(temp)
	event *temp; {

	fprintf(debugFile, "addtointq:\t %s\t%s\t%f\t%d\t%d\n", getEventType(temp->type), temp->space, temp->time, temp->temp, temp->type);
	fflush (debugFile);

	event *run = NULL;

	if ((temp->time + 0.0001) < simtime) {
		fprintf(stderr,
				"Attempting to addtointq an event whose time has passed\n");
		fprintf(stderr, "simtime %f, curr->time %f, type = %d\n", simtime,
				temp->time, temp->type);
		exit(0);
	}

	if (intq == NULL) {
		intq = temp;
		temp->next = NULL;
		temp->prev = NULL;
	} else if (temp->time < intq->time) {
		temp->next = intq;
		intq->prev = temp;
		intq = temp;
		temp->prev = NULL;
	} else {
		run = intq;
		while (run->next != NULL) {
			if (temp->time < run->next->time) {
				break;
			}
			run = run->next;
		}
		temp->next = run->next;
		run->next = temp;
		temp->prev = run;
		if (temp->next != NULL) {
			temp->next->prev = temp;
		}
	}
}

event * getfromintq() {
	event *temp = NULL;

	if (intq == NULL) {
		return (NULL);
	}
	temp = intq;
	intq = intq->next;
	if (intq != NULL) {
		intq->prev = NULL;
	}
	temp->next = NULL;
	temp->prev = NULL;
	return (temp);
}

int removefromintq(curr)
	event *curr; {
	event *tmp;

	tmp = intq;
	while (tmp != NULL) {
		if (tmp == curr) {
			break;
		}
		tmp = tmp->next;
	}
	if (tmp == NULL) {
		return (FALSE);
	}
	if (curr->next != NULL) {
		curr->next->prev = curr->prev;
	}
	if (curr->prev == NULL) {
		intq = curr->next;
	} else {
		curr->prev->next = curr->next;
	}
	curr->next = NULL;
	curr->prev = NULL;
	return (TRUE);
}

void scanparam_int(parline, parname, parptr, parchecks, parminval, parmaxval)
	char *parline;char *parname;int *parptr;int parchecks;int parminval;int
			parmaxval; {
	if (sscanf(parline, "%d", parptr) != 1) {
		fprintf(stderr, "Error reading '%s'\n", parname);
		exit(0);
	}
	if (((parchecks & 1) && (*parptr < parminval)) || ((parchecks & 2)
			&& (*parptr > parmaxval))) {
		fprintf(stderr, "Invalid value for '%s': %d\n", parname, *parptr);
		exit(0);
	}
}

void getparam_int(parfile, parname, parptr, parchecks, parminval, parmaxval)
	FILE *parfile;char *parname;int *parptr;int parchecks;int parminval;int
			parmaxval; {
	char line[201];

	sprintf(line, "%s: %s\n", parname, "%d");
	if (fscanf(parfile, line, parptr) != 1) {
		fprintf(stderr, "Error reading '%s'\n", parname);
		exit(0);
	}
	if (((parchecks & 1) && (*parptr < parminval)) || ((parchecks & 2)
			&& (*parptr > parmaxval))) {
		fprintf(stderr, "Invalid value for '%s': %d\n", parname, *parptr);
		exit(0);
	}
	fprintf(outputfile, "%s: %d\n", parname, *parptr);
}

void getparam_double(parfile, parname, parptr, parchecks, parminval, parmaxval)
	FILE *parfile;char *parname;double *parptr;int parchecks;double parminval;double
			parmaxval; {
	char line[201];

	sprintf(line, "%s: %s\n", parname, "%lf");
	if (fscanf(parfile, line, parptr) != 1) {
		fprintf(stderr, "Error reading '%s'\n", parname);
		exit(0);
	}
	if (((parchecks & 1) && (*parptr < parminval)) || ((parchecks & 2)
			&& (*parptr > parmaxval)) || ((parchecks & 4) && (*parptr
			<= parminval))) {
		fprintf(stderr, "Invalid value for '%s': %f\n", parname, *parptr);
		exit(0);
	}
	fprintf(outputfile, "%s: %f\n", parname, *parptr);
}

void getparam_bool(parfile, parname, parptr)
	FILE *parfile;char *parname;int *parptr; {
	char line[201];

	sprintf(line, "%s %s\n", parname, "%d");
	if (fscanf(parfile, line, parptr) != 1) {
		fprintf(stderr, "Error reading '%s'\n", parname);
		exit(0);
	}
	if ((*parptr != TRUE) && (*parptr != FALSE)) {
		fprintf(stderr, "Invalid value for '%s': %d\n", parname, *parptr);
		exit(0);
	}
	fprintf(outputfile, "%s %d\n", parname, *parptr);
}

void resetstats() {
	if (external_control | synthgen | iotrace) {
		io_resetstats();
	}
	if (synthgen) {
		pf_resetstats();
	}
}

void stat_warmup_done(timer)
	event *timer; {
	warmuptime = simtime;
	resetstats();
	addtoextraq(timer);
}

void readparams(parfile)
	FILE *parfile; {
	char seed[200];
	double warmup;
	int newend = -1;

	if (fscanf(parfile, "\nByte Order (Endian): %s\n", seed) != 1) {
		fprintf(stderr, "Error reading 'Byte Order:'\n");
		exit(0);
	}
	if (strcmp(seed, "Little") == 0) {
		endian = _LITTLE_ENDIAN;
	} else if (strcmp(seed, "Big") == 0) {
		endian = _BIG_ENDIAN;
	} else {
		fprintf(stderr, "Invalid byte ordering scheme - %s\n", seed);
		exit(0);
	}

	{
		int tmp = 0x11223344;
		char *tmp2 = (char *) &tmp;
		newend = (tmp2[0] == 0x44) ? _LITTLE_ENDIAN
				: ((tmp2[0] == 0x11) ? _BIG_ENDIAN : -1);
	}

	if (newend != -1) {
		fprintf(outputfile,
				"Byte Order (Endian): %s (using detected value: %s)\n", seed,
				((newend == _LITTLE_ENDIAN) ? "Little" : "Big"));
		endian = newend;
	} else {
		fprintf(outputfile, "Byte Order (Endian): %s\n", seed);
	}

	if (fscanf(parfile, "Init Seed: %s\n", seed) != 1) {
		fprintf(stderr, "Error reading 'Seed:'\n");
		exit(0);
	}
	if (strcmp(seed, "PID") == 0) {
		srand48((int) getpid());
		fprintf(outputfile, "Init Seed: PID (%d)\n", (int) getpid());
	} else if (sscanf(seed, "%d", &seedval) != 1) {
		fprintf(stderr, "Invalid init seed value: %s\n", seed);
		exit(0);
	} else {
		srand48(seedval);
		fprintf(outputfile, "Init Seed: %d\n", seedval);
	}

	if (fscanf(parfile, "Real Seed: %s\n", seed) != 1) {
		fprintf(stderr, "Error reading 'Seed:'\n");
		exit(0);
	}
	if (strcmp(seed, "PID") == 0) {
		seedval = getpid();
		fprintf(outputfile, "Real Seed: PID\n");
	} else if (sscanf(seed, "%d", &seedval) != 1) {
		fprintf(stderr, "Invalid init seed value: %s\n", seed);
		exit(0);
	} else {
		fprintf(outputfile, "Real Seed: %d\n", seedval);
	}

	if (fscanf(parfile, "Statistic warm-up period: %lf %s\n", &warmup, seed)
			!= 2) {
		fprintf(stderr, "Error reading 'Statistic warm-up period:'\n");
		exit(0);
	}
	fprintf(outputfile, "Statistic warm-up period: %f %s\n", warmup, seed);
	if (warmup > 0.0) {
		if (strcmp(seed, "seconds") == 0) {
			warmup_event = (timer_event *) getfromextraq();
			warmup_event->type = TIMER_EXPIRED;
			warmup_event->time = warmup * (double) 1000.0;
			warmup_event->func = stat_warmup_done;
		} else if (strcmp(seed, "I/Os") == 0) {
			warmup_iocnt = ceil(warmup) + 1;
		} else {
			fprintf(stderr, "Invalid type of statistic warm-up period: %s\n",
					seed);
			exit(0);
		}
	}

	if (fscanf(parfile, "Stat (dist) definition file: %s\n", seed) != 1) {
		fprintf(stderr, "Error reading 'Stat (dist) definition file:'\n");
		exit(0);
	}
	if ((statdeffile = fopen(seed, "r")) == NULL) {
		fprintf(stderr, "Statdeffile %s cannot be opened for read access\n",
				seed);
		exit(0);
	}
	fprintf(outputfile, "Stat (dist) definition file: %s\n", seed);

	if (fscanf(parfile,
			"Output file for trace of I/O requests simulated: %s\n", seed) != 1) {
		fprintf(stderr,
				"Error reading 'Output file for I/O requests simulated:'\n");
		exit(0);
	}
	if ((strcmp(seed, "0") != 0) && (strcmp(seed, "null") != 0)) {
		if ((outios = fopen(seed, "w")) == NULL) {
			fprintf(stderr, "Outios %s cannot be opened for write access\n",
					seed);
			exit(0);
		}
	}
	fprintf(outputfile, "Output file for I/O requests simulated: %s\n", seed);

	if (external_control | synthgen | iotrace) {
		io_readparams(parfile);
	}
	if (synthgen) {
		pf_readparams(parfile);
	}
}

void doparamoverrides(overrides, overcnt)
	char **overrides;int overcnt; {
	int i;
	int first, last;
	int vals;

	for (i = 0; i < overcnt; i += 4) {
		vals = sscanf(overrides[(i + 1)], "%d-%d", &first, &last);
		if ((vals != 2) && ((vals != 1) || (first != -1))) {
			fprintf(stderr, "Invalid format for range in doparamoverrides\n");
			exit(0);
		}
		if (strcmp(overrides[i], "iodriver") == 0) {
			io_param_override(IODRIVER, overrides[(i + 2)], overrides[(i + 3)],
					first, last);
		} else if (strcmp(overrides[i], "iosim") == 0) {
			io_param_override(IOSIM, overrides[(i + 2)], overrides[(i + 3)],
					first, last);
		} else if (strcmp(overrides[i], "sysorg") == 0) {
			io_param_override(SYSORG, overrides[(i + 2)], overrides[(i + 3)],
					first, last);
		} else if (strcmp(overrides[i], "synthio") == 0) {
			pf_param_override(SYNTHIO, overrides[(i + 2)], overrides[(i + 3)],
					first, last);
		} else if (strcmp(overrides[i], "disk") == 0) {
			io_param_override(DISK, overrides[(i + 2)], overrides[(i + 3)],
					first, last);
		} else if (strcmp(overrides[i], "controller") == 0) {
			io_param_override(CONTROLLER, overrides[(i + 2)],
					overrides[(i + 3)], first, last);
		} else if (strcmp(overrides[i], "bus") == 0) {
			io_param_override(BUS, overrides[(i + 2)], overrides[(i + 3)],
					first, last);
		} else {
			fprintf(stderr,
					"Structure with parameter to override not supported: %s\n",
					overrides[i]);
			exit(0);
		}
		fprintf(outputfile, "Override: %s %s %s %s\n", overrides[i],
				overrides[(i + 1)], overrides[(i + 2)], overrides[(i + 3)]);
	}
}

void printstats() {
	fprintf(outputfile, "\nSIMULATION STATISTICS\n");
	fprintf(outputfile, "---------------------\n\n");
	fprintf(outputfile, "Total time of run:       %f\n\n", simtime);
	fprintf(outputfile, "Warm-up time:            %f\n\n", warmuptime);

	if (synthgen) {
		pf_printstats();
	}
	if (external_control | synthgen | iotrace) {
		io_printstats();
	}
}

void initialize() {
	int val = (synthgen) ? 0 : 1;

	iotrace_initialize_file(iotracefile, traceformat, print_tracefile_header);
	while (intq) {
		addtoextraq(getfromintq());
	}
	if (external_control | synthgen | iotrace) {
		io_initialize(val);
	}
	if (synthgen) {
		pf_initialize(seedval);
	} else {
		srand48(seedval);
	}
	simtime = 0.0;
}

void cleanstats() {
	if (external_control | synthgen | iotrace) {
		io_cleanstats();
	}
	if (synthgen) {
		pf_cleanstats();
	}
}

void set_external_io_done_notify(void(*func)(ioreq_event *)) {
	external_io_done_notify = func;
}

event *io_done_notify(curr)
	ioreq_event *curr; {
	if (synthgen) {
		return (pf_io_done_notify(curr));
	}
	if (external_control) {
		external_io_done_notify(curr);
	}
	return (NULL);
}

event * getnextevent() {
	event *curr = getfromintq();
	event *temp;

	if (curr) {
		simtime = curr->time;
	}
	if ((curr) && (curr->type == NULL_EVENT)) {
		if ((iotrace) && io_using_external_event(curr)) {
			if ((temp = io_get_next_external_event(iotracefile)) == NULL) {
				stop_sim = TRUE;
			}
		} else {
			fprintf(stderr, "NULL_EVENT not linked to any external source\n");
			exit(0);
		}
		if (temp) {
			temp->type = NULL_EVENT;
			addtointq(temp);
		}
	}
	if (curr == NULL) {
		fprintf(outputfile, "Returning NULL from getnextevent\n");
		fflush(outputfile);
	}
	return (curr);
}

void simstop() {
	stop_sim = TRUE;
}

void prime_simulation() {
	event *curr;

	if (warmup_event) {
		addtointq(warmup_event);
		warmup_event = NULL;
	}
	if (iotrace) {
		if ((curr = io_get_next_external_event(iotracefile)) == NULL) {
			cleanstats();
			return;
		}
		if ((traceformat != VALIDATE) && (closedios == 0)) {
			curr->type = NULL_EVENT;
		} else if (closedios) {
			int iocnt;
			io_using_external_event(curr);
			curr->time = simtime + closedthinktime;
			for (iocnt = 1; iocnt < closedios; iocnt++) {
				curr->next = io_get_next_external_event(iotracefile);
				if (curr->next) {
					io_using_external_event(curr->next);
					curr->time = simtime + closedthinktime;
					addtointq(curr->next);
				}
			}
		}
		addtointq(curr);
	}
}

void disksim_simulate_event() {
	event *curr;

	if ((curr = getnextevent()) == NULL) {
		stop_sim = TRUE;
	} else {
		simtime = curr->time;
		/*
		 fprintf (outputfile, "%f: next event to handle: type %d, temp %x\n", simtime, curr->type, curr->temp);
		 fflush (outputfile);
		 */

		if (curr->type == INTR_EVENT) {
			intr_acknowledge(curr);
		} else if ((curr->type >= IO_MIN_EVENT) && (curr->type <= IO_MAX_EVENT)) {
			io_internal_event(curr);
		} else if ((curr->type >= PF_MIN_EVENT) && (curr->type <= PF_MAX_EVENT)) {
			pf_internal_event(curr);
		} else if (curr->type == TIMER_EXPIRED) {
			((timer_event *) curr)->func(curr);
		} else {
			fprintf(stderr, "Unrecognized event in simulate: %d\n", curr->type);
			exit(0);
		}
		/*
		 fprintf (outputfile, "Event handled, going for next\n");
		 fflush (outputfile);
		 */
	}
}

void disksim_exec(char *parfilePath, char *outfilePath, char *tracetypeStr, char *tracefilePath,
		char *synthgenStr) {
	StaticAssert (sizeof(intchar) == 4);
	if((debugFile = fopen("debugfile", "w")) == NULL) {
		fprintf(stderr, "can not open debug file");
		exit(0);
	}
	if ((parfile = fopen(parfilePath,"r")) == NULL) {
		fprintf(stderr,"%s cannot be opened for read access\n", parfilePath);
		exit(0);
	}
	if (strcmp(outfilePath, "stdout") == 0) {
		outputfile = stdout;
	} else {
		if ((outputfile = fopen(outfilePath,"w")) == NULL) {
			fprintf(stderr, "Outfile %s cannot be opened for write access\n", outfilePath);
			exit(0);
		}
	}
	fprintf (outputfile, "\nOutput file name: %s\n", outfilePath);
	fflush (outputfile);

	if (strcmp (tracetypeStr, "external") == 0) {
		external_control = 1;
	} else {
		iotrace_set_format(tracetypeStr);
	}
	fprintf (outputfile, "Input trace format: %s\n", tracetypeStr);
	fflush (outputfile);

	if (strcmp(tracefilePath, "0") != 0) {
		assert (external_control == 0);
		iotrace = 1;
		if (strcmp(tracefilePath, "stdin") == 0) {
			iotracefile = stdin;
		} else {
			if ((iotracefile = fopen(tracefilePath,"r")) == NULL) {
				fprintf(stderr, "Tracefile %s cannot be opened for read access\n", tracefilePath);
				exit(0);
			}
		}
	}
	fprintf (outputfile, "I/O trace used: %s\n", tracefilePath);
	fflush (outputfile);

	if (strcmp(synthgenStr, "0") != 0) {
		synthgen = 1;
	}
	fprintf (outputfile, "Synthgen to be used?: %s\n\n", synthgenStr);
	fflush (outputfile);

	readparams(parfile);
	fprintf(outputfile, "Readparams complete\n");
	fflush(outputfile);
	initialize();
	fprintf(outputfile, "Initialization complete\n");
	fflush(outputfile);
	prime_simulation();
	if (external_control) {
		return;
	}

	while (stop_sim == FALSE) {
		disksim_simulate_event();
	}
	fprintf(outputfile, "Simulation complete\n");
	fflush(outputfile);
	cleanstats();
	printstats();

	if (parfile) {
		fclose (parfile);
	}
	if (outputfile) {
		fclose (outputfile);
	}
	if (iotracefile) {
		fclose(iotracefile);
	}
	if(debugFile){
		fclose(debugFile);
	}
}
