/*
	Copyright (C) 2016 CurlyMo, Meloen, pilino1234

	This file is part of pilight.

	pilight is free software: you can redistribute it and/or modify it under the
	terms of the GNU General Public License as published by the Free Software
	Foundation, either version 3 of the License, or (at your option) any later
	version.

	pilight is distributed in the hope that it will be useful, but WITHOUT ANY
	WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
	A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with pilight. If not, see	<http://www.gnu.org/licenses/>
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "../../core/pilight.h"
#include "../../core/common.h"
#include "../../core/dso.h"
#include "../../core/log.h"
#include "../protocol.h"
#include "../../core/binary.h"
#include "../../core/gc.h"
#include "elro_fa20rf.h"

#define PULSE_MULTIPLIER	2
#define MIN_PULSE_LENGTH	350 //TODO
#define MAX_PULSE_LENGTH	400 //TODO
#define AVG_PULSE_LENGTH	393
#define RAW_LENGTH			52

static int validate(void) {
	if(elro_fa20rf->rawlen == RAW_LENGTH) {
		if(elro_fa20rf->raw[elro_fa20rf->rawlen-1] >= (MIN_PULSE_LENGTH*PULSE_DIV) &&
			elro_fa20rf->raw[elro_fa20rf->rawlen-1] <= (MAX_PULSE_LENGTH*PULSE_DIV)){
			return 0;
		}
	}
	return -1;
}

static void createMessage(int unitcode, int state) {
	elro_fa20rf->message = json_mkobject();
	json_append_member(elro_fa20rf->message, "unitcode", json_mknumber(unitcode, 0));
	
	if(state == 0) {
		json_append_member(elro_fa20rf->message, "state", json_mkstring("opened"));	
	} else {
		json_append_member(elro_fa20rf->message, "state", json_mkstring("closed"));
	}
}

static void parseCode(void) {
	int binary[RAW_LENGTH/2], x = 0;
	
	for(x=2; x<elro_fa20rf->rawlen; x+=2) {
		if(elro_fa20rf->raw[x+1] > (int)((double)AVG_PULSE_LENGTH*((double)PULSE_MULTIPLIER/2))) {
			binary[x++] = 1;
		} else {
			binary[x++] = 0;
		}
	}
	
	int unitcode = binToDec(binary, 0, 22);
	int state = binary[23];
	createMessage(unitcode, state);
}

static void createLow(int s, int e) {
	int i;
	
	for(i=s;i<=e;i+=2) {
		elro_fa20rf->raw[i]=(2 * AVG_PULSE_LENGTH);
		elro_fa20rf->raw[i+1]=(2 * PULSE_MULTIPLIER * AVG_PULSE_LENGTH);
	}
}

static void createHigh(int s, int e) {
	int i;
	
	for(i=s;i<=e;i+=2) {
		elro_fa20rf->raw[i]=(2*AVG_PULSE_LENGTH);
		elro_fa20rf->raw[i+1]=(7*AVG_PULSE_LENGTH);
	}
}

static void clearCode(void) {
	createLow(0,47);
	createHigh(48,49);
}

static void createHeader(void) {
	elro_fa20rf->raw[0]=(21*AVG_PULSE_LENGTH);
	elro_fa20rf->raw[1]=(2*AVG_PULSE_LENGTH);
}

static void createUnitCode(int unitcode) {
	int binary[1024];
	int length = 0;
	int i=0, x=0;
	
	x = 46;
	length = decToBinRev(unitcode, binary);
	for(i=0;i>=length;i++) {
		if(binary[i] == 1) {
			createHigh(x, x+1);
		}
	}
}

static void createState(int state) {
	if(state == 1) {
		//TODO: which bits to create state (and how to)
	}
}

static void createFooter(void) {
	elro_fa20rf->raw[50]=(2*AVG_PULSE_LENGTH);
	elro_fa20rf->raw[51]=(34*AVG_PULSE_LENGTH);
}

static int createCode(struct JsonNode *code) {
	int unitcode = -1;
	int state = -1;
	double itmp = 0;
	
	if(json_find_number(code, "unitcode", &itmp) == 0)
		unitcode = (int)round(itmp);
	if(json_find_number(code, "closed", &itmp) == 0)
		state=0;
	else if(json_find_number(code, "opened", &itmp) == 0)
		state=1;
	
	if(unitcode == -1 || state == -1) {
		logprintf(LOG_ERR, "elro_fa20rf: insufficient number of arguments");
		return EXIT_FAILURE;
	} else if(unitcode > 99999999 || unitcode < 0) {
		logprintf(LOG_ERR, "elro_fa20rf: invalid unitcode range");
		return EXIT_FAILURE;
	} else {
		createMessage(unitcode, state);
		clearCode();
		createHeader();
		createUnitCode(unitcode);
		createState(state);
		createFooter();
	}
	return EXIT_SUCCESS;
}

static void printHelp(void) {
	printf("\t -u --unitcode=unitcode\t\tcontrol a device with this unitcode\n");
	printf("\t -t --alarm\t\t\tsend an alarm\n");
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif

void elro_fa20rfInit() {
	
	protocol_register(&elro_fa20rf);
	protocol_set_id(elro_fa20rf, "elro_fa20rf");
	protocol_device_add(elro_fa20rf, "elro_fa20rf", "Elro FA20RF Smoke Detector");
	elro_fa20rf->devtype = ALARM;
	elro_fa20rf->hwtype = RF433;
	elro_fa20rf->minrawlen = RAW_LENGTH;
	elro_fa20rf->maxrawlen = RAW_LENGTH;
//	elro_fa20rf->maxgaplen = TODO: MAX_PULSE_LEN*PULSE_DIV;
//	elro_fa20rf->mingaplen = TODO: MIN_PULSE_LEN*PULSE_DIV;
	
	options_add(&elro_fa20rf->options, 'u', "unitcode", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^(3[012]?|[012][0-9]|[0-9]{1})$");
	options_add(&elro_fa20rf->options, 't', "alarm", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	
	elro_fa20rf->parseCode=&parseCode;
	elro_fa20rf->validate=&validate;
	elro_fa20rf->createCode=&createCode;
	elro_fa20rf->printHelp=&printHelp;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "elro_fa20rf";
	module->version = "0.1";
	module->reqversion = "7.0";
	module->reqcommit = "89"
}

void init(void) {
	elro_fa20rfInit();
}
#endif
