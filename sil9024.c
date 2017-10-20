#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/delay.h>

#include "siHdmiTx_902x_TPI.h"

#define DEVICE_NAME "sii9024"
#define sii9024_DEVICE_ID 0xB0

#define CLOCK_EDGE_RISING

#define HDMI_DEV_ADDR    0x72
#define HDMI_REG_NUM      1
#define HDMI_DATA_NUM     1

#define VIC_BASE                	0
#define HDMI_VIC_BASE           43
#define VIC_3D_BASE             	47
#define PC_BASE                 	64

static int norm = 0;
module_param(norm, int, S_IRUSR);

SIHDMITX_CONFIG		siHdmiTx;
GLOBAL_SYSTEM 		g_sys;
GLOBAL_HDCP 		g_hdcp;
GLOBAL_EDID 		g_edid;
byte 				tpivmode[3];  // saved TPI Reg0x08/Reg0x09/Reg0x0A values.

struct i2c_client *sii9024 = NULL;
struct i2c_client *siiEDID = NULL;
struct i2c_client *siiSegEDID = NULL;
struct i2c_client *siiHDCP = NULL;

struct delayed_work *sii9024_delayed_work;

static const struct i2c_board_info sii_info =
{ 
	I2C_BOARD_INFO("sii9024", 0x72),
};

/**************************************************************/
/**********************FUNCTION DEFINE*************************/
/**************************************************************/
int hi_i2c_read(unsigned char dev_addr, unsigned int reg_addr,
                unsigned int reg_addr_num, unsigned int data_byte_num);

int hi_i2c_write(unsigned char dev_addr,
                        unsigned int reg_addr, unsigned int reg_addr_num,
                        unsigned int data, unsigned int data_byte_num);

/**************************************************************/
/***********************FUNCTION DONE**************************/
/**************************************************************/
void DelayMS (word MS)
{
	mdelay(MS);//call linux kernel delay API function
}
int hi_i2c_write(unsigned char dev_addr,
                        unsigned int reg_addr, unsigned int reg_addr_num,
                        unsigned int data, unsigned int data_byte_num)
{
    unsigned char tmp_buf[8];
    int ret = 0;
    int idx = 0;
    struct i2c_client* client = sii9024;

    sii9024->addr = dev_addr;

    /* reg_addr config */
    tmp_buf[idx++] = reg_addr;
    if (reg_addr_num == 2)
    {
        client->flags  |= I2C_M_16BIT_REG;
        tmp_buf[idx++]  = (reg_addr >> 8);
    }
    else
    {
        client->flags &= ~I2C_M_16BIT_REG;
    }

    /* data config */
    tmp_buf[idx++] = data;
    if (data_byte_num == 2)
    {
        client->flags  |= I2C_M_16BIT_DATA;
        tmp_buf[idx++] = data >> 8;
    }
    else
    {
        client->flags &= ~I2C_M_16BIT_DATA;
    }

    ret = i2c_master_send(client, tmp_buf, idx);

    return ret;
}

int hi_i2c_read(unsigned char dev_addr, unsigned int reg_addr,
                unsigned int reg_addr_num, unsigned int data_byte_num)
{
    unsigned char tmp_buf[8];
    int ret = 0;
    int ret_data = 0xFF;
    int idx = 0;
    struct i2c_client* client = sii9024;

    sii9024->addr = dev_addr;

    /* reg_addr config */
    tmp_buf[idx++] = reg_addr;
    if (reg_addr_num == 2)
    {
        client->flags  |= I2C_M_16BIT_REG;
        tmp_buf[idx++] = reg_addr >> 8;
    }
    else
    {
        client->flags &= ~I2C_M_16BIT_REG;
    }

    /* data config */
    if (data_byte_num == 2)
    {
        client->flags |= I2C_M_16BIT_DATA;
    }
    else
    {
        client->flags &= ~I2C_M_16BIT_DATA;
    }

    ret = i2c_master_recv(client, tmp_buf, idx);
    if (ret >= 0)
    {
        if (data_byte_num == 2)
        {
            ret_data = tmp_buf[0] | (tmp_buf[1] << 8);
        }
        else
        {
            ret_data = tmp_buf[0];
        }
    }

    return ret_data;
}


//------------------------------------------------------------------------------
// Function Name: siHdmiTx_VideoSel()
// Function Description: Select output video mode
//
// Accepts: Video mode
// Returns: none
// Globals: none
//------------------------------------------------------------------------------
void siHdmiTx_VideoSel (byte vmode)
{
	siHdmiTx.HDMIVideoFormat 	= VMD_HDMIFORMAT_CEA_VIC;
	siHdmiTx.ColorSpace 		= YCBCR422_16BITS;
	siHdmiTx.ColorDepth			= VMD_COLOR_DEPTH_8BIT;
	siHdmiTx.SyncMode 			= EMBEDDED_SYNC;
		
	switch (vmode)
	{
		case HDMI_480I60_4X3:
			siHdmiTx.VIC 		= 6;
			siHdmiTx.AspectRatio 	= VMD_ASPECT_RATIO_4x3;
			siHdmiTx.Colorimetry	= COLORIMETRY_601;
			siHdmiTx.TclkSel		= X2;
			break;

		case HDMI_576I50_4X3:
			siHdmiTx.VIC 		= 21;
			siHdmiTx.AspectRatio 	= VMD_ASPECT_RATIO_4x3;
			siHdmiTx.Colorimetry	= COLORIMETRY_601;
			siHdmiTx.TclkSel		= X2;
			break;

		case HDMI_480P60_4X3:
			siHdmiTx.VIC 		= 2;
			siHdmiTx.AspectRatio 	= VMD_ASPECT_RATIO_4x3;
			siHdmiTx.Colorimetry	= COLORIMETRY_601;
			siHdmiTx.TclkSel		= X1;
			break;

		case HDMI_576P50_4X3:
			siHdmiTx.VIC 		= 17;
			siHdmiTx.AspectRatio 	= VMD_ASPECT_RATIO_4x3;
			siHdmiTx.Colorimetry	= COLORIMETRY_601;
			siHdmiTx.TclkSel		= X1;
			break;
			
		case HDMI_720P60:
			siHdmiTx.VIC 		= 4;
			siHdmiTx.AspectRatio 	= VMD_ASPECT_RATIO_16x9;
			siHdmiTx.Colorimetry	= COLORIMETRY_709;
			siHdmiTx.TclkSel		= X1;
			break;

		case HDMI_720P50:
			siHdmiTx.VIC 		= 19;
			siHdmiTx.AspectRatio 	= VMD_ASPECT_RATIO_16x9;
			siHdmiTx.Colorimetry	= COLORIMETRY_709;
			siHdmiTx.TclkSel		= X1;
			break;

		case HDMI_1080I60:
			siHdmiTx.VIC 		= 5;
			siHdmiTx.AspectRatio 	= VMD_ASPECT_RATIO_16x9;
			siHdmiTx.Colorimetry	= COLORIMETRY_709;
			siHdmiTx.TclkSel		= X1;
			break;

		case HDMI_1080I50:
			siHdmiTx.VIC 		= 20;
			siHdmiTx.AspectRatio 	= VMD_ASPECT_RATIO_16x9;
			siHdmiTx.Colorimetry	= COLORIMETRY_709;
			siHdmiTx.TclkSel		= X1;
			break;

		case HDMI_1080P60:
			siHdmiTx.VIC 		= 16;
			siHdmiTx.AspectRatio 	= VMD_ASPECT_RATIO_16x9;
			siHdmiTx.Colorimetry	= COLORIMETRY_709;
			siHdmiTx.TclkSel		= X1;
			break;

		case HDMI_1080P50:
			siHdmiTx.VIC 		= 31;
			siHdmiTx.AspectRatio 	= VMD_ASPECT_RATIO_16x9;
			siHdmiTx.Colorimetry	= COLORIMETRY_709;
			siHdmiTx.TclkSel		= X1;
			break;

		case HDMI_1024_768_60:
			siHdmiTx.VIC 		= PC_BASE+13;
			siHdmiTx.AspectRatio 	= VMD_ASPECT_RATIO_4x3;
			siHdmiTx.Colorimetry	= COLORIMETRY_709;
			siHdmiTx.TclkSel		= X1;
			break;
		case HDMI_800_600_60:
			siHdmiTx.VIC 		= PC_BASE+9;
			siHdmiTx.AspectRatio 	= VMD_ASPECT_RATIO_4x3;
			siHdmiTx.Colorimetry	= COLORIMETRY_709;
			siHdmiTx.TclkSel		= X1;
			break;
		default:
			break;
	}
}

//------------------------------------------------------------------------------
// Function Name: ReadByteTPI()
// Function Description: I2C read
//------------------------------------------------------------------------------
byte ReadByteTPI (byte RegOffset)
{
	byte Readnum;
	Readnum = hi_i2c_read(HDMI_DEV_ADDR,RegOffset,HDMI_REG_NUM,HDMI_DATA_NUM);
	return Readnum;
}

//------------------------------------------------------------------------------
// Function Name: WriteByteTPI()
// Function Description: I2C write
//------------------------------------------------------------------------------
void WriteByteTPI (byte RegOffset, byte Data)
{
	hi_i2c_write(HDMI_DEV_ADDR,RegOffset,HDMI_REG_NUM,Data,HDMI_DATA_NUM);
     
}
//------------------------------------------------------------------------------
// Function Name: WriteBlockTPI()
// Function Description: Write NBytes from a byte Buffer pointed to by Data to
//                      the TPI I2C slave starting at offset Addr
//------------------------------------------------------------------------------
void WriteBlockTPI (byte TPI_Offset, word NBytes, byte * pData)
{
	word NCount = 0;
	byte Data = 0;
	for(NCount = 0; NCount < NBytes; NCount++)
	{
		Data = *pData;
		hi_i2c_write(HDMI_DEV_ADDR,TPI_Offset,HDMI_REG_NUM,Data,HDMI_DATA_NUM);
		TPI_Offset += 0x01;
		pData++;
	}    	
}
//------------------------------------------------------------------------------
// Function Name: ReadSetWriteTPI()
// Function Description: Write "1" to all bits in TPI offset "Offset" that are set
//                  to "1" in "Pattern"; Leave all other bits in "Offset"
//                  unchanged.
//------------------------------------------------------------------------------
void ReadSetWriteTPI (byte Offset, byte Pattern)
{
    	byte Tmp;

    	Tmp = ReadByteTPI(Offset);

    	Tmp |= Pattern;
    	WriteByteTPI(Offset, Tmp);
}

//------------------------------------------------------------------------------
// Function Name: ReadSetWriteTPI()
// Function Description: Write "0" to all bits in TPI offset "Offset" that are set
//                  to "1" in "Pattern"; Leave all other bits in "Offset"
//                  unchanged.
//------------------------------------------------------------------------------
void ReadClearWriteTPI (byte Offset, byte Pattern)
{
    	byte Tmp;

    	Tmp = ReadByteTPI(Offset);

    	Tmp &= ~Pattern;
    	WriteByteTPI(Offset, Tmp);
}
//------------------------------------------------------------------------------
// Function Name: ReadModifyWriteIndexedRegister()
// Function Description: Write "Value" to all bits in TPI offset "Offset" that are set
//                  to "1" in "Mask"; Leave all other bits in "Offset"
//                  unchanged.
//------------------------------------------------------------------------------
void ReadModifyWriteIndexedRegister(byte PageNum, byte RegOffset, byte Mask, byte Value)
{
    	byte Tmp;
    	WriteByteTPI(TPI_INTERNAL_PAGE_REG, PageNum);
		WriteByteTPI(TPI_INDEXED_OFFSET_REG, RegOffset);
    	Tmp = ReadByteTPI(TPI_INDEXED_VALUE_REG);

    	Tmp &= ~Mask;
		Tmp |= (Value & Mask);

    	WriteByteTPI(TPI_INDEXED_VALUE_REG, Tmp);
}
//------------------------------------------------------------------------------
// Function Name: ReadIndexedRegister()
// Function Description: Read an indexed register value
//
//                  Write:
//                      1. 0xBC => Internal page num
//                      2. 0xBD => Indexed register offset
//
//                  Read:
//                      3. 0xBE => Returns the indexed register value
//------------------------------------------------------------------------------
byte ReadIndexedRegister(byte PageNum, byte RegOffset) 
{
    	WriteByteTPI(TPI_INTERNAL_PAGE_REG, PageNum);		// Internal page
    	WriteByteTPI(TPI_INDEXED_OFFSET_REG, RegOffset);	// Indexed register
    	return ReadByteTPI(TPI_INDEXED_VALUE_REG); 		// Return read value
}

//------------------------------------------------------------------------------
// Function Name: WriteIndexedRegister()
// Function Description: Write a value to an indexed register
//
//                  Write:
//                      1. 0xBC => Internal page num
//                      2. 0xBD => Indexed register offset
//                      3. 0xBE => Set the indexed register value
//------------------------------------------------------------------------------
void WriteIndexedRegister (byte PageNum, byte RegOffset, byte RegValue) 
{
    	WriteByteTPI(TPI_INTERNAL_PAGE_REG, PageNum);  // Internal page
   		WriteByteTPI(TPI_INDEXED_OFFSET_REG, RegOffset);  // Indexed register
    	WriteByteTPI(TPI_INDEXED_VALUE_REG, RegValue);    // Read value into buffer
}

//------------------------------------------------------------------------------
// Function Name: ReadSetWriteTPI()
// Function Description: Write "Value" to all bits in TPI offset "Offset" that are set
//                  to "1" in "Mask"; Leave all other bits in "Offset"
//                  unchanged.
//------------------------------------------------------------------------------
void ReadModifyWriteTPI (byte Offset, byte Mask, byte Value)
{
    	byte Tmp;

    	Tmp = ReadByteTPI(Offset);
    	Tmp &= ~Mask;
	Tmp |= (Value & Mask);
    	WriteByteTPI(Offset, Tmp);
}
//------------------------------------------------------------------------------
// Function Name: EnableInterrupts()
// Function Description: Enable the interrupts specified in the input parameter
//
// Accepts: A bit pattern with "1" for each interrupt that needs to be
//                  set in the Interrupt Enable Register (TPI offset 0x3C)
// Returns: TRUE
// Globals: none
//------------------------------------------------------------------------------
byte EnableInterrupts(byte Interrupt_Pattern)
{
    	TPI_TRACE_PRINT((">>EnableInterrupts()\n"));
    	ReadSetWriteTPI(TPI_INTERRUPT_ENABLE_REG, Interrupt_Pattern);
		return TRUE;
}

//------------------------------------------------------------------------------
// Function Name: DisableInterrupts()
// Function Description: Disable the interrupts specified in the input parameter
//
// Accepts: A bit pattern with "1" for each interrupt that needs to be
//                  cleared in the Interrupt Enable Register (TPI offset 0x3C)
// Returns: TRUE
// Globals: none
//------------------------------------------------------------------------------
byte DisableInterrupts (byte Interrupt_Pattern)
{
       TPI_TRACE_PRINT((">>DisableInterrupts()\n"));
    	ReadClearWriteTPI(TPI_INTERRUPT_ENABLE_REG, Interrupt_Pattern);

    	return TRUE;
}
//------------------------------------------------------------------------------
// Function Name: TxHW_Reset()
// Function Description: Hardware reset Tx
//------------------------------------------------------------------------------

void TXHAL_InitPostReset (void) 
{
	// Set terminations to default.
	WriteByteTPI(TMDS_CONT_REG, 0x25);
	// HW debounce to 64ms (0x14)
	WriteByteTPI(0x7C, 0x14);
}

void TxHW_Reset (void)
{
	TPI_TRACE_PRINT((">>TxHW_Reset()\n"));
	TXHAL_InitPostReset();
}

//------------------------------------------------------------------------------
// Function Name: InitializeStateVariables()
// Function Description: Initialize system state variables
//------------------------------------------------------------------------------
void InitializeStateVariables (void)
{
	g_sys.tmdsPoweredUp = FALSE;
	g_sys.hdmiCableConnected = FALSE;
	g_sys.dsRxPoweredUp = FALSE;
}

//------------------------------------------------------------------------------
// Function Name: StartTPI()
// Function Description: Start HW TPI mode by writing 0x00 to TPI address 0xC7.
//
// Accepts: none
// Returns: TRUE if HW TPI started successfully. FALSE if failed to.
// Globals: none
//------------------------------------------------------------------------------
byte StartTPI (void)
{
	byte devID = 0x00;
	word wID = 0x0000;	

	TPI_TRACE_PRINT((">>StartTPI()\n"));

	WriteByteTPI(TPI_ENABLE, 0x00);            // Write "0" to 72:C7 to start HW TPI mode
	
	DelayMS(100);

	devID = ReadIndexedRegister(INDEXED_PAGE_0, 0x03);
	wID = devID;
	wID <<= 8;
	devID = ReadIndexedRegister(INDEXED_PAGE_0, 0x02);
	wID |= devID;

	devID = ReadByteTPI(TPI_DEVICE_ID);

	TPI_TRACE_PRINT(("0x%04X\n", (int)wID));
	TPI_TRACE_PRINT(("%s:%d:devID=0x%04x \n", __func__,__LINE__,devID));

	if (devID == sii9024_DEVICE_ID)
	return TRUE;

	TPI_TRACE_PRINT(("Unsupported TX, devID = 0x%X\n", (int)devID));
	return FALSE;
}
//------------------------------------------------------------------------------
// Function Name: siHdmiTx_TPI_Init()
// Function Description: TPI initialization: HW Reset, Interrupt enable.
//
// Accepts: none
// Returns: TRUE or FLASE
// Globals: none
//------------------------------------------------------------------------------
byte siHdmiTx_TPI_Init (void)
{
	TPI_TRACE_PRINT(("\n>>siHdmiTx_TPI_Init()\n"));
	TPI_TRACE_PRINT(("\n%s\n", TPI_FW_VERSION));
	
  	// Chip powers up in D2 mode.
	g_sys.txPowerState = TX_POWER_STATE_D0;

	InitializeStateVariables();

	// Toggle TX reset pin
	TxHW_Reset();							
	// Enable HW TPI mode, check device ID
	if(StartTPI())
	{
		EnableInterrupts(HOT_PLUG_EVENT);

	//	WriteByteTPI(TPI_INTERRUPT_ENABLE_REG,0x01);	
		//WriteByteTPI(TPI_INTERRUPT_ENABLE_REG,0x01);	
		//WriteByteTPI(TPI_INTERRUPT_STATUS_REG,0x01);
	//	WriteByteTPI(TPI_SYSTEM_CONTROL_DATA_REG,0x10);
	//	WriteIndexedRegister(INDEXED_PAGE_0,0x0a,0x08);
	//	WriteByteTPI(TPI_INTERRUPT_ENABLE_REG,0x00);
//		WriteByteTPI(TPI_SYNC_GEN_CTRL,0x04);
		return 0;
	}
	return EPERM;
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////*************************///////////////////////////////
///////////////////////             AV CONFIG              ///////////////////////////////
///////////////////////*************************///////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

//------------------------------------------------------------------------------
// Video mode table
//------------------------------------------------------------------------------
struct ModeIdType
{
    byte Mode_C1;
    byte Mode_C2;
    byte SubMode;
};

struct PxlLnTotalType
{
    word Pixels;
    word Lines;
} ;
struct HVPositionType
{
    word H;
    word V;
};

struct HVResolutionType
{
    word H;
    word V;
};

struct TagType
{
    byte           	RefrTypeVHPol;
    word           	VFreq;
    struct PxlLnTotalType 	Total;
};

struct  _656Type
{
    byte IntAdjMode;
    word HLength;
    byte VLength;
    word Top;
    word Dly;
    word HBit2HSync;
    byte VBit2VSync;
    word Field2Offset;
};

struct Vspace_Vblank
{
    byte VactSpace1;
    byte VactSpace2;
    byte Vblank1;
    byte Vblank2;
    byte Vblank3;
};

//
// WARNING!  The entries in this enum must remian in the samre order as the PC Codes part
// of the VideoModeTable[].
//
typedef	enum
{
    PC_640x350_85_08 = 0,
    PC_640x400_85_08,
    PC_720x400_70_08,
    PC_720x400_85_04,
    PC_640x480_59_94,
    PC_640x480_72_80,
    PC_640x480_75_00,
    PC_640x480_85_00,
    PC_800x600_56_25,
    PC_800x600_60_317,
    PC_800x600_72_19,
    PC_800x600_75,
    PC_800x600_85_06,
    PC_1024x768_60,
    PC_1024x768_70_07,
    PC_1024x768_75_03,
    PC_1024x768_85,
    PC_1152x864_75,
    PC_1600x1200_60,
    PC_1280x768_59_95,
    PC_1280x768_59_87,
    PC_280x768_74_89,
    PC_1280x768_85,
    PC_1280x960_60,
    PC_1280x960_85,
    PC_1280x1024_60,
    PC_1280x1024_75,
    PC_1280x1024_85,
    PC_1360x768_60,
    PC_1400x105_59_95,
    PC_1400x105_59_98,
    PC_1400x105_74_87,
    PC_1400x105_84_96,
    PC_1600x1200_65,
    PC_1600x1200_70,
    PC_1600x1200_75,
    PC_1600x1200_85,
    PC_1792x1344_60,
    PC_1792x1344_74_997,
    PC_1856x1392_60,
    PC_1856x1392_75,
    PC_1920x1200_59_95,
    PC_1920x1200_59_88,
    PC_1920x1200_74_93,
    PC_1920x1200_84_93,
    PC_1920x1440_60,
    PC_1920x1440_75,
    PC_12560x1440_60,
    PC_SIZE			// Must be last
} PcModeCode_t;

struct VModeInfoType
{
    struct ModeIdType       	ModeId;
    dword             		PixClk;
    struct TagType          		Tag;
    struct HVPositionType  	Pos;
    struct HVResolutionType 	Res;
    byte             		AspectRatio;
    struct _656Type         		_656;
    byte             		PixRep;
    struct Vspace_Vblank 		VsVb;
    byte             		_3D_Struct;
};

#define NSM                     0   // No Sub-Mode

#define	DEFAULT_VIDEO_MODE		0	// 640  x 480p @ 60 VGA

#define ProgrVNegHNeg           0x00
#define ProgrVNegHPos           	0x01
#define ProgrVPosHNeg           	0x02
#define ProgrVPosHPos           	0x03

#define InterlaceVNegHNeg   	0x04
#define InterlaceVPosHNeg      0x05
#define InterlaceVNgeHPos    	0x06
#define InterlaceVPosHPos     	0x07

#define VIC_BASE                	0
#define HDMI_VIC_BASE           43
#define VIC_3D_BASE             	47
#define PC_BASE                 	64

// Aspect ratio
//=================================================
#define R_4                      		0   // 4:3
#define R_4or16                  	1   // 4:3 or 16:9
#define R_16                     		2   // 16:9

//
// These are the VIC codes that we support in a 3D mode
//
#define VIC_FOR_480P_60Hz_4X3			2		// 720p x 480p @60Hz
#define VIC_FOR_480P_60Hz_16X9			3		// 720p x 480p @60Hz
#define VIC_FOR_720P_60Hz				4		// 1280 x 720p @60Mhz
#define VIC_FOR_1080i_60Hz				5		// 1920 x 1080i @60Mhz
#define VIC_FOR_1080p_60Hz				16		// 1920 x 1080i @60hz
#define VIC_FOR_720P_50Hz				19		// 1280 x 720p @50Mhz
#define VIC_FOR_1080i_50Hz				20		// 1920 x 1080i @50Mhz
#define VIC_FOR_1080p_50Hz				31		// 1920 x 720p @50Hz
#define VIC_FOR_1080p_24Hz				32		// 1920 x 720p @24Hz


static struct VModeInfoType VModesTable[] =
{
	//===================================================================================================================================================================================================================================
    //         VIC                  Refresh type Refresh-Rate Pixel-Totals  Position     Active     Aspect   Int  Length          Hbit  Vbit  Field  Pixel          Vact Space/Blank
    //        1   2  SubM   PixClk  V/H Position       VFreq   H      V      H    V       H    V    Ratio    Adj  H   V  Top  Dly HSync VSync Offset Repl  Space1 Space2 Blank1 Blank2 Blank3  3D
    //===================================================================================================================================================================================================================================
    {{        1,  0, NSM},  2517,  {ProgrVNegHNeg,     6000, { 800,  525}}, {144, 35}, { 640, 480}, R_4,     {0,  96, 2, 33,  48,  16,  10,    0},    0,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 0 - 1.       640  x 480p @ 60 VGA
    {{        2,  3, NSM},  2700,  {ProgrVNegHNeg,     6000, { 858,  525}}, {122, 36}, { 720, 480}, R_4or16, {0,  62, 6, 30,  60,  19,   9,    0},    0,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 1 - 2,3      720  x 480p
    {{        4,  0, NSM},  7425,  {ProgrVPosHPos,     6000, {1650,  750}}, {260, 25}, {1280, 720}, R_16,    {0,  40, 5, 20, 220, 110,   5,    0},    0,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 2 - 4        1280 x 720p@60Hz
    {{        5,  0, NSM},  7425,  {InterlaceVPosHPos, 6000, {2200,  562}}, {192, 20}, {1920,1080}, R_16,    {0,  44, 5, 15, 148,  88,   2, 1100},    0,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 3 - 5        1920 x 1080i
    {{        6,  7, NSM},  2700,  {InterlaceVNegHNeg, 6000, {1716,  264}}, {119, 18}, { 720, 480}, R_4or16, {3,  62, 3, 15, 114,  17,   5,  429},    1,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 4 - 6,7      1440 x 480i,pix repl
    {{        8,  9,   1},  2700,  {ProgrVNegHNeg,     6000, {1716,  262}}, {119, 18}, {1440, 240}, R_4or16, {0, 124, 3, 15, 114,  38,   4,    0},    1,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 5 - 8,9(1)   1440 x 240p
    {{        8,  9,   2},  2700,  {ProgrVNegHNeg,     6000, {1716,  263}}, {119, 18}, {1440, 240}, R_4or16, {0, 124, 3, 15, 114,  38,   4,    0},    1,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 6 - 8,9(2)   1440 x 240p
    {{       10, 11, NSM},  5400,  {InterlaceVNegHNeg, 6000, {3432,  525}}, {238, 18}, {2880, 480}, R_4or16, {0, 248, 3, 15, 228,  76,   4, 1716},    0,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 7 - 10,11    2880 x 480i
    {{       12, 13,   1},  5400,  {ProgrVNegHNeg,     6000, {3432,  262}}, {238, 18}, {2880, 240}, R_4or16, {0, 248, 3, 15, 228,  76,   4,    0},    1,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 8 - 12,13(1) 2880 x 240p
    {{       12, 13,   2},  5400,  {ProgrVNegHNeg,     6000, {3432,  263}}, {238, 18}, {2880, 240}, R_4or16, {0, 248, 3, 15, 228,  76,   4,    0},    1,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 9 - 12,13(2) 2880 x 240p
    {{       14, 15, NSM},  5400,  {ProgrVNegHNeg,     6000, {1716,  525}}, {244, 36}, {1440, 480}, R_4or16, {0, 124, 6, 30, 120,  32,   9,    0},    0,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 10 - 14,15    1440 x 480p
    {{       16,  0, NSM}, 14835,  {ProgrVPosHPos,     6000, {2200, 1125}}, {192, 41}, {1920,1080}, R_16,    {0,  44, 5, 36, 148,  88,   4,    0},    0,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 11 - 16       1920 x 1080p
    {{       17, 18, NSM},  2700,  {ProgrVNegHNeg,     5000, { 864,  625}}, {132, 44}, { 720, 576}, R_4or16, {0,  64, 5, 39,  68,  12,   5,    0},    0,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 12 - 17,18    720  x 576p
    {{       19,  0, NSM},  7425,  {ProgrVPosHPos,     5000, {1980,  750}}, {260, 25}, {1280, 720}, R_16,    {0,  40, 5, 20, 220, 440,   5,    0},    0,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 13 - 19       1280 x 720p@50Hz
    {{       20,  0, NSM},  7425,  {InterlaceVPosHPos, 5000, {2640, 1125}}, {192, 20}, {1920,1080}, R_16,    {0,  44, 5, 15, 148, 528,   2, 1320},    0,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 14 - 20       1920 x 1080i
    {{       21, 22, NSM},  2700,  {InterlaceVNegHNeg, 5000, {1728,  625}}, {132, 22}, { 720, 576}, R_4,     {3,  63, 3, 19, 138,  24,   2,  432},    1,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 15 - 21,22    1440 x 576i
    {{       23, 24,   1},  2700,  {ProgrVNegHNeg,     5000, {1728,  312}}, {132, 22}, {1440, 288}, R_4or16, {0, 126, 3, 19, 138,  24,   2,    0},    1,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 16 - 23,24(1) 1440 x 288p
    {{       23, 24,   2},  2700,  {ProgrVNegHNeg,     5000, {1728,  313}}, {132, 22}, {1440, 288}, R_4or16, {0, 126, 3, 19, 138,  24,   2,    0},    1,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 17 - 23,24(2) 1440 x 288p
    {{       23, 24,   3},  2700,  {ProgrVNegHNeg,     5000, {1728,  314}}, {132, 22}, {1440, 288}, R_4or16, {0, 126, 3, 19, 138,  24,   2,    0},    1,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 18 - 23,24(3) 1440 x 288p
    {{       25, 26, NSM},  5400,  {InterlaceVNegHNeg, 5000, {3456,  625}}, {264, 22}, {2880, 576}, R_4or16, {0, 252, 3, 19, 276,  48,   2, 1728},    1,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 19 - 25,26    2880 x 576i
    {{       27, 28,   1},  5400,  {ProgrVNegHNeg,     5000, {3456,  312}}, {264, 22}, {2880, 288}, R_4or16, {0, 252, 3, 19, 276,  48,   2,    0},    1,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 20 - 27,28(1) 2880 x 288p
    {{       27, 28,   2},  5400,  {ProgrVNegHNeg,     5000, {3456,  313}}, {264, 22}, {2880, 288}, R_4or16, {0, 252, 3, 19, 276,  48,   3,    0},    1,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 21 - 27,28(2) 2880 x 288p
    {{       27, 28,   3},  5400,  {ProgrVNegHNeg,     5000, {3456,  314}}, {264, 22}, {2880, 288}, R_4or16, {0, 252, 3, 19, 276,  48,   4,    0},    1,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 22 - 27,28(3) 2880 x 288p
    {{       29, 30, NSM},  5400,  {ProgrVPosHNeg,     5000, {1728,  625}}, {264, 44}, {1440, 576}, R_4or16, {0, 128, 5, 39, 136,  24,   5,    0},    0,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 23 - 29,30    1440 x 576p
    {{       31,  0, NSM}, 14850,  {ProgrVPosHPos,     5000, {2640, 1125}}, {192, 41}, {1920,1080}, R_16,    {0,  44, 5, 36, 148, 528,   4,    0},    0,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 24 - 31(1)    1920 x 1080p
    {{       32,  0, NSM},  7417,  {ProgrVPosHPos,     2400, {2750, 1125}}, {192, 41}, {1920,1080}, R_16,    {0,  44, 5, 36, 148, 638,   4,    0},    0,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 25 - 32(2)    1920 x 1080p@24Hz
    {{       33,  0, NSM},  7425,  {ProgrVPosHPos,     2500, {2640, 1125}}, {192, 41}, {1920,1080}, R_16,    {0,  44, 5, 36, 148, 528,   4,    0},    0,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 26 - 33(3)    1920 x 1080p
    {{       34,  0, NSM},  7417,  {ProgrVPosHPos,     3000, {2200, 1125}}, {192, 41}, {1920,1080}, R_16,    {0,  44, 5, 36, 148, 528,   4,    0},    0,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 27 - 34(4)    1920 x 1080p
    {{       35, 36, NSM}, 10800,  {ProgrVNegHNeg,     5994, {3432,  525}}, {488, 36}, {2880, 480}, R_4or16, {0, 248, 6, 30, 240,  64,  10,    0},    0,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 28 - 35, 36   2880 x 480p@59.94/60Hz
    {{       37, 38, NSM}, 10800,  {ProgrVNegHNeg,     5000, {3456,  625}}, {272, 39}, {2880, 576}, R_4or16, {0, 256, 5, 40, 272,  48,   5,    0},    0,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 29 - 37, 38   2880 x 576p@50Hz
    {{       39,  0, NSM},  7200,  {InterlaceVNegHNeg, 5000, {2304, 1250}}, {352, 62}, {1920,1080}, R_16,    {0, 168, 5, 87, 184,  32,  24,    0},    0,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 30 - 39       1920 x 1080i@50Hz
    {{       40,  0, NSM}, 14850,  {InterlaceVPosHPos, 10000,{2640, 1125}}, {192, 20}, {1920,1080}, R_16,    {0,  44, 5, 15, 148, 528,   2, 1320},    0,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 31 - 40       1920 x 1080i@100Hz
    {{       41,  0, NSM}, 14850,  {InterlaceVPosHPos, 10000,{1980,  750}}, {260, 25}, {1280, 720}, R_16,    {0,  40, 5, 20, 220, 400,   5,    0},    0,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 32 - 41       1280 x 720p@100Hz
    {{       42, 43, NSM},  5400,  {ProgrVNegHNeg,     10000,{ 864,  144}}, {132, 44}, { 720, 576}, R_4or16, {0,  64, 5, 39,  68,  12,   5,    0},    0,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 33 - 42, 43,  720p x 576p@100Hz
    {{       44, 45, NSM},  5400,  {InterlaceVNegHNeg, 10000,{ 864,  625}}, {132, 22}, { 720, 576}, R_4or16, {0,  63, 3, 19,  69,  12,   2,  432},    1,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 34 - 44, 45,  720p x 576i@100Hz, pix repl
    {{       46,  0, NSM}, 14835,  {InterlaceVPosHPos, 11988,{2200, 1125}}, {192, 20}, {1920,1080}, R_16,    {0,  44, 5, 15, 149,  88,   2, 1100},    0,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 35 - 46,      1920 x 1080i@119.88/120Hz
    {{       47,  0, NSM}, 14835,  {ProgrVPosHPos,     11988,{1650,  750}}, {260, 25}, {1280, 720}, R_16,    {0,  40, 5, 20, 220, 110,   5, 1100},    0,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 36 - 47,      1280 x 720p@119.88/120Hz
    {{       48, 49, NSM},  5400,  {ProgrVNegHNeg,     11988,{ 858,  525}}, {122, 36}, { 720, 480}, R_4or16, {0,  62, 6, 30,  60,  16,  10,    0},    0,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 37 - 48, 49   720  x 480p@119.88/120Hz
    {{       50, 51, NSM},  5400,  {InterlaceVNegHNeg, 11988,{ 858,  525}}, {119, 18}, { 720, 480}, R_4or16, {0,  62, 3, 15,  57,  19,   4,  429},    1,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 38 - 50, 51   720  x 480i@119.88/120Hz
    {{       52, 53, NSM}, 10800,  {ProgrVNegHNeg,     20000,{ 864,  625}}, {132, 44}, { 720, 576}, R_4or16, {0,  64, 5, 39,  68,  12,   5,    0},    0,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 39 - 52, 53,  720  x 576p@200Hz
    {{       54, 55, NSM}, 10800,  {InterlaceVNegHNeg, 20000,{ 864,  625}}, {132, 22}, { 720, 576}, R_4or16, {0,  63, 3, 19,  69,  12,   2,  432},    1,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 40 - 54, 55,  1440 x 576i @200Hz, pix repl
    {{       56, 57, NSM}, 10800,  {ProgrVNegHNeg,     24000,{ 858,  525}}, {122, 42}, { 720, 480}, R_4or16, {0,  62, 6, 30,  60,  16,   9,    0},    0,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 41 - 56, 57,  720  x 480p @239.76/240Hz
    {{       58, 59, NSM}, 10800,  {InterlaceVNegHNeg, 24000,{ 858,  525}}, {119, 18}, { 720, 480}, R_4or16, {0,  62, 3, 15,  57,  19,   4,  429},    1,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 42 - 58, 59,  1440 x 480i @239.76/240Hz, pix repl

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// 4K x 2K Modes:
	//===================================================================================================================================================================================================================================
    //                                                                                                            Pulse
    //         VIC                  Refresh type Refresh-Rate Pixel-Totals   Position     Active    Aspect   Int  Width           Hbit  Vbit  Field  Pixel          Vact Space/Blank
    //        1   2  SubM   PixClk  V/H Position       VFreq   H      V      H    V       H    V    Ratio    Adj  H   V  Top  Dly HSync VSync Offset Repl  Space1 Space2 Blank1 Blank2 Blank3  3D
    //===================================================================================================================================================================================================================================
    {{        1,  0, NSM}, 297000, {ProgrVNegHNeg,     30000,{4400, 2250}}, {384, 82}, {3840,2160}, R_16,    {0,  88, 10, 72, 296, 176,  8,    0},   0,     {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 43 - 4k x 2k 29.97/30Hz (297.000 MHz)
    {{        2,  0, NSM}, 297000, {ProgrVNegHNeg,     29700,{5280, 2250}}, {384, 82}, {3840,2160}, R_16,    {0,  88, 10, 72, 296,1056,  8,    0},   0,     {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 44 - 4k x 2k 25Hz
    {{        3,  0, NSM}, 297000, {ProgrVNegHNeg,     24000,{5500, 2250}}, {384, 82}, {3840,2160}, R_16,    {0,  88, 10, 72, 296,1276,  8,    0},   0,     {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 45 - 4k x 2k 24Hz (297.000 MHz)
    {{        4  ,0, NSM}, 297000, {ProgrVNegHNeg,     24000,{6500, 2250}}, {384, 82}, {4096,2160}, R_16,    {0,  88, 10, 72, 296,1020,  8,    0},   0,     {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 46 - 4k x 2k 24Hz (SMPTE)

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// 3D Modes:
	//===================================================================================================================================================================================================================================
    //                                                                                                            Pulse
    //         VIC                  Refresh type Refresh-Rate Pixel-Totals  Position      Active    Aspect   Int  Width           Hbit  Vbit  Field  Pixel          Vact Space/Blank
    //        1   2  SubM   PixClk  V/H Position       VFreq   H      V      H    V       H    V    Ratio    Adj  H   V  Top  Dly HSync VSync Offset Repl  Space1 Space2 Blank1 Blank2 Blank3  3D
    //===================================================================================================================================================================================================================================
    {{        2,  3, NSM},  2700,  {ProgrVPosHPos,     6000, {858,   525}}, {122, 36}, { 720, 480}, R_4or16, {0,  62, 6, 30,  60,  16,   9,    0},    0,    {0,     0,     0,     0,    0},     8}, // 47 - 3D, 2,3 720p x 480p /60Hz, Side-by-Side (Half)
    {{        4,  0, NSM}, 14850,  {ProgrVPosHPos,     6000, {1650,  750}}, {260, 25}, {1280, 720}, R_16,    {0,  40, 5, 20, 220, 110,   5,    0},    0,    {0,     0,     0,     0,    0},     0}, // 48 - 3D  4   1280 x 720p@60Hz,  Frame Packing
    {{        5,  0, NSM}, 14850,  {InterlaceVPosHPos, 6000, {2200,  562}}, {192, 20}, {1920, 540}, R_16,    {0,  44, 5, 15, 148,  88,   2, 1100},    0,    {23,   22,     0,     0,    0},     0}, // 49 - 3D, 5,  1920 x 1080i/60Hz, Frame Packing
    {{        5,  0, NSM}, 14850,  {InterlaceVPosHPos, 6000, {2200,  562}}, {192, 20}, {1920, 540}, R_16,    {0,  44, 5, 15, 148,  88,   2, 1100},    0,    {0,     0,    22,    22,   23},     1}, // 50 - 3D, 5,  1920 x 1080i/60Hz, Field Alternative
    {{       16,  0, NSM}, 29700,  {ProgrVPosHPos,     6000, {2200, 1125}}, {192, 41}, {1920,1080}, R_16,    {0,  44, 5, 36, 148,  88,   4,    0},    0,    {0,     0,     0,     0,    0},     0}, // 51 - 3D, 16, 1920 x 1080p/60Hz, Frame Packing
    {{       16,  0, NSM}, 29700,  {ProgrVPosHPos,     6000, {2200, 1125}}, {192, 41}, {1920,1080}, R_16,    {0,  44, 5, 36, 148,  88,   4,    0},    0,    {0,     0,     0,     0,    0},     2}, // 52 - 3D, 16, 1920 x 1080p/60Hz, Line Alternative
    {{       16,  0, NSM}, 29700,  {ProgrVPosHPos,     6000, {2200, 1125}}, {192, 41}, {1920,1080}, R_16,    {0,  44, 5, 36, 148,  88,   4,    0},    0,    {0,     0,     0,     0,    0},     3}, // 53 - 3D, 16, 1920 x 1080p/60Hz, Side-by-Side (Full)
    {{       16,  0, NSM}, 14850,  {ProgrVPosHPos,     6000, {2200, 1125}}, {192, 41}, {1920,1080}, R_16,    {0,  44, 5, 36, 148,  88,   4,    0},    0,    {0,     0,     0,     0,    0},     8}, // 54 - 3D, 16, 1920 x 1080p/60Hz, Side-by-Side (Half)
    {{       19,  0, NSM}, 14850,  {ProgrVPosHPos,     5000, {1980,  750}}, {260, 25}, {1280, 720}, R_16,    {0,  40, 5, 20, 220, 440,   5,    0},    0,    {0,     0,     0,     0,    0},     0}, // 55 - 3D, 19, 1280 x 720p@50Hz,  Frame Packing
    {{       19,  0, NSM}, 14850,  {ProgrVPosHPos,     5000, {1980,  750}}, {260, 25}, {1280, 720}, R_16,    {0,  40, 5, 20, 220, 440,   5,    0},    0,    {0,     0,     0,     0,    0},     4}, // 56 - 3D, 19, 1280 x 720p/50Hz,  (L + depth)
    {{       19,  0, NSM}, 29700,  {ProgrVPosHPos,     5000, {1980,  750}}, {260, 25}, {1280, 720}, R_16,    {0,  40, 5, 20, 220, 440,   5,    0},    0,    {0,     0,     0,     0,    0},     5}, // 57 - 3D, 19, 1280 x 720p/50Hz,  (L + depth + Gfx + G-depth)
    {{       20,  0, NSM}, 14850,  {InterlaceVPosHPos, 5000, {2640,  562}}, {192, 20}, {1920, 540}, R_16,    {0,  44, 5, 15, 148, 528,   2, 1220},    0,    {23,   22,     0,     0,    0},     0}, // 58 - 3D, 20, 1920 x 1080i/50Hz, Frame Packing
    {{       20,  0, NSM}, 14850,  {InterlaceVPosHPos, 5000, {2640,  562}}, {192, 20}, {1920, 540}, R_16,    {0,  44, 5, 15, 148, 528,   2, 1220},    0,    {0,     0,    22,    22,   23},     1}, // 59 - 3D, 20, 1920 x 1080i/50Hz, Field Alternative
    {{       31,  0, NSM}, 29700,  {ProgrVPosHPos,     5000, {2640, 1125}}, {192, 41}, {1920,1080}, R_16,    {0,  44, 5, 36, 148, 528,   4,    0},    0,    {0,     0,     0,     0,    0},     0}, // 60 - 3D, 31, 1920 x 1080p/50Hz, Frame Packing
    {{       31,  0, NSM}, 29700,  {ProgrVPosHPos,     5000, {2640, 1125}}, {192, 41}, {1920,1080}, R_16,    {0,  44, 5, 36, 148, 528,   4,    0},    0,    {0,     0,     0,     0,    0},     2}, // 61 - 3D, 31, 1920 x 1080p/50Hz, Line Alternative
    {{       31,  0, NSM}, 29700,  {ProgrVPosHPos,     5000, {2650, 1125}}, {192, 41}, {1920,1080}, R_16,    {0,  44, 5, 36, 148, 528,   4,    0},    0,    {0,     0,     0,     0,    0},     3}, // 62 - 3D, 31, 1920 x 1080p/50Hz, Side-by-Side (Full)
    {{       32,  0, NSM}, 14850,  {ProgrVPosHPos,     2400, {2750, 1125}}, {192, 41}, {1920,1080}, R_16,    {0,  44, 5, 36, 148, 638,   4,    0},    0,    {0,     0,     0,     0,    0},     0}, // 63 - 3D, 32, 1920 x 1080p@24Hz, Frame Packing

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// NOTE: DO NOT ATTEMPT INPUT RESOLUTIONS THAT REQUIRE PIXEL CLOCK FREQUENCIES HIGHER THAN THOSE SUPPORTED BY THE TRANSMITTER CHIP

	//===================================================================================================================================================================================================================================
	//                                                                                                              Sync Pulse
    //  VIC                          Refresh type   fresh-Rate  Pixel-Totals    Position    Active     Aspect   Int  Width            Hbit  Vbit  Field  Pixel          Vact Space/Blank
    // 1   2  SubM         PixClk    V/H Position       VFreq   H      V        H    V       H    V     Ratio   {Adj  H   V  Top  Dly HSync VSync Offset} Repl  Space1 Space2 Blank1 Blank2 Blank3  3D
    //===================================================================================================================================================================================================================================
    {{PC_BASE  , 0,NSM},    3150,   {ProgrVNegHPos,     8508,   {832, 445}},    {160,63},   {640,350},   R_16,  {0,  64,  3,  60,  96,  32,  32,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 64 - 640x350@85.08
    {{PC_BASE+1, 0,NSM},    3150,   {ProgrVPosHNeg,     8508,   {832, 445}},    {160,44},   {640,400},   R_16,  {0,  64,  3,  41,  96,  32,   1,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 65 - 640x400@85.08
    {{PC_BASE+2, 0,NSM},    2700,   {ProgrVPosHNeg,     7008,   {900, 449}},    {0,0},      {720,400},   R_16,  {0,   0,  0,   0,   0,   0,   0,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 66 - 720x400@70.08
    {{PC_BASE+3, 0,NSM},    3500,   {ProgrVPosHNeg,     8504,   {936, 446}},    {20,45},    {720,400},   R_16,  {0,  72,  3,  42, 108,  36,   1,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 67 - 720x400@85.04
    {{PC_BASE+4, 0,NSM},    2517,   {ProgrVNegHNeg,     5994,   {800, 525}},    {144,35},   {640,480},   R_4,   {0,  96,  2,  33,  48,  16,  10,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 68 - 640x480@59.94
    {{PC_BASE+5, 0,NSM},    3150,   {ProgrVNegHNeg,     7281,   {832, 520}},    {144,31},   {640,480},   R_4,   {0,  40,  3,  28, 128, 128,   9,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 69 - 640x480@72.80
    {{PC_BASE+6, 0,NSM},    3150,   {ProgrVNegHNeg,     7500,   {840, 500}},    {21,19},    {640,480},   R_4,   {0,  64,  3,  28, 128,  24,   9,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 70 - 640x480@75.00
    {{PC_BASE+7,0,NSM},     3600,   {ProgrVNegHNeg,     8500,   {832, 509}},    {168,28},   {640,480},   R_4,   {0,  56,  3,  25, 128,  24,   9,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 71 - 640x480@85.00
    {{PC_BASE+8,0,NSM},     3600,   {ProgrVPosHPos,     5625,   {1024, 625}},   {200,24},   {800,600},   R_4,   {0,  72,  2,  22, 128,  24,   1,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 72 - 800x600@56.25
    {{PC_BASE+9,0,NSM},     4000,   {ProgrVPosHPos,     6032,   {1056, 628}},   {216,27},   {800,600},   R_4,   {0, 128,  4,  23,  88,  40,   1,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 73 - 800x600@60.317
    {{PC_BASE+10,0,NSM},    5000,   {ProgrVPosHPos,     7219,   {1040, 666}},   {184,29},   {800,600},   R_4,   {0, 120,  6,  23,  64,  56,  37,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 74 - 800x600@72.19
    {{PC_BASE+11,0,NSM},    4950,   {ProgrVPosHPos,     7500,   {1056, 625}},   {240,24},   {800,600},   R_4,   {0,  80,  3,  21, 160,  16,   1,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 75 - 800x600@75
    {{PC_BASE+12,0,NSM},    5625,   {ProgrVPosHPos,     8506,   {1048, 631}},   {216,30},   {800,600},   R_4,   {0,  64,  3,  27, 152,  32,   1,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 76 - 800x600@85.06
    {{PC_BASE+13,0,NSM},    6500,   {ProgrVNegHNeg,     6000,   {1344, 806}},   {296,35},   {1024,768},  R_4,   {0, 136,  6,  29, 160,  24,   3,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 77 - 1024x768@60
    {{PC_BASE+14,0,NSM},    7500,   {ProgrVNegHNeg,     7007,   {1328, 806}},   {280,35},   {1024,768},  R_4,   {0, 136,  6,  19, 144,  24,   3,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 78 - 1024x768@70.07
    {{PC_BASE+15,0,NSM},    7875,   {ProgrVPosHPos,     7503,   {1312, 800}},   {272,31},   {1024,768},  R_4,   {0,  96,  3,  28, 176,  16,   1,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 79 - 1024x768@75.03
    {{PC_BASE+16,0,NSM},    9450,   {ProgrVPosHPos,     8500,   {1376, 808}},   {304,39},   {1024,768},  R_4,   {0,  96,  3,  36, 208,  48,   1,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 80 - 1024x768@85
    {{PC_BASE+17,0,NSM},   10800,   {ProgrVPosHPos,     7500,   {1600, 900}},   {384,35},   {1152,864},  R_4,   {0, 128,  3,  32, 256,  64,   1,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 81 - 1152x864@75
    {{PC_BASE+18,0,NSM},   16200,   {ProgrVPosHPos,     6000,   {2160, 1250}},  {496,49},   {1600,1200}, R_4,   {0, 304,  3,  46, 304,  64,   1,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 82 - 1600x1200@60
    {{PC_BASE+19,0,NSM},    6825,   {ProgrVNegHPos,     6000,   {1440, 790}},   {112,19},   {1280,768},  R_16,  {0,  32,  7,  12,  80,  48,   3,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 83 - 1280x768@59.95
    {{PC_BASE+20,0,NSM},    7950,   {ProgrVPosHNeg,     5987,   {1664, 798}},   {320,27},   {1280,768},  R_16,  {0, 128,  7,  20, 192,  64,   3,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 84 - 1280x768@59.87
    {{PC_BASE+21,0,NSM},   10220,   {ProgrVPosHNeg,     6029,   {1696, 805}},   {320,27},   {1280,768},  R_16,  {0, 128,  7,  27, 208,  80,   3,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 85 - 1280x768@74.89
    {{PC_BASE+22,0,NSM},   11750,   {ProgrVPosHNeg,     8484,   {1712, 809}},   {352,38},   {1280,768},  R_16,  {0, 136,  7,  31, 216,  80,   3,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 86 - 1280x768@85
    {{PC_BASE+23,0,NSM},   10800,   {ProgrVPosHPos,     6000,   {1800, 1000}},  {424,39},   {1280,960},  R_4,   {0, 112,  3,  36, 312,  96,   1,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 87 - 1280x960@60
    {{PC_BASE+24,0,NSM},   14850,   {ProgrVPosHPos,     8500,   {1728, 1011}},  {384,50},   {1280,960},  R_4,   {0, 160,  3,  47, 224,  64,   1,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 88 - 1280x960@85
    {{PC_BASE+25,0,NSM},   10800,   {ProgrVPosHPos,     6002,   {1688, 1066}},  {360,41},   {1280,1024}, R_4,   {0, 112,  3,  38, 248,  48,   1,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 89 - 1280x1024@60
    {{PC_BASE+26,0,NSM},   13500,   {ProgrVPosHPos,     7502,   {1688, 1066}},  {392,41},   {1280,1024}, R_4,   {0, 144,  3,  38, 248,  16,   1,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 90 - 1280x1024@75
    {{PC_BASE+27,0,NSM},   15750,   {ProgrVPosHPos,     8502,   {1728, 1072}},  {384,47},   {1280,1024}, R_4,   {0, 160,  3,   4, 224,  64,   1,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 91 - 1280x1024@85
    {{PC_BASE+28,0,NSM},    8550,   {ProgrVPosHPos,     6002,   {1792, 795}},   {368,24},   {1360,768},  R_16,  {0, 112,  6,  18, 256,  64,   3,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 92 - 1360x768@60
    {{PC_BASE+29,0,NSM},   10100,   {ProgrVNegHPos,     5995,   {1560, 1080}},  {112,27},   {1400,1050}, R_4,   {0,  32,  4,  23,  80,  48,   3,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 93 - 1400x105@59.95
    {{PC_BASE+30,0,NSM},   12175,   {ProgrVPosHNeg,     5998,   {1864, 1089}},  {376,36},   {1400,1050}, R_4,   {0, 144,  4,  32, 232,  88,   3,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 94 - 1400x105@59.98
    {{PC_BASE+31,0,NSM},   15600,   {ProgrVPosHNeg,     7487,   {1896, 1099}},  {392,46},   {1400,1050}, R_4,   {0, 144,  4,  22, 248, 104,   3,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 95 - 1400x105@74.87
    {{PC_BASE+32,0,NSM},   17950,   {ProgrVPosHNeg,     8496,   {1912, 1105}},  {408,52},   {1400,1050}, R_4,   {0, 152,  4,  48, 256, 104,   3,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 96 - 1400x105@84.96
    {{PC_BASE+33,0,NSM},   17550,   {ProgrVPosHPos,     6500,   {2160, 1250}},  {496,49},   {1600,1200}, R_4,   {0, 192,  3,  46, 304,  64,   1,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 97 - 1600x1200@65
    {{PC_BASE+34,0,NSM},   18900,   {ProgrVPosHPos,     7000,   {2160, 1250}},  {496,49},   {1600,1200}, R_4,   {0, 192,  3,  46, 304,  64,   1,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 98 - 1600x1200@70
    {{PC_BASE+35,0,NSM},   20250,   {ProgrVPosHPos,     7500,   {2160, 1250}},  {496,49},   {1600,1200}, R_4,   {0, 192,  3,  46, 304,  64,   1,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 99 - 1600x1200@75
    {{PC_BASE+36,0,NSM},   22950,   {ProgrVPosHPos,     8500,   {2160, 1250}},  {496,49},   {1600,1200}, R_4,   {0, 192,  3,  46, 304,  64,   1,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 100 - 1600x1200@85
    {{PC_BASE+37,0,NSM},   20475,   {ProgrVPosHNeg,     6000,   {2448, 1394}},  {528,49},   {1792,1344}, R_4,   {0, 200,  3,  46, 328, 128,   1,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 101 - 1792x1344@60
    {{PC_BASE+38,0,NSM},   26100,   {ProgrVPosHNeg,     7500,   {2456, 1417}},  {568,72},   {1792,1344}, R_4,   {0, 216,  3,  69, 352,  96,   1,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 102 - 1792x1344@74.997
    {{PC_BASE+39,0,NSM},   21825,   {ProgrVPosHNeg,     6000,   {2528, 1439}},  {576,46},   {1856,1392}, R_4,   {0, 224,  3,  43, 352,  96,   1,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 103 - 1856x1392@60
    {{PC_BASE+40,0,NSM},   28800,   {ProgrVPosHNeg,     7500,   {2560, 1500}},  {576,107},  {1856,1392}, R_4,   {0, 224,  3, 104, 352, 128,   1,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 104 - 1856x1392@75
    {{PC_BASE+41,0,NSM},   15400,   {ProgrVNegHPos,     5995,   {2080, 1235}},  {112,32},   {1920,1200}, R_16,  {0,  32,  6,  26,  80,  48,   3,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 106 - 1920x1200@59.95
    {{PC_BASE+42,0,NSM},   19325,   {ProgrVPosHNeg,     5988,   {2592, 1245}},  {536,42},   {1920,1200}, R_16,  {0, 200,  6,  36, 336, 136,   3,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 107 - 1920x1200@59.88
    {{PC_BASE+43,0,NSM},   24525,   {ProgrVPosHNeg,     7493,   {2608, 1255}},  {552,52},   {1920,1200}, R_16,  {0, 208,  6,  46, 344, 136,   3,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 108 - 1920x1200@74.93
    {{PC_BASE+44,0,NSM},   28125,   {ProgrVPosHNeg,     8493,   {2624, 1262}},  {560,59},   {1920,1200}, R_16,  {0, 208,  6,  53, 352, 144,   3,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 109 - 1920x1200@84.93
    {{PC_BASE+45,0,NSM},   23400,   {ProgrVPosHNeg,     6000,   {2600, 1500}},  {552,59},   {1920,1440}, R_4,   {0, 208,  3,  56, 344, 128,   1,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 110 - 1920x1440@60
    {{PC_BASE+46,0,NSM},   29700,   {ProgrVPosHNeg,     7500,   {2640, 1500}},  {576,59},   {1920,1440}, R_4,   {0, 224,  3,  56, 352, 144,   1,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 111 - 1920x1440@75
    {{PC_BASE+47,0,NSM},   24150,   {ProgrVPosHNeg,     6000,   {2720, 1481}},  {48,  3},   {2560,1440}, R_16,  {0,  32,  5,  56, 352, 144,   1,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 112 - 2560x1440@60 // %%% need work
    {{PC_BASE+48,0,NSM},    2700,   {InterlaceVNegHNeg, 6000,   {1716,  264}},  {244,18},   {1440, 480},R_4or16,{3, 124,  3,  15, 114,  17,   5,     429},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 113 - 1440 x 480i 
};

#if 0
//------------------------------------------------------------------------------
// Aspect Ratio table defines the aspect ratio as function of VIC. This table
// should be used in conjunction with the 861-D part of VModeInfoType VModesTable[]
// (formats 0 - 59) because some formats that differ only in their AR are grouped
// together (e.g., formats 2 and 3).
//------------------------------------------------------------------------------
static u8 AspectRatioTable[] =
{
    R_4,   R_4, R_16, R_16, R_16,  R_4, R_16,  R_4, R_16,  R_4,
	R_16,  R_4, R_16,  R_4, R_16, R_16,  R_4, R_16, R_16, R_16,
	R_4,   R_16, R_4, R_16,  R_4, R_16,  R_4, R_16,  R_4, R_16,
	R_16,  R_16, R_16, R_16,  R_4, R_16,  R_4, R_16, R_16, R_16,
	R_16,  R_4, R_16,  R_4, R_16, R_16, R_16,  R_4, R_16,  R_4,
	R_16,  R_4, R_16,  R_4, R_16,  R_4, R_16,  R_4, R_16
};
#endif

//------------------------------------------------------------------------------
// VIC to Indexc table defines which VideoModeTable entry is appropreate for this VIC code. 
// Note: This table is valid ONLY for VIC codes in 861-D formats, NOT for HDMI_VIC codes
// or 3D codes!
//------------------------------------------------------------------------------
static u8 VIC2Index[] =
{
	0,  0,  1,  1,  2,  3,  4,  4,  5,  5,
	7,  7,  8,  8, 10, 10, 11, 12, 12, 13,
	14, 15, 15, 16, 16, 19, 19, 20, 20, 23,
	23, 24, 25, 26, 27, 28, 28, 29, 29, 30,
	31, 32, 33, 33, 34, 34, 35, 36, 37, 37,
	38, 38, 39, 39, 40, 40, 41, 41, 42, 42
};

//------------------------------------------------------------------------------
// Function Name: printVideoMode()
// Function Description: print video mode
//
// Accepts: siHdmiTx.VIC
// Returns: none
// Globals: none
//------------------------------------------------------------------------------
void printVideoMode (void)
{
	TPI_TRACE_PRINT((">>Video mode = "));

	switch (siHdmiTx.VIC)
	{
		case 6:
			TPI_TRACE_PRINT(("HDMI_480I60_4X3 \n"));
			break;
		case 21:
			TPI_TRACE_PRINT(("HDMI_576I50_4X3 \n"));
			break;
		case 2:
			TPI_TRACE_PRINT(("HDMI_480P60_4X3 \n"));
			break;
		case 17:
			TPI_TRACE_PRINT(("HDMI_576P50_4X3 \n"));
			break;
		case 4:
			TPI_TRACE_PRINT(("HDMI_720P60 \n"));
			break;
		case 19:
			TPI_TRACE_PRINT(("HDMI_720P50 \n"));
			break;
		case 5:
			TPI_TRACE_PRINT(("HDMI_1080I60 \n"));
			break;
		case 20:
			TPI_TRACE_PRINT(("HDMI_1080I50 \n"));
			break;
		case 16:
			TPI_TRACE_PRINT(("HDMI_1080P60 \n"));
			break;
		case 31:
			TPI_TRACE_PRINT(("HDMI_1080P50 \n"));
			break;
		case PC_BASE+13:
			TPI_TRACE_PRINT(("HDMI_1024_768_60 \n"));
			break;
		case PC_BASE+9:
			TPI_TRACE_PRINT(("HDMI_800_600_60 \n"));
			break;
		default:
			break;
	}
}

//------------------------------------------------------------------------------
// Function Name: ConvertVIC_To_VM_Index()
// Function Description: Convert Video Identification Code to the corresponding
//                  		index of VModesTable[]. Conversion also depends on the 
//					value of the 3D_Structure parameter in the case of 3D video format.
// Accepts: VIC to be converted; 3D_Structure value
// Returns: Index into VModesTable[] corrsponding to VIC
// Globals: VModesTable[] siHdmiTx
// Note: Conversion is for 861-D formats, HDMI_VIC or 3D
//------------------------------------------------------------------------------
byte ConvertVIC_To_VM_Index (void)
{
	byte index;

	//
	// The global VideoModeDescription contains all the information we need about
	// the Video mode for use to find its entry in the Videio mode table.
	//
	// The first issue.  The "VIC" may be a 891-D VIC code, or it might be an
	// HDMI_VIC code, or it might be a 3D code.  Each require different handling
	// to get the proper video mode table index.
	//
	if (siHdmiTx.HDMIVideoFormat == VMD_HDMIFORMAT_CEA_VIC)
	{
		//
		// This is a regular 861-D format VIC, so we use the VIC to Index
		// table to look up the index.
		//
        index = VIC2Index[siHdmiTx.VIC];
	}
	else if (siHdmiTx.HDMIVideoFormat == VMD_HDMIFORMAT_HDMI_VIC)
	{
		//
		// HDMI_VIC conversion is simple.  We need to subtract one because the codes start
		// with one instead of zero.  These values are from HDMI 1.4 Spec Table 8-13. 
		//
		if ((siHdmiTx.VIC < 1) || (siHdmiTx.VIC > 4))
		{
			index = DEFAULT_VIDEO_MODE;
		}
		else
		{
			index = (HDMI_VIC_BASE - 1) + siHdmiTx.VIC;
		}
	}
	else if (siHdmiTx.HDMIVideoFormat == VMD_HDMIFORMAT_3D)
	{
		//
		// Currently there are only a few VIC modes that we can do in 3D.  If the VIC code is not
		// one of these OR if the packing type is not supported for that VIC code, then it is an
		// error and we go to the default video mode.  See HDMI Spec 1.4 Table H-6.
		//
		switch (siHdmiTx.VIC)
		{
			case VIC_FOR_480P_60Hz_4X3:
			case VIC_FOR_480P_60Hz_16X9:
				// We only support Side-by-Side (Half) for these modes
				if (siHdmiTx.ThreeDStructure == SIDE_BY_SIDE_HALF)
					index = VIC_3D_BASE + 0;
				else
					index = DEFAULT_VIDEO_MODE;
				break;

			case VIC_FOR_720P_60Hz:
				switch(siHdmiTx.ThreeDStructure)
				{
					case FRAME_PACKING:
						index = VIC_3D_BASE + 1;
						break;
					default:
						index = DEFAULT_VIDEO_MODE;
						break;
				}
				break;

			case VIC_FOR_1080i_60Hz:
				switch(siHdmiTx.ThreeDStructure)
				{
					case FRAME_PACKING:
						index = VIC_3D_BASE + 2;
						break;
					case VMD_3D_FIELDALTERNATIVE:
						index = VIC_3D_BASE + 3;
						break;
					default:
						index = DEFAULT_VIDEO_MODE;
						break;
				}
				break;

			case VIC_FOR_1080p_60Hz:
				switch(siHdmiTx.ThreeDStructure)
				{
					case FRAME_PACKING:
						index = VIC_3D_BASE + 4;
						break;
					case VMD_3D_LINEALTERNATIVE:
						index = VIC_3D_BASE + 5;
						break;
					case SIDE_BY_SIDE_FULL:
						index = VIC_3D_BASE + 6;
						break;
					case SIDE_BY_SIDE_HALF:
						index = VIC_3D_BASE + 7;
						break;
					default:
						index = DEFAULT_VIDEO_MODE;
						break;
				}
				break;

			case VIC_FOR_720P_50Hz:
				switch(siHdmiTx.ThreeDStructure)
				{
					case FRAME_PACKING:
						index = VIC_3D_BASE + 8;
						break;
					case VMD_3D_LDEPTH:
						index = VIC_3D_BASE + 9;
						break;
					case VMD_3D_LDEPTHGRAPHICS:
						index = VIC_3D_BASE + 10;
						break;
					default:
						index = DEFAULT_VIDEO_MODE;
						break;
				}
				break;

			case VIC_FOR_1080i_50Hz:
				switch(siHdmiTx.ThreeDStructure)
				{
					case FRAME_PACKING:
						index = VIC_3D_BASE + 11;			
						break;
					case VMD_3D_FIELDALTERNATIVE:
						index = VIC_3D_BASE + 12;			
						break;
					default:
						index = DEFAULT_VIDEO_MODE;
						break;
				}
			break;

			case VIC_FOR_1080p_50Hz:
				switch(siHdmiTx.ThreeDStructure)
				{
					case FRAME_PACKING:
						index = VIC_3D_BASE + 13;
						break;
					case VMD_3D_LINEALTERNATIVE:
						index = VIC_3D_BASE + 14;
						break;
					case SIDE_BY_SIDE_FULL:
						index = VIC_3D_BASE + 15;
						break;
					default:
						index = DEFAULT_VIDEO_MODE;
						break;
				}
				break;

			case VIC_FOR_1080p_24Hz:
				switch(siHdmiTx.ThreeDStructure)
				{
					case FRAME_PACKING:
						index = VIC_3D_BASE + 16;
						break;
					default:
						index = DEFAULT_VIDEO_MODE;
						break;
				}
				break;

			default:
				index = DEFAULT_VIDEO_MODE;
				break;
		}
	}
	else if (siHdmiTx.HDMIVideoFormat == VMD_HDMIFORMAT_PC)
	{
		if (siHdmiTx.VIC < PC_SIZE)
		{
			index = siHdmiTx.VIC + PC_BASE;
		}
		else
		{
			index = DEFAULT_VIDEO_MODE;
		}
	}
	else
	{
		// This should never happen!  If so, default to first table entry
		index = DEFAULT_VIDEO_MODE;
	}
	
    	return index;
}

byte TPI_REG0x63_SAVED = 0;

//------------------------------------------------------------------------------
// Function Name: SetEmbeddedSync()
// Function Description: Set the 9022/4 registers to extract embedded sync.
//
// Accepts: Index of video mode to set
// Returns: TRUE
// Globals: VModesTable[]
//------------------------------------------------------------------------------
byte SetEmbeddedSync (void)
{
    	byte	ModeTblIndex;
    	word H_Bit_2_H_Sync;
    	word Field2Offset;
    	word H_SyncWidth;

    	byte V_Bit_2_V_Sync;
    	byte V_SyncWidth;
    	byte B_Data[8];

    	TPI_TRACE_PRINT((">>SetEmbeddedSync()\n"));

	ReadModifyWriteIndexedRegister(INDEXED_PAGE_0, 0x0A, 0x01, 0x00);   //set Output Format YCbCr 4:4:4
	ReadClearWriteTPI(TPI_SYNC_GEN_CTRL, MSBIT);	 // set 0x60[7] = 0 for External DE mode
//	WriteByteTPI(TPI_SYNC_GEN_CTRL, 0x04);                           //Vsync and Hsync Polarity settings 1 : Negative(leading edge falls)
	WriteByteTPI(TPI_DE_CTRL, 0x30);                           //Vsync and Hsync Polarity settings 1 : Negative(leading edge falls)
    ReadSetWriteTPI(TPI_SYNC_GEN_CTRL, MSBIT);       // set 0x60[7] = 1 for Embedded Sync

    	ModeTblIndex = ConvertVIC_To_VM_Index(); 

    	H_Bit_2_H_Sync = VModesTable[ModeTblIndex]._656.HBit2HSync;
    	Field2Offset = VModesTable[ModeTblIndex]._656.Field2Offset;
    	H_SyncWidth = VModesTable[ModeTblIndex]._656.HLength;
    	V_Bit_2_V_Sync = VModesTable[ModeTblIndex]._656.VBit2VSync;
    	V_SyncWidth = VModesTable[ModeTblIndex]._656.VLength;

    	B_Data[0] = H_Bit_2_H_Sync & LOW_BYTE;                  // Setup HBIT_TO_HSYNC 8 LSBits (0x62)

    	B_Data[1] = (H_Bit_2_H_Sync >> 8) & TWO_LSBITS;         // HBIT_TO_HSYNC 2 MSBits
    	B_Data[1] |= BIT_EN_SYNC_EXTRACT;                     // and Enable Embedded Sync to 0x63
    	TPI_REG0x63_SAVED = B_Data[1];

    	B_Data[2] = Field2Offset & LOW_BYTE;                    // 8 LSBits of "Field2 Offset" to 0x64
    	B_Data[3] = (Field2Offset >> 8) & LOW_NIBBLE;           // 2 MSBits of "Field2 Offset" to 0x65

    	B_Data[4] = H_SyncWidth & LOW_BYTE;
    	B_Data[5] = (H_SyncWidth >> 8) & TWO_LSBITS;                    // HWIDTH to 0x66, 0x67
    	B_Data[6] = V_Bit_2_V_Sync;                                     // VBIT_TO_VSYNC to 0x68
    	B_Data[7] = V_SyncWidth;                                        // VWIDTH to 0x69

	WriteBlockTPI(TPI_HBIT_TO_HSYNC_7_0, 8, &B_Data[0]);
	
    	return TRUE;
}

//------------------------------------------------------------------------------
// Function Name: EnableEmbeddedSync()
// Function Description: EnableEmbeddedSync
//
// Accepts: none
// Returns: none
// Globals: none
//------------------------------------------------------------------------------
void EnableEmbeddedSync (void)
{
    	TPI_TRACE_PRINT((">>EnableEmbeddedSync()\n"));

	ReadClearWriteTPI(TPI_SYNC_GEN_CTRL, MSBIT);	 // set 0x60[7] = 0 for DE mode
	WriteByteTPI(TPI_DE_CTRL, 0x30);
    ReadSetWriteTPI(TPI_SYNC_GEN_CTRL, MSBIT);       // set 0x60[7] = 1 for Embedded Sync
	ReadSetWriteTPI(TPI_DE_CTRL, BIT_6);
}
//------------------------------------------------------------------------------
// Function Name: SetFormat()
// Function Description: Set the 9022/4 format
//
// Accepts: none
// Returns: DE_SET_OK
// Globals: none
//------------------------------------------------------------------------------
void SetFormat (byte *Data)
{
	ReadModifyWriteTPI(TPI_SYSTEM_CONTROL_DATA_REG, OUTPUT_MODE_MASK, OUTPUT_MODE_HDMI); // Set HDMI mode to allow color space conversion
	
	WriteBlockTPI(TPI_INPUT_FORMAT_REG, 2, Data);   // Program TPI AVI Input and Output Format
	WriteByteTPI(TPI_END_RIGHT_BAR_MSB, 0x00);	  // Set last byte of TPI AVI InfoFrame for TPI AVI I/O Format to take effect

	ReadModifyWriteTPI(TPI_SYSTEM_CONTROL_DATA_REG, OUTPUT_MODE_MASK, OUTPUT_MODE_DVI);

	if (siHdmiTx.SyncMode == EMBEDDED_SYNC)
		EnableEmbeddedSync();					// Last byte of TPI AVI InfoFrame resets Embedded Sync Extraction
}

//------------------------------------------------------------------------------
// Function Name: SetDE()
// Function Description: Set the 9022/4 internal DE generator parameters
//
// Accepts: none
// Returns: DE_SET_OK
// Globals: none
//
// NOTE: 0x60[7] must be set to "0" for the follwing settings to take effect
//------------------------------------------------------------------------------
byte SetDE (void)
{
    	byte RegValue;
	byte	ModeTblIndex;
	
    	word H_StartPos, V_StartPos;
	word Htotal, Vtotal;
    	word H_Res, V_Res;

    	byte Polarity;
    	byte B_Data[12];

    	TPI_TRACE_PRINT((">>SetDE()\n"));

    	ModeTblIndex = ConvertVIC_To_VM_Index();

    	if (VModesTable[ModeTblIndex]._3D_Struct != NO_3D_SUPPORT)
    	{
        	return DE_CANNOT_BE_SET_WITH_3D_MODE;
        	TPI_TRACE_PRINT((">>SetDE() not allowed with 3D video format\n"));
    	}

    	// Make sure that External Sync method is set before enableing the DE Generator:
    	RegValue = ReadByteTPI(TPI_SYNC_GEN_CTRL);

    	if (RegValue & BIT_7)
    	{
        	return DE_CANNOT_BE_SET_WITH_EMBEDDED_SYNC;
    	}

    	H_StartPos = VModesTable[ModeTblIndex].Pos.H;
    	V_StartPos = VModesTable[ModeTblIndex].Pos.V;

   	Htotal = VModesTable[ModeTblIndex].Tag.Total.Pixels;
	Vtotal = VModesTable[ModeTblIndex].Tag.Total.Lines;

    	Polarity = (~VModesTable[ModeTblIndex].Tag.RefrTypeVHPol) & TWO_LSBITS;

    	H_Res = VModesTable[ModeTblIndex].Res.H;

       if ((VModesTable[ModeTblIndex].Tag.RefrTypeVHPol & 0x04))
       {
        	V_Res = (VModesTable[ModeTblIndex].Res.V) >> 1;       //if interlace V-resolution divided by 2
       }
       else
       {
            V_Res = (VModesTable[ModeTblIndex].Res.V);
       }

    	B_Data[0] = H_StartPos & LOW_BYTE;              // 8 LSB of DE DLY in 0x62

    	B_Data[1] = (H_StartPos >> 8) & TWO_LSBITS;     // 2 MSBits of DE DLY to 0x63
    	B_Data[1] |= (Polarity << 4);                   // V and H polarity
    	B_Data[1] |= BIT_EN_DE_GEN;                     // enable DE generator

    	B_Data[2] = V_StartPos & SEVEN_LSBITS;      // DE_TOP in 0x64
    	B_Data[3] = 0x00;                           // 0x65 is reserved
    	B_Data[4] = H_Res & LOW_BYTE;               // 8 LSBits of DE_CNT in 0x66
    	B_Data[5] = (H_Res >> 8) & LOW_NIBBLE;      // 4 MSBits of DE_CNT in 0x67
    	B_Data[6] = V_Res & LOW_BYTE;               // 8 LSBits of DE_LIN in 0x68
    	B_Data[7] = (V_Res >> 8) & THREE_LSBITS;    // 3 MSBits of DE_LIN in 0x69
	B_Data[8] = Htotal & LOW_BYTE;				// 8 LSBits of H_RES in 0x6A
	B_Data[9] =	(Htotal >> 8) & LOW_NIBBLE;		// 4 MSBITS of H_RES in 0x6B
	B_Data[10] = Vtotal & LOW_BYTE;				// 8 LSBits of V_RES in 0x6C
	B_Data[11] = (Vtotal >> 8) & BITS_2_1_0;	// 3 MSBITS of V_RES in 0x6D

    	WriteBlockTPI(TPI_DE_DLY, 12, &B_Data[0]);
	TPI_REG0x63_SAVED = B_Data[1];

    	return DE_SET_OK;                               // Write completed successfully
}
//------------------------------------------------------------------------------
// Function Name: InitVideo()
// Function Description: Set the 9022/4 to the video mode determined by GetVideoMode()
//
// Accepts: Index of video mode to set; Flag that distinguishes between
//                  calling this function after power up and after input
//                  resolution change
// Returns: TRUE
// Globals: VModesTable, VideoCommandImage
//------------------------------------------------------------------------------
byte InitVideo (byte TclkSel)
{
	byte	ModeTblIndex;
	
#ifdef DEEP_COLOR
	byte temp;
#endif
	byte B_Data[8];

	byte EMB_Status; //EmbeddedSync set flag
	byte DE_Status;
	byte Pattern;

	TPI_TRACE_PRINT((">>InitVideo()\n"));
	printVideoMode();
	TPI_TRACE_PRINT((" HF:%d", (int) siHdmiTx.HDMIVideoFormat));
	TPI_TRACE_PRINT((" VIC:%d", (int) siHdmiTx.VIC));
	TPI_TRACE_PRINT((" A:%x", (int) siHdmiTx.AspectRatio));
	TPI_TRACE_PRINT((" CS:%x", (int) siHdmiTx.ColorSpace));
	TPI_TRACE_PRINT((" CD:%x", (int) siHdmiTx.ColorDepth));
	TPI_TRACE_PRINT((" CR:%x", (int) siHdmiTx.Colorimetry));
	TPI_TRACE_PRINT((" SM:%x", (int) siHdmiTx.SyncMode));
	TPI_TRACE_PRINT((" TCLK:%x", (int) siHdmiTx.TclkSel)); 
	TPI_TRACE_PRINT((" 3D:%d", (int) siHdmiTx.ThreeDStructure));
	TPI_TRACE_PRINT((" 3Dx:%d\n", (int) siHdmiTx.ThreeDExtData));

	ModeTblIndex = (byte)ConvertVIC_To_VM_Index();

	Pattern = (TclkSel << 6) & TWO_MSBITS;							// Use TPI 0x08[7:6] for 9022A/24A video clock multiplier
	ReadSetWriteTPI(TPI_PIX_REPETITION, Pattern);			//TClkSel1:Ratio of output TMDS clock to input video clock,00-x0.5,01- x1 (default),10 -x2,11-x4
	WriteByteTPI(TPI_PIX_REPETITION, 0x70);
	// Take values from VModesTable[]:
	if( (siHdmiTx.VIC == 6) || (siHdmiTx.VIC == 7) ||	//480i
	     (siHdmiTx.VIC == 21) || (siHdmiTx.VIC == 22) )	//576i
	{
		if( siHdmiTx.ColorSpace == YCBCR422_8BITS)	//27Mhz pixel clock
		{
			B_Data[0] = VModesTable[ModeTblIndex].PixClk & 0x00FF;
			B_Data[1] = (VModesTable[ModeTblIndex].PixClk >> 8) & 0xFF;
		}
		else											//13.5Mhz pixel clock
		{
			B_Data[0] = (VModesTable[ModeTblIndex].PixClk /2) & 0x00FF;
			B_Data[1] = ((VModesTable[ModeTblIndex].PixClk /2) >> 8) & 0xFF;
		}
	
	}
	else
	{
		B_Data[0] = VModesTable[ModeTblIndex].PixClk & 0x00FF;			// write Pixel clock to TPI registers 0x00, 0x01
		B_Data[1] = (VModesTable[ModeTblIndex].PixClk >> 8) & 0xFF;
		//B_Data[0] = 0xf9;
		//B_Data[1] = 0x1c;
	}
	
	B_Data[2] = VModesTable[ModeTblIndex].Tag.VFreq & 0x00FF;		// write Vertical Frequency to TPI registers 0x02, 0x03
	B_Data[3] = (VModesTable[ModeTblIndex].Tag.VFreq >> 8) & 0xFF;
	//B_Data[2] = 0xb8;
 	//B_Data[3] = 0xb;	

	if( (siHdmiTx.VIC == 6) || (siHdmiTx.VIC == 7) ||	//480i
	     (siHdmiTx.VIC == 21) || (siHdmiTx.VIC == 22) )	//576i
	{
		B_Data[4] = (VModesTable[ModeTblIndex].Tag.Total.Pixels /2) & 0x00FF;	// write total number of pixels to TPI registers 0x04, 0x05
		B_Data[5] = ((VModesTable[ModeTblIndex].Tag.Total.Pixels /2) >> 8) & 0xFF;
	}
	else
	{
		B_Data[4] = VModesTable[ModeTblIndex].Tag.Total.Pixels & 0x00FF;	// write total number of pixels to TPI registers 0x04, 0x05
		B_Data[5] = (VModesTable[ModeTblIndex].Tag.Total.Pixels >> 8) & 0xFF;
	}
	
	B_Data[6] = VModesTable[ModeTblIndex].Tag.Total.Lines & 0x00FF;	// write total number of lines to TPI registers 0x06, 0x07
	B_Data[7] = (VModesTable[ModeTblIndex].Tag.Total.Lines >> 8) & 0xFF;

	WriteBlockTPI(TPI_PIX_CLK_LSB, 8, B_Data);						// Write TPI Mode data.//0x00-0x07 :Video Mode Defines the incoming resolution
#if 1
	// TPI Input Bus and Pixel Repetition Data
	// B_Data[0] = Reg0x08;
	B_Data[0] = 0;  // Set to default 0 for use again
	//B_Data[0] = (VModesTable[ModeTblIndex].PixRep) & LOW_BYTE;		// Set pixel replication field of 0x08
	B_Data[0] |= BIT_BUS_24;										// Set 24 bit bus:Input Bus Select. The input data bus can be either one pixel wide or 1/2  pixel wide. The bit defaults to 1 to select full pixel mode. In  1/2  pixel mode, the full pixel is brought in on two successive clock edges (one rising, one falling). 
																//All parts support 24-bit full-pixel and 12-bit half-pixel input modes.
	B_Data[0] |= (TclkSel << 6) & TWO_MSBITS;

#ifdef CLOCK_EDGE_FALLING
	B_Data[0] &= ~BIT_EDGE_RISE;									// Set to falling edge
#endif
#ifdef CLOCK_EDGE_RISING
	B_Data[0] |= BIT_EDGE_RISE;									// Set to rising edge
#endif
#endif
//	B_Data[0] = 0x70;
	tpivmode[0] = B_Data[0]; // saved TPI Reg0x08 value.
	WriteByteTPI(TPI_PIX_REPETITION, B_Data[0]);					// 0x08

	// TPI AVI Input and Output Format Data
	// B_Data[0] = Reg0x09;
	// B_Data[1] = Reg0x0A;	
	B_Data[0] = 0;  // Set to default 0 for use again
	B_Data[1] = 0;  // Set to default 0 for use again
	
	if (siHdmiTx.SyncMode == EMBEDDED_SYNC)
	{
		EMB_Status = SetEmbeddedSync();
		EnableEmbeddedSync();    //enablle EmbeddedSync
	}

	if (siHdmiTx.SyncMode == INTERNAL_DE)
	{
		ReadClearWriteTPI(TPI_SYNC_GEN_CTRL, MSBIT);	// set 0x60[7] = 0 for External Sync
		DE_Status = SetDE();								// Call SetDE() with Video Mode as a parameter
	}

	if (siHdmiTx.ColorSpace == RGB)
		B_Data[0] = (((BITS_IN_RGB | BITS_IN_AUTO_RANGE) & ~BIT_EN_DITHER_10_8) & ~BIT_EXTENDED_MODE); // reg0x09
	
	else if (siHdmiTx.ColorSpace == YCBCR444)
		B_Data[0] = (((BITS_IN_YCBCR444 | BITS_IN_AUTO_RANGE) & ~BIT_EN_DITHER_10_8) & ~BIT_EXTENDED_MODE); // 0x09
		
	else if ((siHdmiTx.ColorSpace == YCBCR422_16BITS) ||(siHdmiTx.ColorSpace == YCBCR422_8BITS))
		B_Data[0] = (((BITS_IN_YCBCR422 | BITS_IN_AUTO_RANGE) & ~BIT_EN_DITHER_10_8) & ~BIT_EXTENDED_MODE); // 0x09
	
#ifdef DEEP_COLOR
	switch (siHdmiTx.ColorDepth)
	{
		case 0:  temp = 0x00; ReadModifyWriteTPI(TPI_DEEP_COLOR_GCP, BIT_2, 0x00); break;
		case 1:  temp = 0x80; ReadModifyWriteTPI(TPI_DEEP_COLOR_GCP, BIT_2, BIT_2); break;
		case 2:  temp = 0xC0; ReadModifyWriteTPI(TPI_DEEP_COLOR_GCP, BIT_2, BIT_2); break;
		case 3:  temp = 0x40; ReadModifyWriteTPI(TPI_DEEP_COLOR_GCP, BIT_2, BIT_2); break;
		default: temp = 0x00; ReadModifyWriteTPI(TPI_DEEP_COLOR_GCP, BIT_2, 0x00); break;
		//General Control Packet – Deep color settings require the General Control Packet to be sent once per video field
		//with the correct PP and CD information. This must be enabled by software via TPI Deep Color Packet Enable
		//Register 0x40[2] = 1, enable transmission of the GCP packet.
	}
	B_Data[0] = ((B_Data[0] & 0x3F) | temp);// reg0x09
#endif

	B_Data[1] = (BITS_OUT_RGB | BITS_OUT_AUTO_RANGE);  //Reg0x0A

	if ((siHdmiTx.VIC == 6) || (siHdmiTx.VIC == 7) ||	//480i
	     (siHdmiTx.VIC == 21) || (siHdmiTx.VIC == 22) ||//576i
	     (siHdmiTx.VIC == 2) || (siHdmiTx.VIC == 3) ||	//480p
	     (siHdmiTx.VIC == 17) ||(siHdmiTx.VIC == 18))	//576p
	{
		B_Data[1] &= ~BIT_BT_709;
	}
	else
	{
		B_Data[1] |= BIT_BT_709;
	}
	
#ifdef DEEP_COLOR
	B_Data[1] = ((B_Data[1] & 0x3F) | temp);
#endif

#ifdef DEV_SUPPORT_EDID
	if (!IsHDMI_Sink()) 
	{
		B_Data[1] = ((B_Data[1] & 0xFC) | BITS_OUT_RGB);
	}
	else 
	{
		// Set YCbCr color space depending on EDID
		if (g_edid.YCbCr_4_4_4) 
		{
			B_Data[1] = ((B_Data[1] & 0xFC) | BITS_OUT_YCBCR444);
		}
		else 
		{
			if (g_edid.YCbCr_4_2_2)
			{
				B_Data[1] = ((B_Data[1] & 0xFC) | BITS_OUT_YCBCR422);
			}
			else
			{
				B_Data[1] = ((B_Data[1] & 0xFC) | BITS_OUT_RGB);
			}
		}
	}
#endif
	//B_Data[0] = 0x02;
	tpivmode[1] = B_Data[0];	// saved TPI Reg0x09 value.
	tpivmode[2] = B_Data[1];  // saved TPI Reg0x0A value.
	SetFormat(B_Data);

	ReadClearWriteTPI(TPI_SYNC_GEN_CTRL, BIT_2);	// Number HSync pulses from VSync active edge to Video Data Period should be 20 (VS_TO_VIDEO)

	return TRUE;
}

//------------------------------------------------------------------------------
// Function Name: siHdmiTx_PowerStateD0()
// Function Description: Set TX to D0 mode.
//------------------------------------------------------------------------------
void siHdmiTx_PowerStateD0 (void)
{
	ReadModifyWriteTPI(TPI_DEVICE_POWER_STATE_CTRL_REG, TX_POWER_STATE_MASK, TX_POWER_STATE_D0);
	TPI_DEBUG_PRINT(("TX Power State D0\n"));
	g_sys.txPowerState = TX_POWER_STATE_D0;
}

//------------------------------------------------------------------------------
// Function Name: siHdmiTx_Init()
// Function Description: Set the 9022/4 video and video.
//
// Accepts: none
// Returns: none
// Globals: siHdmiTx
//------------------------------------------------------------------------------
void siHdmiTx_Init (void) 
{
	TPI_TRACE_PRINT((">>siHdmiTx_Init()\n"));
	InitVideo(siHdmiTx.TclkSel);			// Set PLL Multiplier to x1 upon power up
	siHdmiTx_PowerStateD0();

	WriteByteTPI(TPI_DE_CTRL, TPI_REG0x63_SAVED);
	WriteByteTPI(TPI_YC_Input_Mode, 0x00);

	TPI_DEBUG_PRINT (("TMDS -> Enabled\n"));
	ReadModifyWriteTPI(TPI_SYSTEM_CONTROL_DATA_REG, LINK_INTEGRITY_MODE_MASK | TMDS_OUTPUT_CONTROL_MASK | AV_MUTE_MASK, LINK_INTEGRITY_DYNAMIC | TMDS_OUTPUT_CONTROL_ACTIVE | AV_MUTE_NORMAL);

	WriteByteTPI(TPI_PIX_REPETITION, tpivmode[0]);      		// Write register 0x08
	g_sys.tmdsPoweredUp = TRUE;
	EnableInterrupts(HOT_PLUG_EVENT | RX_SENSE_EVENT | AUDIO_ERROR_EVENT);

}

void OnHdmiCableDisconnected(void)
{
	TPI_DEBUG_PRINT(("Cable Disconnected\n"));
	g_sys.hdmiCableConnected = FALSE;
}

void OnHdmiCableConnected(void)
{
	TPI_DEBUG_PRINT(("Cable Connected\n"));
	g_sys.hdmiCableConnected = TRUE;
	TPI_DEBUG_PRINT(("Sink............\n"));
	ReadModifyWriteTPI(TPI_SYSTEM_CONTROL_DATA_REG, OUTPUT_MODE_MASK, OUTPUT_MODE_HDMI);
}

void siHdmiTx_TPI_Poll(void)
{
	byte InterruptStatus;
	if(g_sys.txPowerState == TX_POWER_STATE_D0)
	{
		InterruptStatus = ReadByteTPI(TPI_INTERRUPT_STATUS_REG);
		if(InterruptStatus & HOT_PLUG_EVENT)
		{
			TPI_DEBUG_PRINT(("HPD -> "));
			ReadSetWriteTPI(TPI_INTERRUPT_ENABLE_REG, HOT_PLUG_EVENT); // 使能HPD中断
			do
			{
				WriteByteTPI(TPI_INTERRUPT_STATUS_REG, HOT_PLUG_EVENT); // 
				DelayMS(T_HPD_DELAY);
				InterruptStatus = ReadByteTPI(TPI_INTERRUPT_STATUS_REG);
			}while(InterruptStatus & HOT_PLUG_EVENT);
			if(((InterruptStatus & HOT_PLUG_EVENT) >> 2) != g_sys.hdmiCableConnected)
			{
				if(g_sys.hdmiCableConnected == TRUE)
				{
					OnHdmiCableDisconnected();
				}
				else
				{
					OnHdmiCableConnected();
					ReadModifyWriteIndexedRegister(INDEXED_PAGE_0, 0x0A, 0x08, 0x08);
				}
				if(g_sys.hdmiCableConnected == FALSE)
				{
					return;
				}
			}
			siHdmiTx_TPI_Init();
			TPI_DEBUG_PRINT(("*** up from INT ***\n"));
		}
		DelayMS(200);
	}
}


static void work_queue(struct work_struct *work)
{
	siHdmiTx_TPI_Poll();
	schedule_delayed_work(sii9024_delayed_work, HZ/10);
}

static const struct i2c_device_id hdmi_sii_id[] = {
	{"sii9024",0},
	{"siiEDID",0},
	{"siiSegEDID",0},
	{"siiHDCP",0},
	{}
};

static int __init delay_init(void)
{
	struct i2c_adapter *adap;
	adap = i2c_get_adapter(0);
	sii9024 = i2c_new_device(adap,&sii_info);
	i2c_put_adapter(adap);

	sii9024_delayed_work = kmalloc(sizeof(*sii9024_delayed_work), GFP_ATOMIC);
	INIT_DELAYED_WORK(sii9024_delayed_work, work_queue);

	siHdmiTx_VideoSel(HDMI_1080P60);
	siHdmiTx_TPI_Init();
	siHdmiTx_Init();
	schedule_delayed_work(sii9024_delayed_work,5*HZ);
	return 0;
}
static void __exit delay_exit(void)
{
	kfree(sii9024_delayed_work);
	i2c_unregister_device(sii9024);
}


module_init(delay_init);
module_exit(delay_exit);
MODULE_LICENSE("GPL");
//MODULE_DEVICE_TABLE(i2c, hdmi_sii_id);
