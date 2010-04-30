/* -------------------------------- Arctic Core ------------------------------
 * Arctic Core - the open source AUTOSAR platform http://arccore.com
 *
 * Copyright (C) 2009  ArcCore AB <contact@arccore.com>
 *
 * This source code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation; See <http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 * -------------------------------- Arctic Core ------------------------------*/


#include "Std_Types.h"
#include "Mcu.h"
#include "Det.h"
#if defined(USE_DEM)
#include "Dem.h"
#endif
#include <assert.h>
#include "cpu.h"
#include <string.h>
#include "Ramlog.h"

//#define USE_TRACE 1
//#define USE_LDEBUG_PRINTF 1
#include "debug.h"


#ifndef ARRAY_SIZE
#define ARRAY_SIZE(_x)  (sizeof(_x)/sizeof((_x)[0]))
#endif

/* Development error macros. */
#if ( MCU_DEV_ERROR_DETECT == STD_ON )
#define VALIDATE(_exp,_api,_err ) \
        if( !(_exp) ) { \
          Det_ReportError(MODULE_ID_MCU,0,_api,_err); \
          return; \
        }

#define VALIDATE_W_RV(_exp,_api,_err,_rv ) \
        if( !(_exp) ) { \
          Det_ReportError(MODULE_ID_MCU,0,_api,_err); \
          return (_rv); \
        }
#else
#define VALIDATE(_exp,_api,_err )
#define VALIDATE_W_RV(_exp,_api,_err,_rv )
#endif


#define CORE_CPUID_CORTEX_M3   	0x411FC231UL



typedef struct {
	uint32 lossOfLockCnt;
	uint32 lossOfClockCnt;
} Mcu_Stats;

/**
 * Type that holds all global data for Mcu
 */
typedef struct
{
  // Set if Mcu_Init() have been called
  boolean initRun;

  // Our config
  const Mcu_ConfigType *config;

  Mcu_ClockType clockSetting;

  Mcu_Stats stats;

} Mcu_GlobalType;


// Global config
Mcu_GlobalType Mcu_Global =
{
		.initRun = 0,
		.config = &McuConfigData[0],
};

//-------------------------------------------------------------------

typedef struct {
  char *name;
  uint32 pvr;
} core_info_t;

typedef struct {
  char *name;
  uint32 pvr;
} cpu_info_t;


void Mcu_ConfigureFlash(void);



/* Haven't found any ID accessable from memory.
 * There is the DBGMCU_IDCODE (0xe0042000) found in RM0041 but it
 * you can't read from that address..
 */
#if 0
cpu_info_t cpu_info_list[] = {
    {
    .name = "????",
    .pvr = 0,
    },
};
#endif

/* The supported cores
 */
core_info_t core_info_list[] = {
    {
    .name = "CORE_ARM_CORTEX_M3",
    .pvr = CORE_CPUID_CORTEX_M3,
    },
};

#if 0
static cpu_info_t *Mcu_IdentifyCpu(uint32 pvr)
{
  int i;
  for (i = 0; i < ARRAY_SIZE(cpu_info_list); i++) {
    if (cpu_info_list[i].pvr == pvr) {
      return &cpu_info_list[i];
    }
  }

  return NULL;
}
#endif


static core_info_t *Mcu_IdentifyCore(uint32 pvr)
{
  int i;
  for (i = 0; i < ARRAY_SIZE(core_info_list); i++) {
    if (core_info_list[i].pvr == pvr) {
      return &core_info_list[i];
    }
  }

  return NULL;
}



/**
 * Identify the core, just to check that we have support for it.
 *
 * @return
 */
static uint32 Mcu_CheckCpu( void ) {

  uint32 pvr;
  //uint32 pir;
  //cpu_info_t *cpuType;
  core_info_t *coreType;

  //cpuType = Mcu_IdentifyCpu(pvr);
  coreType = Mcu_IdentifyCore(pvr);

  if( (coreType == NULL) ) {
    // Just hang
    while(1);
  }

  return 0;
}

/**
  * Set bus clocks.
  * SysClk  = 72MHz
  * AHBClk  = 72MHz
  * APB1Clk = 72MHz/2
  * APB2Clk = 72MHz
  */
static void SetClocks(void)
{
  volatile uint32 StartUpCounter = 0, HSEStatus = 0;

  /* SYSCLK, HCLK, PCLK2 and PCLK1 configuration ---------------------------*/
  /* Enable HSE */
  RCC->CR |= ((uint32_t)RCC_CR_HSEON);

  /* Wait till HSE is ready and if Time out is reached exit */
  do
  {
    HSEStatus = RCC->CR & RCC_CR_HSERDY;
    StartUpCounter++;
  } while((HSEStatus == 0) && (StartUpCounter != HSEStartUp_TimeOut));

  if ((RCC->CR & RCC_CR_HSERDY) != RESET)
  {
    HSEStatus = (uint32_t)0x01;
  }
  else
  {
    HSEStatus = (uint32_t)0x00;
  }

  if (HSEStatus == (uint32_t)0x01)
  {
    /* Enable Prefetch Buffer */
    FLASH->ACR |= FLASH_ACR_PRFTBE;

    /* Flash 2 wait state */
    FLASH->ACR &= (uint32_t)((uint32_t)~FLASH_ACR_LATENCY);
    FLASH->ACR |= (uint32_t)FLASH_ACR_LATENCY_2;


    /* HCLK = SYSCLK */
    RCC->CFGR |= (uint32_t)RCC_CFGR_HPRE_DIV1;

    /* PCLK2 = HCLK */
    RCC->CFGR |= (uint32_t)RCC_CFGR_PPRE2_DIV1;

    /* PCLK1 = HCLK */
    RCC->CFGR |= (uint32_t)RCC_CFGR_PPRE1_DIV2;

#ifdef STM32F10X_CL
    /* Configure PLLs ------------------------------------------------------*/
    /* PLL2 configuration: PLL2CLK = (HSE / 5) * 8 = 40 MHz */
    /* PREDIV1 configuration: PREDIV1CLK = PLL2 / 5 = 8 MHz */

    RCC->CFGR2 &= (uint32_t)~(RCC_CFGR2_PREDIV2 | RCC_CFGR2_PLL2MUL |
                              RCC_CFGR2_PREDIV1 | RCC_CFGR2_PREDIV1SRC);
    RCC->CFGR2 |= (uint32_t)(RCC_CFGR2_PREDIV2_DIV5 | RCC_CFGR2_PLL2MUL8 |
                             RCC_CFGR2_PREDIV1SRC_PLL2 | RCC_CFGR2_PREDIV1_DIV5);

    /* Enable PLL2 */
    RCC->CR |= RCC_CR_PLL2ON;
    /* Wait till PLL2 is ready */
    while((RCC->CR & RCC_CR_PLL2RDY) == 0)
    {
    }

    /* PLL configuration: PLLCLK = PREDIV1 * 9 = 72 MHz */
    RCC->CFGR &= (uint32_t)~(RCC_CFGR_PLLXTPRE | RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL);
    RCC->CFGR |= (uint32_t)(RCC_CFGR_PLLXTPRE_PREDIV1 | RCC_CFGR_PLLSRC_PREDIV1 |
                            RCC_CFGR_PLLMULL9);
#else
    /*  PLL configuration: PLLCLK = HSE * 9 = 72 MHz */
    RCC->CFGR &= (uint32_t)((uint32_t)~(RCC_CFGR_PLLSRC | RCC_CFGR_PLLXTPRE |
                                        RCC_CFGR_PLLMULL));
    RCC->CFGR |= (uint32_t)(RCC_CFGR_PLLSRC_HSE | RCC_CFGR_PLLMULL9);
#endif /* STM32F10X_CL */

    /* Enable PLL */
    RCC->CR |= RCC_CR_PLLON;

    /* Wait till PLL is ready */
    while((RCC->CR & RCC_CR_PLLRDY) == 0)
    {
    }

    /* Select PLL as system clock source */
    RCC->CFGR &= (uint32_t)((uint32_t)~(RCC_CFGR_SW));
    RCC->CFGR |= (uint32_t)RCC_CFGR_SW_PLL;

    /* Wait till PLL is used as system clock source */
    while ((RCC->CFGR & (uint32_t)RCC_CFGR_SWS) != (uint32_t)0x08)
    {
    }
  }
  else
  { /* HSE fails to start-up, the application will have wrong clock */
	  NVIC_SystemReset();
  }
}

/**
  * Initialize Flash, PLL and clocks.
  */
static void InitMcu(void)
{
  /* Reset the RCC clock configuration to the default reset state(for debug purpose) */
  /* Set HSION bit */
  RCC->CR |= (uint32_t)0x00000001;

  /* Reset SW, HPRE, PPRE1, PPRE2, ADCPRE and MCO bits */
#ifndef STM32F10X_CL
  RCC->CFGR &= (uint32_t)0xF8FF0000;
#else
  RCC->CFGR &= (uint32_t)0xF0FF0000;
#endif /* STM32F10X_CL */

  /* Reset HSEON, CSSON and PLLON bits */
  RCC->CR &= (uint32_t)0xFEF6FFFF;

  /* Reset HSEBYP bit */
  RCC->CR &= (uint32_t)0xFFFBFFFF;

  /* Reset PLLSRC, PLLXTPRE, PLLMUL and USBPRE/OTGFSPRE bits */
  RCC->CFGR &= (uint32_t)0xFF80FFFF;

#ifndef STM32F10X_CL
  /* Disable all interrupts and clear pending bits  */
  RCC->CIR = 0x009F0000;
#else
  /* Reset PLL2ON and PLL3ON bits */
  RCC->CR &= (uint32_t)0xEBFFFFFF;

  /* Disable all interrupts and clear pending bits  */
  RCC->CIR = 0x00FF0000;

  /* Reset CFGR2 register */
  RCC->CFGR2 = 0x00000000;
#endif /* STM32F10X_CL */

  /* Configure the System clock frequency, HCLK, PCLK2 and PCLK1 prescalers */
  /* Configure the Flash Latency cycles and enable prefetch buffer */
  SetClocks();
}

//-------------------------------------------------------------------

void Mcu_Init(const Mcu_ConfigType *configPtr)
{
  VALIDATE( ( NULL != configPtr ), MCU_INIT_SERVICE_ID, MCU_E_PARAM_CONFIG );

  if( !SIMULATOR() ) {
	  Mcu_CheckCpu();
  }

  memset(&Mcu_Global.stats,0,sizeof(Mcu_Global.stats));

  InitMcu();

  Irq_Enable();

  Mcu_Global.config = configPtr;
  Mcu_Global.initRun = 1;
}
//-------------------------------------------------------------------

void Mcu_DeInit()
{
  Mcu_Global.initRun = FALSE; // Very simple Deinit. Should we do more?
}

//-------------------------------------------------------------------
Std_ReturnType Mcu_InitRamSection(const Mcu_RamSectionType RamSection)
{
  VALIDATE_W_RV( ( 1 == Mcu_Global.initRun ), MCU_INITRAMSECTION_SERVICE_ID, MCU_E_UNINIT, E_NOT_OK );
  VALIDATE_W_RV( ( RamSection <= Mcu_Global.config->McuRamSectors ), MCU_INITRAMSECTION_SERVICE_ID, MCU_E_PARAM_RAMSECTION, E_NOT_OK );

  /* NOT SUPPORTED, reason: no support for external RAM */

  return E_OK;
}



//-------------------------------------------------------------------

Std_ReturnType Mcu_InitClock(const Mcu_ClockType ClockSetting)
{
  Mcu_ClockSettingConfigType *clockSettingsPtr;
  VALIDATE_W_RV( ( 1 == Mcu_Global.initRun ), MCU_INITCLOCK_SERVICE_ID, MCU_E_UNINIT, E_NOT_OK );
  VALIDATE_W_RV( ( ClockSetting < Mcu_Global.config->McuClockSettings ), MCU_INITCLOCK_SERVICE_ID, MCU_E_PARAM_CLOCK, E_NOT_OK );

  Mcu_Global.clockSetting = ClockSetting;
  clockSettingsPtr = &Mcu_Global.config->McuClockSettingConfig[Mcu_Global.clockSetting];


  return E_OK;
}

//-------------------------------------------------------------------

void Mcu_DistributePllClock(void)
{
  VALIDATE( ( 1 == Mcu_Global.initRun ), MCU_DISTRIBUTEPLLCLOCK_SERVICE_ID, MCU_E_UNINIT );
//  VALIDATE( ( FMPLL.SYNSR.B.LOCK == 1 ), MCU_DISTRIBUTEPLLCLOCK_SERVICE_ID, MCU_E_PLL_NOT_LOCKED );

  /* NOT IMPLEMENTED due to pointless function on this hardware */

}

//-------------------------------------------------------------------


Mcu_PllStatusType Mcu_GetPllStatus(void) {
	VALIDATE_W_RV( ( 1 == Mcu_Global.initRun ), MCU_GETPLLSTATUS_SERVICE_ID, MCU_E_UNINIT, MCU_PLL_STATUS_UNDEFINED );
	Mcu_PllStatusType rv;

	if (!SIMULATOR()) {
		if (RCC->CR & RCC_CR_PLLRDY) {
			rv = MCU_PLL_LOCKED;
		} else {
			rv = MCU_PLL_UNLOCKED;
		}
	} else {
		/* We are running on instruction set simulator. PLL is then always in sync... */
		rv = MCU_PLL_LOCKED;
	}

	return rv;
}

//-------------------------------------------------------------------

/**
 *
 * @return
 */
Mcu_ResetType Mcu_GetResetReason(void) {
	Mcu_ResetType rv;
	uint32_t csr;

	VALIDATE_W_RV( ( 1 == Mcu_Global.initRun ), MCU_GETRESETREASON_SERVICE_ID, MCU_E_UNINIT, MCU_RESET_UNDEFINED );

	csr = RCC->CSR;

	if (csr & RCC_CSR_SFTRSTF) {
		rv = MCU_SW_RESET;
	} else if (csr & (RCC_CSR_IWDGRSTF|RCC_CSR_WWDGRSTF) ) {
		rv = MCU_WATCHDOG_RESET;
	} else if ( csr & RCC_CSR_PORRSTF ) {
		rv = MCU_POWER_ON_RESET;
	} else {
		rv = MCU_RESET_UNDEFINED;
	}

	return rv;
}

//-------------------------------------------------------------------

/**
 * Shall read the raw reset value from hardware register if the hardware
 * supports this.
 *
 * @return
 */

Mcu_RawResetType Mcu_GetResetRawValue(void) {
	VALIDATE_W_RV( ( 1 == Mcu_Global.initRun ), MCU_GETRESETREASON_SERVICE_ID, MCU_E_UNINIT, MCU_GETRESETRAWVALUE_UNINIT_RV );

	if (!Mcu_Global.initRun) {
		return MCU_GETRESETRAWVALUE_UNINIT_RV;
	} else {
		return (RCC->CSR) & (RCC_CSR_RMVF | RCC_CSR_PINRSTF | RCC_CSR_PORRSTF
				| RCC_CSR_SFTRSTF | RCC_CSR_IWDGRSTF | RCC_CSR_WWDGRSTF
				| RCC_CSR_LPWRRSTF);
	}
	return 0;
}

//-------------------------------------------------------------------

#if ( MCU_PERFORM_RESET_API == STD_ON )
/**
 * Shell perform a microcontroller reset by using the hardware feature
 * of the micro controller.
 */
void Mcu_PerformReset(void)
{
  VALIDATE( ( 1 == Mcu_Global.initRun ), MCU_PERFORMRESET_SERVICE_ID, MCU_E_UNINIT );

  NVIC_SystemReset();
}
#endif

//-------------------------------------------------------------------

void Mcu_SetMode(const Mcu_ModeType McuMode)
{
  VALIDATE( ( 1 == Mcu_Global.initRun ), MCU_SETMODE_SERVICE_ID, MCU_E_UNINIT );
  VALIDATE( (0), MCU_SETMODE_SERVICE_ID, MCU_E_PARAM_MODE );
  //VALIDATE( ( McuMode <= Mcu_Global.config->McuNumberOfMcuModes ), MCU_SETMODE_SERVICE_ID, MCU_E_PARAM_MODE );
  (void) McuMode;

  /* NOT SUPPORTED */
}

//-------------------------------------------------------------------

/**
 * Get the system clock in Hz. It calculates the clock from the
 * different register settings in HW.
 */
uint32_t McuE_GetSystemClock(void)
{
  /*
   * System clock calculation
   *
   */

  // TODO: This of course wrong....
  uint32_t f_sys = 72000000UL;
#if 0
  uint32  extal = Mcu_Global.config->McuClockSettingConfig[Mcu_Global.clockSetting].McuClockReferencePointFrequency;

  f_sys =  CALC_SYSTEM_CLOCK(extal,emfd,eprediv,erfd);
#endif

//  f_sys = extal * (emfd+16) / ( (eprediv+1) * ( erfd+1 ));
  return f_sys;
}

imask_t McuE_EnterCriticalSection()
{
#if 0
  uint32_t msr = get_msr();
  Irq_Disable();
  return msr;
#endif
  return 0;
}

void McuE_ExitCriticalSection(uint32_t old_state)
{
#if 0
  set_msr(old_state);
#endif
}

/**
 * Get the peripheral clock in Hz for a specific device
 */

#if 0
uint32_t McuE_GetPeripheralClock(McuE_PeriperalClock_t type)
{

	return 0;
}
#endif


/**
 * Function to setup the internal flash for optimal performance
 */

void Mcu_ConfigureFlash(void)
{

}

void McuE_EnableInterrupts(void)
{
  Irq_Enable();
}

void McuE_DisableInterrupts(void)
{
  Irq_Disable();
}
