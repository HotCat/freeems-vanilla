/* FreeEMS - the open source engine management system
 *
 * Copyright 2008, 2009, 2010, 2011 Fred Cooke
 *
 * This file is part of the FreeEMS project.
 *
 * FreeEMS software is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * FreeEMS software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with any FreeEMS software.  If not, see http://www.gnu.org/licenses/
 *
 * We ask that if you make any changes to this file you email them upstream to
 * us at admin(at)diyefi(dot)org or, even better, fork the code on github.com!
 *
 * Thank you for choosing FreeEMS to run your engine!
 */


/**	@file Simple.c
 * @ingroup interruptHandlers
 * @ingroup enginePositionRPMDecoders
 *
 * @brief Reads any signal that is once per cylinder
 *
 * This file contains the two interrupt service routines required for to build
 * cleanly. However, only the first one is used due to the simple nature of it.
 *
 * The functional ISR just blindly injects fuel for every input it receives.
 * Thus a perfectly clean input is absolutely essential at this time.
 *
 * Supported engines include:
 * B230F
 *
 * @author Fred Cooke
 *
 * @note Even though I ran my US road trip car on this exact code, I don't recommend it unless you REALLY know what you are doing!
 */


#define DECODER_IMPLEMENTATION_C
#define DECODER_MAX_CODE_TIME    100 // To be optimised (shortened)!
#define NUMBER_OF_REAL_EVENTS     1
#define NUMBER_OF_VIRTUAL_EVENTS  4

#include "../inc/freeEMS.h"
#include "../inc/interrupts.h"
#include "../inc/decoderInterface.h"
#include "../inc/utils.h"

void decoderInitPreliminary(){} // This decoder works with the defaults
void perDecoderReset(){} // Nothing special to reset for this code

const unsigned char decoderName[] = "Simple";
const unsigned short eventAngles[] = {0,180,360,540};
const unsigned char eventValidForCrankSync[] = {0,0,0,0};


/* Blindly start fuel pulses for each and every input pulse.
 *
 * Warning This is for testing and demonstration only, not suitable for driving with just yet.
 */
void PrimaryRPMISR(){
	// TODO make this code more general and robust such that it can be used for real simple applications

	/* Clear the interrupt flag for this input compare channel */
	TFLG = 0x01;

	/* Save all relevant available data here */
	unsigned short codeStartTimeStamp = TCNT;		/* Save the current timer count */
	unsigned short edgeTimeStamp = TC0;				/* Save the edge time stamp */
	unsigned char PTITCurrentState = PTIT;			/* Save the values on port T regardless of the state of DDRT */

	// set as synced for volvo always as loss of sync not actually possible
	decoderFlags |= COMBUSTION_SYNC;

	/* Calculate the latency in ticks */
	ISRLatencyVars.primaryInputLatency = codeStartTimeStamp - edgeTimeStamp;

	if(PTITCurrentState & 0x01){
		/* Echo input condition on J7 */
		PORTJ |= 0x80;

		Counters.primaryTeethSeen++;

		LongTime timeStamp;

		/* Install the low word */
		timeStamp.timeShorts[1] = edgeTimeStamp;
		/* Find out what our timer value means and put it in the high word */
		if(TFLGOF && !(edgeTimeStamp & 0x8000)){ /* see 10.3.5 paragraph 4 of 68hc11 ref manual for details */
			timeStamp.timeShorts[0] = timerExtensionClock + 1;
		}else{
			timeStamp.timeShorts[0] = timerExtensionClock;
		}

		// temporary data from inputs
		unsigned long primaryLeadingEdgeTimeStamp = timeStamp.timeLong;
		unsigned long timeBetweenSuccessivePrimaryPulses = primaryLeadingEdgeTimeStamp - lastEventTimeStamp;
		lastEventTimeStamp = primaryLeadingEdgeTimeStamp;


		*ticksPerDegreeRecord = (unsigned short)(timeBetweenSuccessivePrimaryPulses / 4);

		// TODO sample ADCs on teeth other than that used by the scheduler in order to minimise peak run time and get clean signals
		sampleEachADC(ADCArrays);
		Counters.syncedADCreadings++;
		*mathSampleTimeStampRecord = TCNT;

		/* Set flag to say calc required */
		coreStatusA |= CALC_FUEL_IGN;

		/* Reset the clock for reading timeout */
		Clocks.timeoutADCreadingClock = 0;

		// TODO behave differently depending upon sync level? Genericise this loop/logic?
//		if(decoderFlags & COMBUSTION_SYNC){
//			unsigned char pin;
//			for(pin=0;pin<6;pin++){
//				if(pinEventNumbers[pin] == 0){
//					schedulePortTPin(pin, timeStamp);
//				}
//			}
//		} todo in mitsi decoder

		RuntimeVars.primaryInputLeadingRuntime = TCNT - codeStartTimeStamp;
	}else{
		PORTJ &= 0x7F;
		RuntimeVars.primaryInputTrailingRuntime = TCNT - codeStartTimeStamp;
	}
}


void SecondaryRPMISR(){
	TFLG = 0x02;
}
