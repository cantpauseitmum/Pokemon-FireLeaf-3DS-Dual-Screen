#include <3ds.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "librfu.h"

// -------------------------------------------------------------------------
// 3DS UDS (Unlicensed Data Services) Translation Layer for GBA Wireless Adapter
// -------------------------------------------------------------------------

static bool s_udsInitialized = false;
static udsNetworkStruct s_network;
static udsBindContext s_bindContext;

// Global structures exported by librfu that the game expects to read
struct STWIStatus *gSTWIStatus = NULL;
struct RfuLinkStatus *gRfuLinkStatus = NULL;
struct RfuStatic *gRfuStatic = NULL;
struct RfuFixed *gRfuFixed = NULL;
struct RfuSlotStatusNI *gRfuSlotStatusNI[RFU_CHILD_MAX] = {0};
struct RfuSlotStatusUNI *gRfuSlotStatusUNI[RFU_CHILD_MAX] = {0};

static struct STWIStatus s_stwiStatus;
static struct RfuLinkStatus s_rfuLinkStatus;
static struct RfuStatic s_rfuStatic;
static struct RfuFixed s_rfuFixed;
static struct RfuSlotStatusNI s_niStatus[RFU_CHILD_MAX];
static struct RfuSlotStatusUNI s_uniStatus[RFU_CHILD_MAX];

// -------------------------------------------------------------------------
// Initialization & Configuration
// -------------------------------------------------------------------------

u16 rfu_initializeAPI(u32 *APIBuffer, u16 buffByteSize, void *sioIntrTable_p, bool8 copyInterruptToRam) {
    gSTWIStatus = &s_stwiStatus;
    gRfuLinkStatus = &s_rfuLinkStatus;
    gRfuStatic = &s_rfuStatic;
    gRfuFixed = &s_rfuFixed;
    
    for(int i=0; i<RFU_CHILD_MAX; i++) {
        gRfuSlotStatusNI[i] = &s_niStatus[i];
        gRfuSlotStatusUNI[i] = &s_uniStatus[i];
    }
    
    if (!s_udsInitialized) {
        // Initialize 3DS UDS for local multiplayer (0x3000 bytes for shared memory)
        if (R_SUCCEEDED(udsInit(0x3000, NULL))) {
            s_udsInitialized = true;
        }
    }
    return 0;
}

u32 rfu_REQBN_softReset_and_checkID(void) {
    // Return the hardware ID expected by the engine for the wireless adapter
    return RFU_ID; 
}

void rfu_REQ_reset(void) { }

void rfu_REQ_stopMode(void) {
    if (s_udsInitialized) {
        udsExit();
        s_udsInitialized = false;
    }
}

void rfu_REQ_configSystem(u16 availSlotFlag, u8 maxMFrame, u8 mcTimer) { }

void rfu_REQ_configGameData(u8 mbootFlag, u16 serialNo, const u8 *gname, const u8 *uname) {
    // In a full implementation, we'd copy the gname/uname to the UDS beacon payload here
}

void rfu_setREQCallback(void (*callback)(u16 reqCommandId, u16 reqResult)) {
    if (gRfuFixed) gRfuFixed->reqCallback = (void(*)(u16,u16))callback;
}

u16 rfu_waitREQComplete(void) { return 0; }
u16 rfu_syncVBlank(void) { return 0; }

// -------------------------------------------------------------------------
// Host (Parent) - Creating Union Room
// -------------------------------------------------------------------------

void rfu_REQ_startSearchChild(void) {
    // Hook: udsCreateNetwork()
    // Game is trying to host a union room.
}

void rfu_REQ_pollSearchChild(void) {
    // Game checks if clients connected. We'd poll UDS nodes here.
}

void rfu_REQ_endSearchChild(void) { }

// -------------------------------------------------------------------------
// Client (Child) - Searching and Connecting
// -------------------------------------------------------------------------

void rfu_REQ_startSearchParent(void) {
    // Hook: udsScanBeacons()
    // Game is searching for available union rooms.
}

void rfu_REQ_pollSearchParent(void) {
    // We would parse the discovered UDS beacons and populate gRfuLinkStatus->partner[]
}

void rfu_REQ_endSearchParent(void) { }

void rfu_REQ_startConnectParent(u16 pid) {
    // Hook: udsConnectNetwork()
}

void rfu_REQ_pollConnectParent(void) { }

void rfu_REQ_endConnectParent(void) { }

u16 rfu_getConnectParentStatus(u8 *status, u8 *connectSlotNo) {
    *status = 0; // CP_STATUS_DONE
    *connectSlotNo = 0;
    return 0;
}

// -------------------------------------------------------------------------
// Data Transmission (Packets mapping to UDS Data Frames)
// -------------------------------------------------------------------------

u16 rfu_UNI_setSendData(u8 bmSendSlot, const void *src, u8 size) { return 0; }
void rfu_UNI_readySendData(u8 slotStatusIndex) {}
u16 rfu_UNI_changeAndReadySendData(u8 slotStatusIndex, const void *src, u8 size) { return 0; }
u16 rfu_UNI_PARENT_getDRAC_ACK(u8 *ackFlag) { return 0; }
void rfu_UNI_clearRecvNewDataFlag(u8 slotStatusIndex) {}

u16 rfu_NI_setSendData(u8 bmSendSlot, u8 subFrameSize, const void *src, u32 size) { return 0; }
u16 rfu_NI_CHILD_setSendGameName(u8 slotNo, u8 subFrameSize) { return 0; }
u16 rfu_NI_stopReceivingData(u8 slotStatusIndex) { return 0; }
u16 rfu_changeSendTarget(u8 connType, u8 slotStatusIndex, u8 bmNewTgtSlot) { return 0; }

void rfu_REQ_sendData(bool8 clockChangeFlag) {
    // Hook: udsSendTo()
}
void rfu_REQ_PARENT_resumeRetransmitAndChange(void) {}

void rfu_REQ_recvData(void) {
    // Hook: udsPullPacket()
    // Retrieve packet from UDS buffer and load it into gRfuSlotStatusUNI or NI
}

// -------------------------------------------------------------------------
// Link Management
// -------------------------------------------------------------------------

u16 rfu_REQBN_watchLink(u16 reqCommandId, u8 *bmLinkLossSlot, u8 *linkLossReason, u8 *parentBmLinkRecoverySlot) {
    return 0;
}

void rfu_REQ_disconnect(u8 bmDisconnectSlot) {
    // Hook: udsDisconnectNetwork()
}

void rfu_REQ_changeMasterSlave(void) {}
bool8 rfu_getMasterSlave(void) { return 0; }

void rfu_setMSCCallback(void (*callback)(u16 reqCommandId)) {}
void rfu_clearAllSlot(void) {}
u16 rfu_clearSlot(u8 connTypeFlag, u8 slotStatusIndex) { return 0; }
u16 rfu_setRecvBuffer(u8 connType, u8 slotNo, void *buffer, u32 buffSize) { return 0; }

// -------------------------------------------------------------------------
// Stubs for STWI/INTR layer just in case game calls them directly
// -------------------------------------------------------------------------

void rfu_setTimerInterrupt(u8 timerNo, void *timerIntrTable_p) {}
u16 rfu_MBOOT_CHILD_inheritanceLinkStatus(void) { return 0; }
u8 *rfu_getSTWIRecvBuffer(void) { return NULL; }
void rfu_REQ_RFUStatus(void) {}
u16 rfu_getRFUStatus(u8 *rfuState) { return 0; }
void rfu_REQ_noise(void) {}

void IntrSIO32(void) {}
void STWI_init_all(void *interruptStruct, void *interrupt, bool8 copyInterruptToRam) {}
void STWI_set_MS_mode(u8 mode) {}
void STWI_init_Callback_M(void) {}
void STWI_init_Callback_S(void) {}
void STWI_set_Callback_M(void *callbackM) {}
void STWI_set_Callback_S(void (*callbackS)(u16)) {}
void STWI_init_timer(void *interrupt, s32 timerSelect) {}
void AgbRFU_SoftReset(void) {}
void STWI_set_Callback_ID(void (*func)(void)) {}
u16 STWI_read_status(u8 index) { return 0; }
u16 STWI_poll_CommandEnd(void) { return 0; }

void STWI_send_DataRxREQ(void) {}
void STWI_send_MS_ChangeREQ(void) {}
void STWI_send_StopModeREQ(void) {}
void STWI_send_SystemStatusREQ(void) {}
void STWI_send_GameConfigREQ(const u8 *serial_uname, const u8 *gname) {}
void STWI_send_ResetREQ(void) {}
void STWI_send_LinkStatusREQ(void) {}
void STWI_send_VersionStatusREQ(void) {}
void STWI_send_SlotStatusREQ(void) {}
void STWI_send_ConfigStatusREQ(void) {}
void STWI_send_ResumeRetransmitAndChangeREQ(void) {}
void STWI_send_SystemConfigREQ(u16 availSlotFlag, u8 maxMFrame, u8 mcTimer) {}
void STWI_send_SC_StartREQ(void) {}
void STWI_send_SC_PollingREQ(void) {}
void STWI_send_SC_EndREQ(void) {}
void STWI_send_SP_StartREQ(void) {}
void STWI_send_SP_PollingREQ(void) {}
void STWI_send_SP_EndREQ(void) {}
void STWI_send_CP_StartREQ(u16 unk1) {}
void STWI_send_CP_PollingREQ(void) {}
void STWI_send_CP_EndREQ(void) {}
void STWI_send_DataTxREQ(const void *in, u8 size) {}
void STWI_send_DataTxAndChangeREQ(const void *in, u8 size) {}
void STWI_send_DataReadyAndChangeREQ(u8 unk) {}
void STWI_send_DisconnectedAndChangeREQ(u8 unk0, u8 unk1) {}
void STWI_send_DisconnectREQ(u8 unk) {}
void STWI_send_TestModeREQ(u8 unk0, u8 unk1) {}
void STWI_send_CPR_StartREQ(u16 unk0, u16 unk1, u8 unk2) {}
void STWI_send_CPR_PollingREQ(void) {}
void STWI_send_CPR_EndREQ(void) {}
