/*
 * disksim_rms.c
 *
 *  Created on: Jul 5, 2014
 *      Author: WangZhi
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "disksim_rms.h"

#define RMS_INCR 0.0001

#define MAX_POINTS	10000

double line1_x[MAX_POINTS];
double line1_y[MAX_POINTS];
double line2_x[MAX_POINTS];
double line2_y[MAX_POINTS];

void get_line(filename, x, y, distname)
	char *filename;double *x;double *y;char *distname; {
	FILE *infile;
	char line[201];
	double junk;
	int i = 0;

	if (strcmp(filename, "stdin") == 0) {
		infile = stdin;
	} else if ((infile = fopen(filename, "r")) == NULL) {
		fprintf(stderr, "Can't open infile %s\n", filename);
		exit(0);
	}
	while ((fgets(line, 200, infile)) && (strcmp(line, distname))) {
	}
	while (fgets(line, 200, infile)) {
		if (sscanf(line, "%lf %lf %lf %lf", &x[i], &junk, &junk, &y[i]) != 4) {
			fprintf(stderr, "Invalid line in distribution: %s\n", line);
			exit(0);
		}
		if (y[i] >= 1.0) {
			return;
		}
		if (i >= MAX_POINTS) {
			fprintf(stderr, "Too many lines in distribution: %d\n", i);
			exit(0);
		}
		i++;
	}
}

double compute_rms(x1, y1, x2, y2)
	double *x1;double *y1;double *x2;double *y2; {
	int count = 0;
	double runval = 0.0;
	double runsquares = 0.0;
	int i1 = 0;
	int i2 = 0;
	double pnt = RMS_INCR;
	double yfrac;
	double x1pnt, x2pnt;

	for (; pnt <= 1.0; pnt += RMS_INCR) {
		while (y1[i1] < pnt) {
			i1++;
		}
		while (y2[i2] < pnt) {
			i2++;
		}
		yfrac = (pnt - y1[(i1 - 1)]) / (y1[i1] - y1[(i1 - 1)]);
		x1pnt = x1[(i1 - 1)] + (yfrac * (x1[i1] - x1[(i1 - 1)]));
		yfrac = (pnt - y2[(i2 - 1)]) / (y2[i2] - y2[(i2 - 1)]);
		x2pnt = x2[(i2 - 1)] + (yfrac * (x2[i2] - x2[(i2 - 1)]));
		runval += fabs(x1pnt - x2pnt);
		runsquares += (x1pnt - x2pnt) * (x1pnt - x2pnt);
		count++;
	}
	return (sqrt(runsquares / (double) count));
}

void disksim_rms(char *filePath1, char *filePath2, char *disknoStr,
		char *rootnamesStr, char *doforservsStr) {
	int diskno;
	char distname[201];
	double rms;
	int rootnames;
	int doforservs;
	if (sscanf(disknoStr, "%d", &diskno) != 1) {
		fprintf(stderr, "Invalid disk number: %s\n", disknoStr);
		exit(0);
	}
	if (sscanf(doforservsStr, "%d", &doforservs) != 1) {
		fprintf(stderr, "Invalid value for doforservs: %s\n", disknoStr);
		exit(0);
	}
	if (doforservs) {
		if (diskno == -1) {
			sprintf(distname, "IOdriver Physical access time distribution\n");
		} else {
			sprintf(
					distname,
					"IOdriver #0 device #%d Physical access time distribution\n",
					diskno);
		}
	} else {
		if (diskno == -1) {
			sprintf(distname, "IOdriver Response time distribution\n");
		} else {
			sprintf(distname,
					"IOdriver #0 device #%d Response time distribution\n",
					diskno);
		}
	}
	get_line(filePath1, line1_x, line1_y, distname);
	if (sscanf(rootnamesStr, "%d", &rootnames) != 1) {
		fprintf(stderr, "Invalid number of rootnames: %s\n", rootnamesStr);
		exit(0);
	}
	if (rootnames == -1) {
		sprintf(distname, "VALIDATE Trace access time distribution\n");
		rootnames = 0;
	}
	if (rootnames == 0) {
		get_line(filePath2, line2_x, line2_y, distname);
		rms = compute_rms(line1_x, line1_y, line2_x, line2_y);
		printf("rms = %f\n", rms);
	} else {
		double runrms = 0.0;
		double runsquares = 0.0;
		char filename[51];
		int i;

		for (i = 1; i <= rootnames; i++) {
			sprintf(filename, "%s.%d", filePath2, i);
			get_line(filename, line2_x, line2_y, distname);
			rms = compute_rms(line1_x, line1_y, line2_x, line2_y);
			runrms += rms;
			runsquares += rms * rms;
		}
		runrms /= (double) rootnames;
		runsquares = (runsquares / (double) rootnames) - (runrms * runrms);
		runsquares = (runsquares > 0.0) ? sqrt(runsquares) : 0.0;
		printf("rms = %f (%f)\n", runrms, runsquares);
	}
}

