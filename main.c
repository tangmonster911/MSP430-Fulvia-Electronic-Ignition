#include <msp430g2553.h>

/*
 * main.c
 */
 /*
 
 18 Jul 2019
 
 
This code has two basic options for ignition depending if IgnitionAdvance = 1 or 0
 This code keeps dwell at 4ms under 4500 rpm , above 4500 rpm it will be 3ms
 
 For IgnitionAdvance = 0:

 Ignition advance is disbaled , distributor weights handles advance.
 Points closing or hall effect trigger will case see 
 delay then 4ms dwell or 2ms depending on RPM
 then ignition at 72 degrees after trigger.
 
 
 for ignition advance enabled for 0 advance spark happens at 80 degrees after trigger
 static advance has to be set as normal.
 Fulvia uses two advance curves standard
 0 rpm to 999rpm uses 0 advance
 1000 rpm to 1800 rpm uses 0 to 10 degree engine advance
 1801 rpm to 3250 rpm uses 10 to 15 degree engine advance
 3251 rpm to xxxx rpm uses 15 degree engine advance
 NOTE!!!!:::  In this mode distributor weights must be locked out.
 
 Points closing or hall effect trigger will case see 
 delay depending on RPM and advance curve
 then 4ms dwell or 2ms depending on RPM
 then ignition at 72 degrees after trigger for 0 advance
 or earlier depending on advance.
 


 
 */
 unsigned int TimerCnt,IgnitionCount;
 unsigned int DwellCount,DefaultDwellCount;
 unsigned int DwellStartCount;
 unsigned int SwitchDebounce;
 unsigned int TriggerCount;
 unsigned int RevLimit,RevLimitIntCount;
 unsigned int IgnitionAdvance;
 unsigned int curve1Start,curve1StartRPM;
 unsigned int curve1End,curve1EndRPM;
 unsigned int curve2End,curve2EndRPM;
 unsigned int curve1Divider,AdvanceDeg1;
 unsigned int curve2Divider,AdvanceDeg2;
 unsigned int MaxDivider;
 unsigned int doMath = 0;
 float AdvanceTicks,x1,x2,x3;

int main(void) {
    /* RevLimit set the Engine RPMlimit by changing this variable where coil will not be energised. This value 10us counts for 90 degrees. so 
       this count is the amount for ENGINE RPM
       545 = 5500rpm and 500 = 6000rpm and 492 = +- 6100 and 483 = +- 6200 and 476 = 6300 and 461 = 6500 rpm 
    */
    RevLimit = 6500;
    RevLimitIntCount = 3000000/(RevLimit); // above Revlimit calculated to 10uS counts.
    /*
       should we do automatic ignition advance?
    */ 
    IgnitionAdvance = 1;

    /*
        firstand second  ignition cure start and end rpm and the devider to figure out atvance timer
    */
    
    curve1StartRPM = 1000;   //start first ignition advance curve at this engine RPM
    curve1EndRPM = 1800;     //end first igntiion advance curve at this engine RPM 
    AdvanceDeg1 = 9;         //engine degrees to be advanced inside this 1st curve

    curve2EndRPM = 3200;     //second Curve ends at this RPM
    AdvanceDeg2 = 6;         //engine degrees to be advanced inside this 2nd curve
    
    curve1Start = 3000000/curve1StartRPM;     //calculate 10us timer ticks per 90 degree distributor at curve1StartRPM
    curve1End = 3000000/curve1EndRPM;         //calculate 10us timer ticks per 90 degree distributor at curve1EndRPM
    curve2End = 3000000/curve2EndRPM;         //calculate 10us timer ticks per 90 degree distributor at curve2EndRPM 
  
    DefaultDwellcount = 500;    //set default dwell Count in 10us  500 = 5ms


    /*
     * main Code
     */


    DwellCount = DefaultDwellCount;
    TriggerCount = 0;
    SwitchDebounce = 100;
    TimerCnt = 0;
    DwellStartCount = 65535;
    WDTCTL = WDTPW + WDTHOLD;	                   // Stop watchdog timer
    /* Use Factory stored presets to calibrate the internal oscillator */
    DCOCTL = CALDCO_16MHZ;
    BCSCTL1 = CALBC1_16MHZ;

    TA0CTL = TACLR;                              // Reset Timer ,halted
    TA0CCTL0 = CCIE;                             // Timer A interrupts enabled
    //TA0CTL = TASSEL_2 + MC_0 + ID_3;           // SMCLK/8, timer halted , count up
    TA0CTL = TASSEL_2 + MC_1 + ID_3;             // SMCLK/8, started , count up
    TA0CCR0 = 20-1;                              // Set maximum count (Interrupt frequency 16MHz/8/20 = 100000Hz or 10us per interrupt)

    P1OUT &= 0x00;                               // Shut down everything
    P1DIR &= 0x00;               
    P1DIR |= BIT0 + BIT6;                        // P1.0 and P1.6 pins output the rest are input 
    P1REN |= BIT3;                               // Enable internal pull-up/down resistors
    P1OUT |= BIT3;                               //Select pull-up mode for P1.3
    P1IE |= BIT3;                                // P1.3 interrupt enabled
    P1IES |= BIT3;                               // P1.3 Hi/lo edge
    P1IFG &= ~BIT3;                              // P1.3 IFG cleared
    _BIS_SR(GIE);                                // global interrupt enable

    while(1)                                     //Loop forever, we work with interrupts!
    {
       if (doMath == 1)
       {
           if (IgnitionCount < 625)                     // check if motor rpm more than 4800rpm by checking how many 10uS counts since last trigger   (625 = 1000*(90*(1/((4800/2)*(360/60))))
           {
               DwellCount = 6*(IgnitionCount/10);      // make dwell 54 degrees
           }
           if (IgnitionAdvance == 0)                  //NO advance so standard delay and dwell and trigger at 72 degrees
           {

                DwellStartCount = (8*(IgnitionCount/10))-DwellCount;   //dwell must be 4ms and spark at 8/10 into 90 degree distributor (at 72 degrees)
           }
           if (IgnitionAdvance == 1)                  // Ignition advance must be done so math for that:
           {
                if (IgnitionCount >= curve1Start )                          //rpm lower than advance curve1
                {
                    DwellStartCount = (8*(IgnitionCount/10))-DwellCount;
                }
                if ((IgnitionCount < curve1Start) && (IgnitionCount >= curve1End)) //rpm higher than advance start1
                {
                    x1 = (3000000/IgnitionCount)-curve1StartRPM;         //calculate RPM difference higher than first curve rpm start
                    x2 = (curve1EndRPM-curve1StartRPM)/AdvanceDeg1; //rpm difference at which you need to advance 1 degree
                    x3 = x1 / x2;                                   //what fraction of a degree do we need to advance
                    AdvanceTicks = x3 * (IgnitionCount/180);             //how many 10us timerticks do we need to advance for the current engine RPM (distributor is half speed to engine)
                    //AdvanceTicks = (((3000000/TimerCnt)-curve1StartRPM)/((curve1EndRPM-curve1StartRPM)/AdvanceDeg1)) * (TimerCnt/180);
                    DwellStartCount = (8*(IgnitionCount/10))- DwellCount - AdvanceTicks; // start dwell taking into account dwell and advance.
                }
                if ((IgnitionCount < curve1End) && (IgnitionCount >= curve2End)) //rpm higher than advance start2
                {
                    x1 = (3000000/IgnitionCount)-curve1EndRPM;           //calculate RPM difference higher than second curve rpm start
                    x2 = (curve2EndRPM-curve1EndRPM)/AdvanceDeg2;   //rpm difference at which you need to advance 1 degree
                    x3 = (x1 / x2) + AdvanceDeg1;                   //what fraction of a degree do we need to advance ( add total advance of first curve)
                    AdvanceTicks = x3 * (IgnitionCount/180);             //how many 10us timerticks do we need to advance for the current engine RPM (distributor is half speed to engine)
                    //AdvanceTicks = (((3000000/TimerCnt)-(curve1EndRPM))/((curve2EndRPM-curve1EndRPM)/(AdvanceDeg2)) * (TimerCnt/180);
                    DwellStartCount = (8*(IgnitionCount/10))- DwellCount - AdvanceTicks;     // start dwell taking into account dwell and advance.
                }
                if ((IgnitionCount < curve2End) && (IgnitionCount >= RevLimitIntCount)) //rpm higher than advance Curve2End
                {
                    //stay at max advance
                    x3 = AdvanceDeg2 + AdvanceDeg1;                   //add up total advance from first two curves
                    AdvanceTicks = x3 * (IgnitionCount/180);               //how many 10us timerticks do we need to advance for the current engine RPM (distributor is half speed to engine)
                    DwellStartCount = (8*(IgnitionCount/10))- DwellCount - AdvanceTicks;
                }
                if (IgnitionCount < RevLimitIntCount) DwellStartCount = 65535;   // above rev limit so no spark
            }
       doMath = 0;
       }
    }
}

#pragma vector=TIMER0_A0_VECTOR 
__interrupt void Timer0 (void) 
{  
    //P1OUT |= BIT6;                             // test
    if (TimerCnt < 30000) TimerCnt++;                                  //increase time count by 10us
    if (TimerCnt >= DwellStartCount)             //should we energize coil yet?
    {
        if  (DwellCount > 0)                    //Only Energize coil if DwellCount is larger then 0 (0 setting is for rev limit)
        {
            P1OUT |= BIT6;                        // energize to coil with pin high
            DwellCount--;
        } else                                   //if end of dwell time
        {
            P1OUT &= ~BIT6;                      // coil off , SPARK
        }        
    }

    if (SwitchDebounce > 0)                      // check if we are in debounce time
    {
        if ((P1IN & BIT3) == BIT3)                // are points open?
        {
            SwitchDebounce--;
        } else
        {
            SwitchDebounce = 100;                //switch still closed so reset bounce to 1ms
        }
        if (SwitchDebounce == 0)                 // debounce time has elapsed and points is open
        {
            P1IE |= BIT3;                        // P1.3 interrupt enabled check for next points trigger
        }
    }

    // P1OUT &= ~BIT6;                        // test
} 
  
  
  // Port 1 interrupt service routine
#pragma vector=PORT1_VECTOR
__interrupt void Port_1 (void)
{    
   TA0CTL = TACLR;                              // Reset Timer ,halted
   TA0CTL = TASSEL_2 + MC_1 + ID_3;             // SMCLK/8, started , count up
   P1IE &= ~BIT3;                               // P1.3 P1.3 interrupt disabled for switchbounce
    //P1OUT |= BIT6;                             // test

   if (TimerCnt == 30000)                        //if engine RPM below 100rpm then don't trigger spark
   {
       TriggerCount = 0;                          //reset trigger count to 0 for start.
       SwitchDebounce = 100;                      //set 1ms debounce timer (open for 100 x 10us checks consecutively)
       TimerCnt = 0;                              //reset timer count to 0
       P1OUT |= BIT0;                             // error light up led
       DwellStartCount = 65535;                   //reset Dwellstartcount after ignition trigger interrupt
   } else                                         //we are above 100rpm
   {
       TriggerCount++;
       //SwitchDebounce = TimerCnt >> 3;          // switch Debounce must be 12% of 90 degree time.
       SwitchDebounce = 100;                      //set 1 ms debounce timer (open for 100 x 10us checks consecutively)
       DwellCount = DefaultDwellCount;            //set up dwell count
       DwellStartCount = 65535;                   //reset Dwellstartcount after ignition trigger interrupt
       IgnitionCount = TimerCnt;
       TimerCnt = 0;                              // reset timer count to zero
   }

   if ( TriggerCount > 2)                    // three triggers has been found we have rpm
   {
       doMath = 1;
       TriggerCount = 3;                          // run this one again next time
   }  

   P1IFG &= ~BIT3;                            // P1.3 IFG cleared 
  // P1OUT &= ~BIT6;                        // test
}
