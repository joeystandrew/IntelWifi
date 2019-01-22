//
//  IwlMvmOpMode_fw.hpp
//  IntelWifi
//
//  Created by Алексей Аносов on 1/22/19.
//  Copyright © 2019 Roman Peshkov. All rights reserved.
//

#ifndef IwlMvmOpMode_fw_hpp
#define IwlMvmOpMode_fw_hpp

#define MVM_UCODE_ALIVE_TIMEOUT    HZ
#define MVM_UCODE_CALIB_TIMEOUT    (2*HZ)

#define UCODE_VALID_OK    cpu_to_le32(0x1)

struct iwl_mvm_alive_data {
    bool valid;
    u32 scd_base_addr;
};

#endif /* IwlMvmOpMode_fw_hpp */
