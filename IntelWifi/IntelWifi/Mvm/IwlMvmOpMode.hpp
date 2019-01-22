//
//  IwlMvmOpMode.hpp
//  IntelWifi
//
//  Created by Anosova Natalia on 16/01/2019.
//  Copyright © 2019 Roman Peshkov. All rights reserved.
//

#ifndef IwlMvmOpMode_hpp
#define IwlMvmOpMode_hpp

#include <IOKit/IOLib.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <libkern/OSKextLib.h>
#include <libkern/locks.h>


extern "C" {
//    #include <linux/mac80211.h>
//    #include <net/cfg80211.h>
//    #include <linux/jiffies.h>
    
//    #include "iwl-debug.h"
//    #include "../iw_utils/allocation.h"
    #include "../iwlwifi/mvm/mvm.h"
    #include "../iwlwifi/iwl-prph.h"
    


}
#ifdef CONFIG_PM
#define IWL_MVM_D0I3_OPS                    \
.enter_d0i3 = iwl_mvm_enter_d0i3,            \
.exit_d0i3 = iwl_mvm_exit_d0i3,
#else /* CONFIG_PM */
#define IWL_MVM_D0I3_OPS
#endif /* CONFIG_PM */

#define IWL_MVM_COMMON_OPS                    \
/* these could be differentiated */            \
.async_cb = iwl_mvm_async_cb,                \
.queue_full = iwl_mvm_stop_sw_queue,            \
.queue_not_full = iwl_mvm_wake_sw_queue,        \
.hw_rf_kill = iwl_mvm_set_hw_rfkill_state,        \
.free_skb = iwl_mvm_free_skb,                \
.nic_error = iwl_mvm_nic_error,                \
.cmd_queue_full = iwl_mvm_cmd_queue_full,        \
.nic_config = iwl_mvm_nic_config,            \
IWL_MVM_D0I3_OPS                    \
/* as we only register one, these MUST be common! */    \
.start = iwl_op_mode_mvm_start,                \
.stop = iwl_op_mode_mvm_stop

#include <IOKit/IOBufferMemoryDescriptor.h>

#include "IwlOpModeOps.h"
#include "TransOps.h"


extern "C" {
    
}


#define MAC_FMT "%02x:%02x:%02x:%02x:%02x:%02x"
#define MAC_BYTES(x) (x)[0],(x)[1],(x)[2],(x)[3],(x)[4],(x)[5]


class IwlMvmOpMode : public IwlOpModeOps {
public:
    IwlMvmOpMode(TransOps *ops);
    
    struct ieee80211_hw *start(struct iwl_trans *trans, const struct iwl_cfg *cfg,
                               const struct iwl_fw *fw) override;
    void nic_config() override;
    
    
    
    void stop() override;
    void rx(struct napi_struct *napi, struct iwl_rx_cmd_buffer *rxb) override;
    
    
    IOReturn getCARD_CAPABILITIES(IO80211Interface *interface, struct apple80211_capability_data *cd) override;
    
    IOReturn getPHY_MODE(IO80211Interface *interface, struct apple80211_phymode_data *pd) override;
    
    IOReturn getPOWER(IO80211Interface *intf, apple80211_power_data *power_data) override;
    IOReturn setPOWER(IO80211Interface *intf, apple80211_power_data *power_data) override;
    
    
    
private:
    // ops.c
//    void iwl_down(struct iwl_mvm *priv); // line 916
    struct iwl_mvm *iwl_op_mode_mvm_start(struct iwl_trans *trans,
                                           const struct iwl_cfg *cfg,
                                           const struct iwl_fw *fw); // line 1232
    
    int iwl_run_unified_mvm_ucode(bool read_nvm);
    int iwl_mvm_load_ucode_wait_alive(enum iwl_ucode_type ucode_type);
    bool iwl_alive_fn(struct iwl_notif_wait_data *notif_wait,
                      struct iwl_rx_packet *pkt, void *data);
    
    void iwl_op_mode_mvm_stop(); // line 1524
    
    
    
    int iwl_mvm_load_rt_fw();
    int iwl_run_init_mvm_ucode(bool read_nvm);
    int iwl_send_tx_ant_cfg(u8 valid_tx_ant);
    
    int iwl_up();
    
    // mac80211.c
//    int __iwl_up(struct iwl_priv *priv); // line 238
//    int iwlagn_mac_start(struct iwl_mvm *priv); // line 296
//    void iwlagn_mac_stop(struct iwl_mvm *priv); // line 323
//    void iwlagn_mac_channel_switch(struct iwl_mvm *priv,
//                                   struct ieee80211_vif *vif,
//                                   struct ieee80211_channel_switch *ch_switch); // line 964
    
//    int iwl_setup_interface(struct iwl_mvm *priv, struct iwl_rxon_context *ctx); // line 1251
//    int iwlagn_mac_add_interface(struct iwl_mvm *priv, struct ieee80211_vif *vif); // line 1297
    
    // ucode.c
//    int iwl_load_ucode_wait_alive(enum iwl_ucode_type ucode_type);
//    int iwl_run_init_ucode(struct iwl_mvm *priv);
//    int iwl_alive_notify(struct iwl_mvm *priv);
//    void iwl_nic_config(struct iwl_mvm *priv);
//    static bool iwlagn_wait_calib(struct iwl_notif_wait_data *notif_wait,
//                                  struct iwl_rx_packet *pkt, void *data);
    
    
    
    TransOps *_ops;
    
    struct iwl_mvm *priv;
};



#endif /* IwlMvmOpMode_hpp */
