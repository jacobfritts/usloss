#include <stdio.h>
#include <phase1.h>
#include <phase2.h>
#include <usloss.h>
#include "message.h"

extern int debugflag2;
#define CLOCKBOX 0
#define DISKBOX 1
#define TERMBOX 3

extern void disableInterrupts(void);
extern void enableInterrupts(void);
extern void requireKernelMode(char *);

int IOmailboxes[7]; // mboxIDs for the IO devices
int IOblocked = 0; // number of processes blocked on IO mailboxes

int clockInterruptCount = 0;


/* an error method to handle invalid syscalls */
void nullsys(USLOSS_Sysargs *args)
{
    USLOSS_Console("nullsys(): Invalid syscall. Halting...\n");
    USLOSS_Halt(1);
} /* nullsys */


/* ------------------------------------------------------------------------
   Name - clockHandler2
   Purpose - 1) Check that device is clock
			 2)	Call timeSlice function when necessary
			 3) Conditionally send to the clock i/o mailbox every 5th clock 
			 interrupt.
   Parameters - int dev, void *arg (Ignore arg; it is not used by the clock)
   Returns - nothing
   Side Effects - Calls dispatcher() if necessary
   ----------------------------------------------------------------------- */
void clockHandler2(int dev, void *arg)
{
    if (DEBUG2 && debugflag2)
        USLOSS_Console("clockHandler2(): called\n");

	int unit = 0;
	int status;
	int dev_rtn;
	int msg_rtn;
	
	dev_rtn = USLOSS_DeviceInput(USLOSS_CLOCK_DEV, unit, &status);

	if(dev == USLOSS_CLOCK_DEV)
	{
		if(dev_rtn == USLOSS_DEV_OK) 
		{
			clockInterruptCount ++;
			if(clockInterruptCount > 4)
			{
				clockInterruptCount = 0;
			
				timeSlice(); 
			
				msg_rtn = MboxCondSend(IOmailboxes[CLOCKBOX], &status, sizeof(int));
			}
		}
		else
		{
			USLOSS_Console("diskHandler(): device input error %d\n", dev);
			USLOSS_Halt(1);
		}
	}
	else
	{
		USLOSS_Console("clockHandler(): device type error %d\n", dev_rtn);
		USLOSS_Halt(1);
	}
	if (DEBUG2 && debugflag2)
        USLOSS_Console("clockHandler(): msg_rtn %d\n", msg_rtn);
	
} /* clockHandler2 */


/* ------------------------------------------------------------------------
   Name - diskHandler
   Purpose - 1) Check that device is disk
			 2) Check that unit number is in the correct range
			 3)	Read disk status register using USLOSS_Device_Input
			 4) Conditionally send status to the appropriate i/o mailbox 
			 5) Conditional send so disk is never blocked on the mailbox
   Parameters - int dev, void *arg
   Returns - nothing
   Side Effects - none
   ----------------------------------------------------------------------- */
void diskHandler(int dev, void *arg)
{
    if (DEBUG2 && debugflag2)
        USLOSS_Console("diskHandler(): called\n");

	int unit = (long int) arg;
	int status;
	int dev_rtn;
	int msg_rtn;

	dev_rtn = USLOSS_DeviceInput(dev, unit, &status);

    if (DEBUG2 && debugflag2)
        USLOSS_Console("diskHandler(): dev %d, unit %d, status %d, dev_rtn %d\n", dev, unit, status, dev_rtn);

	if(dev == USLOSS_DISK_DEV)
	{
		if(dev_rtn == USLOSS_DEV_OK) 
		{
			if(unit == 0)
			{
				msg_rtn = MboxCondSend(IOmailboxes[DISKBOX], &status, sizeof(int));
			}
			else if(unit == 1) 
			{
				msg_rtn = MboxCondSend(IOmailboxes[DISKBOX+1], &status, sizeof(int));
			}
			else
			{
				USLOSS_Console("diskHandler(): device unit number error %d\n", unit);
				USLOSS_Halt(1);
			}
		}
		else
		{
			USLOSS_Console("diskHandler(): device input error %d\n", dev_rtn);
			USLOSS_Halt(1);
		}
	}
	else
	{
	}
	if (DEBUG2 && debugflag2)
        USLOSS_Console("diskHandler(): msg_rtn %d\n", msg_rtn);
	
} /* diskHandler */


/* ------------------------------------------------------------------------
   Name - termHandler
   Purpose - 1) Check that device is terminal
			 2) Check that unit number is in the correct range
			 3)	Read terminal status register using USLOSS_Device_Input
			 4) Conditionally send status to the appropriate i/o mailbox 
			 5) Conditional send so disk is never blocked on the mailbox
   Parameters - int dev, void *arg
   Returns - nothing
   Side Effects - none
   ----------------------------------------------------------------------- */
void termHandler(int dev, void *arg)
{
    if (DEBUG2 && debugflag2)
        USLOSS_Console("termHandler(): called\n");

	int unit = (long int) arg;
	int status;
	int dev_rtn;
	int msg_rtn;
	
	dev_rtn = USLOSS_DeviceInput(dev, unit, &status);

    if (DEBUG2 && debugflag2)
        USLOSS_Console("termHandler(): dev %d, unit %d, status %d, dev_rtn %d\n", dev, unit, status, dev_rtn);

	
	if(dev == USLOSS_TERM_DEV)
	{
		if(dev_rtn == USLOSS_DEV_OK) 
		{
			if(unit == 0)
			{
				msg_rtn = MboxCondSend(IOmailboxes[TERMBOX], &status, sizeof(int));
			}
			else if(unit == 1) 
			{
				msg_rtn = MboxCondSend(IOmailboxes[TERMBOX+1], &status, sizeof(int));				
			}
			else if(unit == 2) 
			{
				msg_rtn = MboxCondSend(IOmailboxes[TERMBOX+2], &status, sizeof(int));
			}
			else if(unit == 3) 
			{
				msg_rtn = MboxCondSend(IOmailboxes[TERMBOX+3], &status, sizeof(int));
			}
			else
			{
				USLOSS_Console("termHandler(): device unit number error %d\n", unit);
				USLOSS_Halt(1);
			}
		}
		else
		{
			USLOSS_Console("termHandler(): device input error %d\n", dev_rtn);
			USLOSS_Halt(1);
		}
	}
	else
	{
		USLOSS_Console("termHandler(): device type error %d\n", dev);
		USLOSS_Halt(1);
	}
    if (DEBUG2 && debugflag2)
        USLOSS_Console("termHandler(): msg_rtn %d\n", msg_rtn);
	
} /* termHandler */


/* ------------------------------------------------------------------------
   Name - syscallHandler
   Purpose - Call the dispatcher if the currently executing process has 
			 exceeded its time slice; otherwise return.
   Parameters - none
   Returns - nothing
   Side Effects - none
   ----------------------------------------------------------------------- */
void syscallHandler(int dev, void *arg)
{
    if (DEBUG2 && debugflag2)
        USLOSS_Console("syscallHandler(): called\n");

	int unit = (long int) arg;

    if (DEBUG2 && debugflag2)
        USLOSS_Console("syscallHandler(): dev %d, unit %d\n", dev, unit);

	if(dev == USLOSS_SYSCALL_INT)
	{
	}
	else
	{
		USLOSS_Console("syscallHandler(): device type error %d\n", dev);
		USLOSS_Halt(1);
	}


/*
	int index;

	USLOSS_Sysargs *saptr = (USLOSS_Sysargs *) arg;

	int oldPSR = USLOSS_PsrGet();
	
	index = saptr->number;
	
	if(syscalls_vec[index] == NULL)
		fprintf(stderr, "Underconstruction until a later phase!");
	else 
	{
		USLOSS_PsrSet(oldPSR | USLOSS_PSR_CURRENT_INT);
		(syscalls_vec[index])(saptr); //this is supposed to work because of vector according to mike
		USLOSS_PsrSet(oldPSR);

	}
*/

} /* syscallHandler */
