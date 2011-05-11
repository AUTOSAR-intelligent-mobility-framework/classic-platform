/*
* Configuration of module: Com (Com_PbCfg.h)
*
* Created by:              
* Copyright:               
*
* Configured for (MCU):    MPC551x
*
* Module vendor:           ArcCore
* Generator version:       2.0.5
*
* Generated by Arctic Studio (http://arccore.com) 
*           on Tue May 10 13:57:12 CEST 2011
*/


#if !(((COM_SW_MAJOR_VERSION == 1) && (COM_SW_MINOR_VERSION == 1)) )
#error Com: Configuration file expected BSW module version to be 1.1.*
#endif

#ifndef COM_PBCFG_H
#define COM_PBCFG_H

#include "Com_Types.h"

extern const Com_ConfigType ComConfiguration;

//  COM Polite Defines.
#define COM_PDU_ID_RxPdu1		0
#define COM_PDU_ID_RxPdu2		1
#define COM_PDU_ID_TxPdu1		2
#define COM_PDU_ID_TxPdu2		3



// PDU group definitions
#define ComPduGroup 0


// Signal definitions
#define RxPdu1_Signal1 0
#define RxPdu2_Signal1 1
#define TxPdu1_Signal1 2
#define TxPdu2_Signal1 3
#define TxPdu2_SignalGroup1 4


// Group signal definitions
#define TxPdu2_SignalGroup1_GroupSignal1 0


#endif /* COM_PBCFG_H */