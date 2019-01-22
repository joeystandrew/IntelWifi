//
//  IwlMvmOpMode_fw.cpp
//  IntelWifi
//
//  Created by Алексей Аносов on 1/22/19.
//  Copyright © 2019 Roman Peshkov. All rights reserved.
//


#include "IwlMvmOpMode.hpp"
#include "IwlMvmOpMode_fw.hpp"
#include "../porting/linux/err.h"
extern "C"{
    #include "iwl-io.h"
    #include "mvm.h"
}

static bool iwl_wait_init_complete(struct iwl_notif_wait_data *notif_wait,
                                   struct iwl_rx_packet *pkt, void *data)
{
    WARN_ON(pkt->hdr.cmd != INIT_COMPLETE_NOTIF);
    
    return true;
}

int IwlMvmOpMode::iwl_run_unified_mvm_ucode(bool read_nvm)
{
    struct iwl_mvm *mvm = this->priv;
    
    struct iwl_notification_wait init_wait;
    struct iwl_nvm_access_complete_cmd nvm_complete = {};
    struct iwl_init_extended_cfg_cmd init_cfg = {
        .init_flags = cpu_to_le32(BIT(IWL_INIT_NVM)),
    };
    static const u16 init_complete[] = {
        INIT_COMPLETE_NOTIF,
    };
    int ret;
    
    IOLockLock(mvm->mutex);
    
    iwl_init_notification_wait(&mvm->notif_wait,
                               &init_wait,
                               init_complete,
                               ARRAY_SIZE(init_complete),
                               iwl_wait_init_complete,
                               NULL);
    
    /* Will also start the device */
    ret = iwl_mvm_load_ucode_wait_alive(IWL_UCODE_REGULAR);
    if (ret) {
        IWL_ERR(mvm, "Failed to start RT ucode: %d\n", ret);
        goto error;
    }
    
    /* Send init config command to mark that we are sending NVM access
     * commands
     */
    ret = iwl_mvm_send_cmd_pdu(mvm, WIDE_ID(SYSTEM_GROUP,
                                            INIT_EXTENDED_CFG_CMD), 0,
                               sizeof(init_cfg), &init_cfg);
    if (ret) {
        IWL_ERR(mvm, "Failed to run init config command: %d\n",
                ret);
        goto error;
    }
    
    /* Load NVM to NIC if needed */
    if (mvm->nvm_file_name) {
        iwl_read_external_nvm(mvm->trans, mvm->nvm_file_name,
                              mvm->nvm_sections);
        iwl_mvm_load_nvm_to_nic(mvm);
    }
    
    if (IWL_MVM_PARSE_NVM && read_nvm) {
        ret = iwl_nvm_init(mvm);
        if (ret) {
            IWL_ERR(mvm, "Failed to read NVM: %d\n", ret);
            goto error;
        }
    }
    
    ret = iwl_mvm_send_cmd_pdu(mvm, WIDE_ID(REGULATORY_AND_NVM_GROUP,
                                            NVM_ACCESS_COMPLETE), 0,
                               sizeof(nvm_complete), &nvm_complete);
    if (ret) {
        IWL_ERR(mvm, "Failed to run complete NVM access: %d\n",
                ret);
        goto error;
    }
    
    /* We wait for the INIT complete notification */
    ret = iwl_wait_notification(&mvm->notif_wait, &init_wait,
                                MVM_UCODE_ALIVE_TIMEOUT);
    if (ret)
        return ret;
    
    /* Read the NVM only at driver load time, no need to do this twice */
    if (!IWL_MVM_PARSE_NVM && read_nvm) {
        mvm->nvm_data = iwl_get_nvm(mvm->trans, mvm->fw);
        if (IS_ERR(mvm->nvm_data)) {
            ret = PTR_ERR(mvm->nvm_data);
            mvm->nvm_data = NULL;
            IWL_ERR(mvm, "Failed to read NVM: %d\n", ret);
            return ret;
        }
    }
    
    return 0;
    
error:
    iwl_remove_notification(&mvm->notif_wait, &init_wait);
    return ret;
}

static int iwl_send_phy_cfg_cmd(struct iwl_mvm *mvm)
{
    struct iwl_phy_cfg_cmd phy_cfg_cmd;
    enum iwl_ucode_type ucode_type = mvm->fwrt.cur_fw_img;
    
    /* Set parameters */
    phy_cfg_cmd.phy_cfg = cpu_to_le32(iwl_mvm_get_phy_config(mvm));
    
    /* set flags extra PHY configuration flags from the device's cfg */
    phy_cfg_cmd.phy_cfg |= cpu_to_le32(mvm->cfg->extra_phy_cfg_flags);
    
    phy_cfg_cmd.calib_control.event_trigger =
    mvm->fw->default_calib[ucode_type].event_trigger;
    phy_cfg_cmd.calib_control.flow_trigger =
    mvm->fw->default_calib[ucode_type].flow_trigger;
    
    IWL_DEBUG_INFO(mvm, "Sending Phy CFG command: 0x%x\n",
                   phy_cfg_cmd.phy_cfg);
    
    return iwl_mvm_send_cmd_pdu(mvm, PHY_CONFIGURATION_CMD, 0,
                                sizeof(phy_cfg_cmd), &phy_cfg_cmd);
}

int IwlMvmOpMode::iwl_mvm_load_ucode_wait_alive(enum iwl_ucode_type ucode_type)
{
    struct iwl_mvm *mvm = this->priv;
    struct iwl_notification_wait alive_wait;
    struct iwl_mvm_alive_data alive_data;
    const struct fw_img *fw;
    int ret, i;
    enum iwl_ucode_type old_type = mvm->fwrt.cur_fw_img;
    static const u16 alive_cmd[] = { MVM_ALIVE };
    
    set_bit(IWL_FWRT_STATUS_WAIT_ALIVE, &mvm->fwrt.status);
    if (ucode_type == IWL_UCODE_REGULAR &&
        iwl_fw_dbg_conf_usniffer(mvm->fw, FW_DBG_START_FROM_ALIVE) &&
        !(fw_has_capa(&mvm->fw->ucode_capa,
                      IWL_UCODE_TLV_CAPA_USNIFFER_UNIFIED)))
        fw = iwl_get_ucode_image(mvm->fw, IWL_UCODE_REGULAR_USNIFFER);
    else
        fw = iwl_get_ucode_image(mvm->fw, ucode_type);
    if (WARN_ON(!fw))
        return -EINVAL;
    iwl_fw_set_current_image(&mvm->fwrt, ucode_type);
    clear_bit(IWL_MVM_STATUS_FIRMWARE_RUNNING, &mvm->status);
    
//    iwl_init_notification_wait(&mvm->notif_wait, &alive_wait,
//                               alive_cmd, ARRAY_SIZE(alive_cmd),
//                               iwl_alive_fn, &alive_data);
    
    ret = iwl_trans_start_fw(mvm->trans, fw, ucode_type == IWL_UCODE_INIT);
    if (ret) {
        iwl_fw_set_current_image(&mvm->fwrt, old_type);
        iwl_remove_notification(&mvm->notif_wait, &alive_wait);
        return ret;
    }
    
    /*
     * Some things may run in the background now, but we
     * just wait for the ALIVE notification here.
     */
    ret = iwl_wait_notification(&mvm->notif_wait, &alive_wait,
                                MVM_UCODE_ALIVE_TIMEOUT);
    if (ret) {
        struct iwl_trans *trans = mvm->trans;
        
        if (trans->cfg->device_family >= IWL_DEVICE_FAMILY_22000)
            IWL_ERR(mvm,
                    "SecBoot CPU1 Status: 0x%x, CPU2 Status: 0x%x\n",
                    iwl_read_prph(trans, UMAG_SB_CPU_1_STATUS),
                    iwl_read_prph(trans, UMAG_SB_CPU_2_STATUS));
        else if (trans->cfg->device_family >= IWL_DEVICE_FAMILY_8000)
            IWL_ERR(mvm,
                    "SecBoot CPU1 Status: 0x%x, CPU2 Status: 0x%x\n",
                    iwl_read_prph(trans, SB_CPU_1_STATUS),
                    iwl_read_prph(trans, SB_CPU_2_STATUS));
        iwl_fw_set_current_image(&mvm->fwrt, old_type);
        return ret;
    }
    
    if (!alive_data.valid) {
        IWL_ERR(mvm, "Loaded ucode is not valid!\n");
        iwl_fw_set_current_image(&mvm->fwrt, old_type);
        return -EIO;
    }
    
    iwl_trans_fw_alive(mvm->trans, alive_data.scd_base_addr);
    
    /*
     * Note: all the queues are enabled as part of the interface
     * initialization, but in firmware restart scenarios they
     * could be stopped, so wake them up. In firmware restart,
     * mac80211 will have the queues stopped as well until the
     * reconfiguration completes. During normal startup, they
     * will be empty.
     */
    
    memset(&mvm->queue_info, 0, sizeof(mvm->queue_info));
    /*
     * Set a 'fake' TID for the command queue, since we use the
     * hweight() of the tid_bitmap as a refcount now. Not that
     * we ever even consider the command queue as one we might
     * want to reuse, but be safe nevertheless.
     */
    mvm->queue_info[IWL_MVM_DQA_CMD_QUEUE].tid_bitmap =
    BIT(IWL_MAX_TID_COUNT + 2);
    
//    for (i = 0; i < IEEE80211_MAX_QUEUES; i++)
//        atomic_set(&mvm->mac80211_queue_stop_count[i], 0);
    
    set_bit(IWL_MVM_STATUS_FIRMWARE_RUNNING, &mvm->status);
    clear_bit(IWL_FWRT_STATUS_WAIT_ALIVE, &mvm->fwrt.status);
    
    return 0;
}


int IwlMvmOpMode::iwl_mvm_load_rt_fw()
{
    struct iwl_mvm *mvm = this->priv;
    int ret;
    
    if (iwl_mvm_has_unified_ucode(mvm))
        return iwl_run_unified_mvm_ucode(false);
    
    ret = iwl_run_init_mvm_ucode(false);
    
    if (ret) {
        IWL_ERR(mvm, "Failed to run INIT ucode: %d\n", ret);
        
        if (iwlmvm_mod_params.init_dbg)
            return 0;
        return ret;
    }
    
    /*
     * Stop and start the transport without entering low power
     * mode. This will save the state of other components on the
     * device that are triggered by the INIT firwmare (MFUART).
     */
    _iwl_trans_stop_device(mvm->trans, false);
    ret = _iwl_trans_start_hw(mvm->trans, false);
    if (ret)
        return ret;
    
    ret = iwl_mvm_load_ucode_wait_alive(IWL_UCODE_REGULAR);
    if (ret)
        return ret;
    
    return iwl_init_paging(&mvm->fwrt, mvm->fwrt.cur_fw_img);
}

bool IwlMvmOpMode::iwl_alive_fn(struct iwl_notif_wait_data *notif_wait,
                  struct iwl_rx_packet *pkt, void *data)
{
    struct iwl_mvm *mvm = this->priv;
    struct iwl_mvm_alive_data *alive_data = (struct iwl_mvm_alive_data *)data;
    struct mvm_alive_resp_v3 *palive3;
    struct mvm_alive_resp *palive;
    struct iwl_umac_alive *umac;
    struct iwl_lmac_alive *lmac1;
    struct iwl_lmac_alive *lmac2 = NULL;
    u16 status;
    u32 umac_error_event_table;
    
    if (iwl_rx_packet_payload_len(pkt) == sizeof(*palive)) {
        palive = (struct mvm_alive_resp *)pkt->data;
        umac = &palive->umac_data;
        lmac1 = &palive->lmac_data[0];
        lmac2 = &palive->lmac_data[1];
        status = le16_to_cpu(palive->status);
    } else {
        palive3 = (struct mvm_alive_resp_v3 *)pkt->data;
        umac = &palive3->umac_data;
        lmac1 = &palive3->lmac_data;
        status = le16_to_cpu(palive3->status);
    }
    
    mvm->error_event_table[0] = le32_to_cpu(lmac1->error_event_table_ptr);
    if (lmac2)
        mvm->error_event_table[1] =
        le32_to_cpu(lmac2->error_event_table_ptr);
    mvm->log_event_table = le32_to_cpu(lmac1->log_event_table_ptr);
    
    umac_error_event_table = le32_to_cpu(umac->error_info_addr);
    
    if (!umac_error_event_table) {
        mvm->support_umac_log = false;
    } else if (umac_error_event_table >=
               mvm->trans->cfg->min_umac_error_event_table) {
        mvm->support_umac_log = true;
        mvm->umac_error_event_table = umac_error_event_table;
    } else {
        IWL_ERR(mvm,
                "Not valid error log pointer 0x%08X for %s uCode\n",
                mvm->umac_error_event_table,
                (mvm->fwrt.cur_fw_img == IWL_UCODE_INIT) ?
                "Init" : "RT");
        mvm->support_umac_log = false;
    }
    
    alive_data->scd_base_addr = le32_to_cpu(lmac1->scd_base_ptr);
    alive_data->valid = status == IWL_ALIVE_STATUS_OK;
    
    IWL_DEBUG_FW(mvm,
                 "Alive ucode status 0x%04x revision 0x%01X 0x%01X\n",
                 status, lmac1->ver_type, lmac1->ver_subtype);
    
    if (lmac2)
        IWL_DEBUG_FW(mvm, "Alive ucode CDB\n");
    
    IWL_DEBUG_FW(mvm,
                 "UMAC version: Major - 0x%x, Minor - 0x%x\n",
                 le32_to_cpu(umac->umac_major),
                 le32_to_cpu(umac->umac_minor));
    
    return true;
}


int IwlMvmOpMode::iwl_run_init_mvm_ucode(bool read_nvm)
{
    struct iwl_mvm *mvm = this -> priv;
    struct iwl_notification_wait calib_wait;
    static const u16 init_complete[] = {
        INIT_COMPLETE_NOTIF,
        CALIB_RES_NOTIF_PHY_DB
    };
    int ret;
    
    if (iwl_mvm_has_unified_ucode(mvm))
        return iwl_run_unified_mvm_ucode(true);
    
    IOLockLock(mvm->mutex);
    
    if (WARN_ON_ONCE(mvm->calibrating))
        return 0;
    
//    iwl_init_notification_wait(&mvm->notif_wait,
//                               &calib_wait,
//                               init_complete,
//                               ARRAY_SIZE(init_complete),
//                               iwl_wait_phy_db_entry,
//                               mvm->phy_db);
    
    /* Will also start the device */
    ret = iwl_mvm_load_ucode_wait_alive(IWL_UCODE_INIT);
    if (ret) {
        IWL_ERR(mvm, "Failed to start INIT ucode: %d\n", ret);
        goto remove_notif;
    }
    
    if (mvm->cfg->device_family < IWL_DEVICE_FAMILY_8000) {
        ret = iwl_mvm_send_bt_init_conf(mvm);
        if (ret)
            goto remove_notif;
    }
    
    /* Read the NVM only at driver load time, no need to do this twice */
    if (read_nvm) {
        ret = iwl_nvm_init(mvm);
        if (ret) {
            IWL_ERR(mvm, "Failed to read NVM: %d\n", ret);
            goto remove_notif;
        }
    }
    
    /* In case we read the NVM from external file, load it to the NIC */
    if (mvm->nvm_file_name)
        iwl_mvm_load_nvm_to_nic(mvm);
    
    WARN_ON(iwl_nvm_check_version(mvm->nvm_data, mvm->trans));
    
    /*
     * abort after reading the nvm in case RF Kill is on, we will complete
     * the init seq later when RF kill will switch to off
     */
    if (iwl_mvm_is_radio_hw_killed(mvm)) {
        IWL_DEBUG_RF_KILL(mvm,
                          "jump over all phy activities due to RF kill\n");
        goto remove_notif;
    }
    
    mvm->calibrating = true;
    
    /* Send TX valid antennas before triggering calibrations */
    ret = iwl_send_tx_ant_cfg(iwl_mvm_get_valid_tx_ant(mvm));
    if (ret)
        goto remove_notif;
    
    ret = iwl_send_phy_cfg_cmd(mvm);
    if (ret) {
        IWL_ERR(mvm, "Failed to run INIT calibrations: %d\n",
                ret);
        goto remove_notif;
    }
    
    /*
     * Some things may run in the background now, but we
     * just wait for the calibration complete notification.
     */
    ret = iwl_wait_notification(&mvm->notif_wait, &calib_wait,
                                MVM_UCODE_CALIB_TIMEOUT);
    if (!ret)
        goto out;
    
    if (iwl_mvm_is_radio_hw_killed(mvm)) {
        IWL_DEBUG_RF_KILL(mvm, "RFKILL while calibrating.\n");
        ret = 0;
    } else {
        IWL_ERR(mvm, "Failed to run INIT calibrations: %d\n",
                ret);
    }
    
    goto out;
    
remove_notif:
    iwl_remove_notification(&mvm->notif_wait, &calib_wait);
out:
    mvm->calibrating = false;
    if (iwlmvm_mod_params.init_dbg && !mvm->nvm_data) {
        /* we want to debug INIT and we have no NVM - fake */
        
        mvm->nvm_data = (struct iwl_nvm_data *)iwh_zalloc(sizeof(struct iwl_nvm_data) +
                                sizeof(struct ieee80211_channel) +
                                sizeof(struct ieee80211_rate)
                                );
        if (!mvm->nvm_data)
            return -ENOMEM;
        mvm->nvm_data->bands[0].channels = mvm->nvm_data->channels;
        mvm->nvm_data->bands[0].n_channels = 1;
        mvm->nvm_data->bands[0].n_bitrates = 1;
        mvm->nvm_data->bands[0].bitrates =(struct ieee80211_rate *)
        (int *)mvm->nvm_data->channels + 1;
        mvm->nvm_data->bands[0].bitrates->hw_value = 10;
    }
    
    return ret;
}

int IwlMvmOpMode::iwl_send_tx_ant_cfg(u8 valid_tx_ant)
{
    struct iwl_mvm *mvm = this->priv;
    struct iwl_tx_ant_cfg_cmd tx_ant_cmd = {
        .valid = cpu_to_le32(valid_tx_ant),
    };
    
    //    IWL_DEBUG_FW(mvm, "select valid tx ant: %u\n", valid_tx_ant);
    return iwl_mvm_send_cmd_pdu(mvm, TX_ANT_CONFIGURATION_CMD, 0,
                                sizeof(tx_ant_cmd), &tx_ant_cmd);
}
