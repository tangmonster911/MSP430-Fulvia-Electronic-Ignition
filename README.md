# MSP430  Fulvia Electronic Ignition

The the calculation for timing advance has been tested to take roughly 250us. This happens inside main While loop. 

When low trigger interrupt from points input is received 
1) the 10us timer and count is reset and counted 
2) math is completed ( less than 250us) 
3) coil needs to be energised for no longer than default set ms Dwell 
4) coil is switched off and spark should happen

RPM is calculated from each 90 degree turn on distributor.

