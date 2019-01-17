//
//  IwlMvmOpMode.hpp
//  IntelWifi
//
//  Created by Anosova Natalia on 16/01/2019.
//  Copyright © 2019 Roman Peshkov. All rights reserved.
//

#ifndef IwlMvmOpMode_hpp
#define IwlMvmOpMode_hpp

extern "C" {
#include <linux/mac80211.h>
#include <net/cfg80211.h>
#include <linux/jiffies.h>
    
#include "iwl-debug.h"
#include "../iw_utils/allocation.h"
    
}

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
    void nic_config(struct iwl_op_mode *op_mode) override;
    
    void stop(struct iwl_op_mode *op_mode) override;
    void rx(struct iwl_priv *priv, struct napi_struct *napi, struct iwl_rx_cmd_buffer *rxb) override;
    
    //    void add_interface(struct ieee80211_vif *vif) override;
    //    void channel_switch(struct iwl_priv *priv, struct ieee80211_vif *vif, struct ieee80211_channel_switch *chsw) override;
    
    IOReturn getCARD_CAPABILITIES(IO80211Interface *interface, struct apple80211_capability_data *cd) override;
    
    IOReturn getPHY_MODE(IO80211Interface *interface, struct apple80211_phymode_data *pd) override;
    
    IOReturn getPOWER(IO80211Interface *intf, apple80211_power_data *power_data) override;
    IOReturn setPOWER(IO80211Interface *intf, apple80211_power_data *power_data) override;
    
    
private:
    // ops.c
    void iwl_down(struct iwl_op_mode *priv); // line 916
    struct iwl_op_mode *iwl_op_mode_mvm_start(struct iwl_trans *trans,
                                           const struct iwl_cfg *cfg,
                                           const struct iwl_fw *fw); // line 1232
    void iwl_op_mode_mvm_stop(struct iwl_op_mode* op_mode); // line 1524
    
    // mac80211.c
//    int __iwl_up(struct iwl_priv *priv); // line 238
    int iwlagn_mac_start(struct iwl_op_mode *op_mode); // line 296
    void iwlagn_mac_stop(struct iwl_op_mode *op_mode); // line 323
    void iwlagn_mac_channel_switch(struct iwl_priv *priv,
                                   struct ieee80211_vif *vif,
                                   struct ieee80211_channel_switch *ch_switch); // line 964
    
    int iwl_setup_interface(struct iwl_priv *priv, struct iwl_rxon_context *ctx); // line 1251
    int iwlagn_mac_add_interface(struct iwl_priv *priv, struct ieee80211_vif *vif); // line 1297
    
    // ucode.c
    int iwl_load_ucode_wait_alive(struct iwl_priv *priv,
                                  enum iwl_ucode_type ucode_type);
    int iwl_run_init_ucode(struct iwl_priv *priv);
    int iwl_alive_notify(struct iwl_priv *priv);
    void iwl_nic_config(struct iwl_priv *priv);
    static bool iwlagn_wait_calib(struct iwl_notif_wait_data *notif_wait,
                                  struct iwl_rx_packet *pkt, void *data);
    
    
    
    TransOps *_ops;
    
    struct iwl_op_mode *op_mode;
};



#endif /* IwlMvmOpMode_hpp */
