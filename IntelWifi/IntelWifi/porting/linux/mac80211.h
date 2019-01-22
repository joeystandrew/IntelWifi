//
//  mac80211.h
//  IntelWifi
//
//  Created by Roman Peshkov on 18/01/2018.
//  Copyright © 2018 Roman Peshkov. All rights reserved.
//

#ifndef mac80211_h
#define mac80211_h

#include <linux/kernel.h>
#include <linux/ieee80211.h>
#include <linux/netdevice.h>
#include <net/cfg80211.h>
#include "jiffies.h"

// line 135
#define IEEE80211_INVAL_HW_QUEUE    0xff


/* line 335
 * The maximum number of IPv4 addresses listed for ARP filtering. If the number
 * of addresses for an interface increase beyond this value, hardware ARP
 * filtering will be disabled.
 */
#define IEEE80211_BSS_ARP_ADDR_LIST_LEN 4


/** line 443
 * struct ieee80211_mu_group_data - STA's VHT MU-MIMO group data
 *
 * This structure describes the group id data of VHT MU-MIMO
 *
 * @membership: 64 bits array - a bit is set if station is member of the group
 * @position: 2 bits per group id indicating the position in the group
 */
struct ieee80211_mu_group_data {
    u8 membership[WLAN_MEMBERSHIP_LEN];
    u8 position[WLAN_USER_POSITION_LEN];
};


/** line 456
 * struct ieee80211_bss_conf - holds the BSS's changing parameters
 *
 * This structure keeps information about a BSS (and an association
 * to that BSS) that can change during the lifetime of the BSS.
 *
 * @assoc: association status
 * @ibss_joined: indicates whether this station is part of an IBSS
 *    or not
 * @ibss_creator: indicates if a new IBSS network is being created
 * @aid: association ID number, valid only when @assoc is true
 * @use_cts_prot: use CTS protection
 * @use_short_preamble: use 802.11b short preamble
 * @use_short_slot: use short slot time (only relevant for ERP)
 * @dtim_period: num of beacons before the next DTIM, for beaconing,
 *    valid in station mode only if after the driver was notified
 *    with the %BSS_CHANGED_BEACON_INFO flag, will be non-zero then.
 * @sync_tsf: last beacon's/probe response's TSF timestamp (could be old
 *    as it may have been received during scanning long ago). If the
 *    HW flag %IEEE80211_HW_TIMING_BEACON_ONLY is set, then this can
 *    only come from a beacon, but might not become valid until after
 *    association when a beacon is received (which is notified with the
 *    %BSS_CHANGED_DTIM flag.). See also sync_dtim_count important notice.
 * @sync_device_ts: the device timestamp corresponding to the sync_tsf,
 *    the driver/device can use this to calculate synchronisation
 *    (see @sync_tsf). See also sync_dtim_count important notice.
 * @sync_dtim_count: Only valid when %IEEE80211_HW_TIMING_BEACON_ONLY
 *    is requested, see @sync_tsf/@sync_device_ts.
 *    IMPORTANT: These three sync_* parameters would possibly be out of sync
 *    by the time the driver will use them. The synchronized view is currently
 *    guaranteed only in certain callbacks.
 * @beacon_int: beacon interval
 * @assoc_capability: capabilities taken from assoc resp
 * @basic_rates: bitmap of basic rates, each bit stands for an
 *    index into the rate table configured by the driver in
 *    the current band.
 * @beacon_rate: associated AP's beacon TX rate
 * @mcast_rate: per-band multicast rate index + 1 (0: disabled)
 * @bssid: The BSSID for this BSS
 * @enable_beacon: whether beaconing should be enabled or not
 * @chandef: Channel definition for this BSS -- the hardware might be
 *    configured a higher bandwidth than this BSS uses, for example.
 * @mu_group: VHT MU-MIMO group membership data
 * @ht_operation_mode: HT operation mode like in &struct ieee80211_ht_operation.
 *    This field is only valid when the channel is a wide HT/VHT channel.
 *    Note that with TDLS this can be the case (channel is HT, protection must
 *    be used from this field) even when the BSS association isn't using HT.
 * @cqm_rssi_thold: Connection quality monitor RSSI threshold, a zero value
 *    implies disabled. As with the cfg80211 callback, a change here should
 *    cause an event to be sent indicating where the current value is in
 *    relation to the newly configured threshold.
 * @cqm_rssi_low: Connection quality monitor RSSI lower threshold, a zero value
 *    implies disabled.  This is an alternative mechanism to the single
 *    threshold event and can't be enabled simultaneously with it.
 * @cqm_rssi_high: Connection quality monitor RSSI upper threshold.
 * @cqm_rssi_hyst: Connection quality monitor RSSI hysteresis
 * @arp_addr_list: List of IPv4 addresses for hardware ARP filtering. The
 *    may filter ARP queries targeted for other addresses than listed here.
 *    The driver must allow ARP queries targeted for all address listed here
 *    to pass through. An empty list implies no ARP queries need to pass.
 * @arp_addr_cnt: Number of addresses currently on the list. Note that this
 *    may be larger than %IEEE80211_BSS_ARP_ADDR_LIST_LEN (the arp_addr_list
 *    array size), it's up to the driver what to do in that case.
 * @qos: This is a QoS-enabled BSS.
 * @idle: This interface is idle. There's also a global idle flag in the
 *    hardware config which may be more appropriate depending on what
 *    your driver/device needs to do.
 * @ps: power-save mode (STA only). This flag is NOT affected by
 *    offchannel/dynamic_ps operations.
 * @ssid: The SSID of the current vif. Valid in AP and IBSS mode.
 * @ssid_len: Length of SSID given in @ssid.
 * @hidden_ssid: The SSID of the current vif is hidden. Only valid in AP-mode.
 * @txpower: TX power in dBm
 * @txpower_type: TX power adjustment used to control per packet Transmit
 *    Power Control (TPC) in lower driver for the current vif. In particular
 *    TPC is enabled if value passed in %txpower_type is
 *    NL80211_TX_POWER_LIMITED (allow using less than specified from
 *    userspace), whereas TPC is disabled if %txpower_type is set to
 *    NL80211_TX_POWER_FIXED (use value configured from userspace)
 * @p2p_noa_attr: P2P NoA attribute for P2P powersave
 * @allow_p2p_go_ps: indication for AP or P2P GO interface, whether it's allowed
 *    to use P2P PS mechanism or not. AP/P2P GO is not allowed to use P2P PS
 *    if it has associated clients without P2P PS support.
 * @max_idle_period: the time period during which the station can refrain from
 *    transmitting frames to its associated AP without being disassociated.
 *    In units of 1000 TUs. Zero value indicates that the AP did not include
 *    a (valid) BSS Max Idle Period Element.
 * @protected_keep_alive: if set, indicates that the station should send an RSN
 *    protected frame to the AP to reset the idle timer at the AP for the
 *    station.
 */
struct ieee80211_bss_conf {
    const u8 *bssid;
    /* association related data */
    bool assoc, ibss_joined;
    bool ibss_creator;
    u16 aid;
    /* erp related data */
    bool use_cts_prot;
    bool use_short_preamble;
    bool use_short_slot;
    bool enable_beacon;
    u8 dtim_period;
    u16 beacon_int;
    u16 assoc_capability;
    u64 sync_tsf;
    u32 sync_device_ts;
    u8 sync_dtim_count;
    u32 basic_rates;
    struct ieee80211_rate *beacon_rate;
    int mcast_rate[NUM_NL80211_BANDS];
    u16 ht_operation_mode;
    s32 cqm_rssi_thold;
    u32 cqm_rssi_hyst;
    s32 cqm_rssi_low;
    s32 cqm_rssi_high;
    struct cfg80211_chan_def chandef;
    struct ieee80211_mu_group_data mu_group;
    u32 arp_addr_list[IEEE80211_BSS_ARP_ADDR_LIST_LEN];
    int arp_addr_cnt;
    bool qos;
    bool idle;
    bool ps;
    u8 ssid[IEEE80211_MAX_SSID_LEN];
    size_t ssid_len;
    bool hidden_ssid;
    int txpower;
    enum nl80211_tx_power_setting txpower_type;
    struct ieee80211_p2p_noa_attr p2p_noa_attr;
    bool allow_p2p_go_ps;
    u16 max_idle_period;
    bool protected_keep_alive;
};

/* maximum number of rate table entries */
#define IEEE80211_TX_RATE_TABLE_SIZE    4



/**
 * struct ieee80211_vif - per-interface data
 *
 * Data in this structure is continually present for driver
 * use during the life of a virtual interface.
 *
 * @type: type of this virtual interface
 * @bss_conf: BSS configuration for this interface, either our own
 *    or the BSS we're associated to
 * @addr: address of this interface
 * @p2p: indicates whether this AP or STA interface is a p2p
 *    interface, i.e. a GO or p2p-sta respectively
 * @csa_active: marks whether a channel switch is going on. Internally it is
 *    write-protected by sdata_lock and local->mtx so holding either is fine
 *    for read access.
 * @mu_mimo_owner: indicates interface owns MU-MIMO capability
 * @driver_flags: flags/capabilities the driver has for this interface,
 *    these need to be set (or cleared) when the interface is added
 *    or, if supported by the driver, the interface type is changed
 *    at runtime, mac80211 will never touch this field
 * @hw_queue: hardware queue for each AC
 * @cab_queue: content-after-beacon (DTIM beacon really) queue, AP mode only
 * @chanctx_conf: The channel context this interface is assigned to, or %NULL
 *    when it is not assigned. This pointer is RCU-protected due to the TX
 *    path needing to access it; even though the netdev carrier will always
 *    be off when it is %NULL there can still be races and packets could be
 *    processed after it switches back to %NULL.
 * @debugfs_dir: debugfs dentry, can be used by drivers to create own per
 *    interface debug files. Note that it will be NULL for the virtual
 *    monitor interface (if that is requested.)
 * @probe_req_reg: probe requests should be reported to mac80211 for this
 *    interface.
 * @drv_priv: data area for driver use, will always be aligned to
 *    sizeof(void \*).
 * @txq: the multicast data TX queue (if driver uses the TXQ abstraction)
 */
struct ieee80211_vif {
    enum nl80211_iftype type;
    struct ieee80211_bss_conf bss_conf;
    u8 addr[ETH_ALEN] __aligned(2);
    bool p2p;
    bool csa_active;
    bool mu_mimo_owner;
    
    u8 cab_queue;
    u8 hw_queue[IEEE80211_NUM_ACS];
    
    struct ieee80211_txq *txq;
    
    struct ieee80211_chanctx_conf __rcu *chanctx_conf;
    
    u32 driver_flags;
    
#ifdef CONFIG_MAC80211_DEBUGFS
    struct dentry *debugfs_dir;
#endif
    
    unsigned int probe_req_reg;
    
    /* must be last */
    u8 drv_priv[0] __aligned(sizeof(void *));
};

/** line 1031
 * enum mac80211_rx_flags - receive flags
 *
 * These flags are used with the @flag member of &struct ieee80211_rx_status.
 * @RX_FLAG_MMIC_ERROR: Michael MIC error was reported on this frame.
 *    Use together with %RX_FLAG_MMIC_STRIPPED.
 * @RX_FLAG_DECRYPTED: This frame was decrypted in hardware.
 * @RX_FLAG_MMIC_STRIPPED: the Michael MIC is stripped off this frame,
 *    verification has been done by the hardware.
 * @RX_FLAG_IV_STRIPPED: The IV and ICV are stripped from this frame.
 *    If this flag is set, the stack cannot do any replay detection
 *    hence the driver or hardware will have to do that.
 * @RX_FLAG_PN_VALIDATED: Currently only valid for CCMP/GCMP frames, this
 *    flag indicates that the PN was verified for replay protection.
 *    Note that this flag is also currently only supported when a frame
 *    is also decrypted (ie. @RX_FLAG_DECRYPTED must be set)
 * @RX_FLAG_DUP_VALIDATED: The driver should set this flag if it did
 *    de-duplication by itself.
 * @RX_FLAG_FAILED_FCS_CRC: Set this flag if the FCS check failed on
 *    the frame.
 * @RX_FLAG_FAILED_PLCP_CRC: Set this flag if the PCLP check failed on
 *    the frame.
 * @RX_FLAG_MACTIME_START: The timestamp passed in the RX status (@mactime
 *    field) is valid and contains the time the first symbol of the MPDU
 *    was received. This is useful in monitor mode and for proper IBSS
 *    merging.
 * @RX_FLAG_MACTIME_END: The timestamp passed in the RX status (@mactime
 *    field) is valid and contains the time the last symbol of the MPDU
 *    (including FCS) was received.
 * @RX_FLAG_MACTIME_PLCP_START: The timestamp passed in the RX status (@mactime
 *    field) is valid and contains the time the SYNC preamble was received.
 * @RX_FLAG_NO_SIGNAL_VAL: The signal strength value is not present.
 *    Valid only for data frames (mainly A-MPDU)
 * @RX_FLAG_AMPDU_DETAILS: A-MPDU details are known, in particular the reference
 *    number (@ampdu_reference) must be populated and be a distinct number for
 *    each A-MPDU
 * @RX_FLAG_AMPDU_LAST_KNOWN: last subframe is known, should be set on all
 *    subframes of a single A-MPDU
 * @RX_FLAG_AMPDU_IS_LAST: this subframe is the last subframe of the A-MPDU
 * @RX_FLAG_AMPDU_DELIM_CRC_ERROR: A delimiter CRC error has been detected
 *    on this subframe
 * @RX_FLAG_AMPDU_DELIM_CRC_KNOWN: The delimiter CRC field is known (the CRC
 *    is stored in the @ampdu_delimiter_crc field)
 * @RX_FLAG_MIC_STRIPPED: The mic was stripped of this packet. Decryption was
 *    done by the hardware
 * @RX_FLAG_ONLY_MONITOR: Report frame only to monitor interfaces without
 *    processing it in any regular way.
 *    This is useful if drivers offload some frames but still want to report
 *    them for sniffing purposes.
 * @RX_FLAG_SKIP_MONITOR: Process and report frame to all interfaces except
 *    monitor interfaces.
 *    This is useful if drivers offload some frames but still want to report
 *    them for sniffing purposes.
 * @RX_FLAG_AMSDU_MORE: Some drivers may prefer to report separate A-MSDU
 *    subframes instead of a one huge frame for performance reasons.
 *    All, but the last MSDU from an A-MSDU should have this flag set. E.g.
 *    if an A-MSDU has 3 frames, the first 2 must have the flag set, while
 *    the 3rd (last) one must not have this flag set. The flag is used to
 *    deal with retransmission/duplication recovery properly since A-MSDU
 *    subframes share the same sequence number. Reported subframes can be
 *    either regular MSDU or singly A-MSDUs. Subframes must not be
 *    interleaved with other frames.
 * @RX_FLAG_RADIOTAP_VENDOR_DATA: This frame contains vendor-specific
 *    radiotap data in the skb->data (before the frame) as described by
 *    the &struct ieee80211_vendor_radiotap.
 * @RX_FLAG_ALLOW_SAME_PN: Allow the same PN as same packet before.
 *    This is used for AMSDU subframes which can have the same PN as
 *    the first subframe.
 * @RX_FLAG_ICV_STRIPPED: The ICV is stripped from this frame. CRC checking must
 *    be done in the hardware.
 */
enum mac80211_rx_flags {
    RX_FLAG_MMIC_ERROR        = BIT(0),
    RX_FLAG_DECRYPTED        = BIT(1),
    RX_FLAG_MACTIME_PLCP_START    = BIT(2),
    RX_FLAG_MMIC_STRIPPED        = BIT(3),
    RX_FLAG_IV_STRIPPED        = BIT(4),
    RX_FLAG_FAILED_FCS_CRC        = BIT(5),
    RX_FLAG_FAILED_PLCP_CRC     = BIT(6),
    RX_FLAG_MACTIME_START        = BIT(7),
    RX_FLAG_NO_SIGNAL_VAL        = BIT(8),
    RX_FLAG_AMPDU_DETAILS        = BIT(9),
    RX_FLAG_PN_VALIDATED        = BIT(10),
    RX_FLAG_DUP_VALIDATED        = BIT(11),
    RX_FLAG_AMPDU_LAST_KNOWN    = BIT(12),
    RX_FLAG_AMPDU_IS_LAST        = BIT(13),
    RX_FLAG_AMPDU_DELIM_CRC_ERROR    = BIT(14),
    RX_FLAG_AMPDU_DELIM_CRC_KNOWN    = BIT(15),
    RX_FLAG_MACTIME_END        = BIT(16),
    RX_FLAG_ONLY_MONITOR        = BIT(17),
    RX_FLAG_SKIP_MONITOR        = BIT(18),
    RX_FLAG_AMSDU_MORE        = BIT(19),
    RX_FLAG_RADIOTAP_VENDOR_DATA    = BIT(20),
    RX_FLAG_MIC_STRIPPED        = BIT(21),
    RX_FLAG_ALLOW_SAME_PN        = BIT(22),
    RX_FLAG_ICV_STRIPPED        = BIT(23),
};

/**
 * enum mac80211_rx_encoding_flags - MCS & bandwidth flags
 *
 * @RX_ENC_FLAG_SHORTPRE: Short preamble was used for this frame
 * @RX_ENC_FLAG_SHORT_GI: Short guard interval was used
 * @RX_ENC_FLAG_HT_GF: This frame was received in a HT-greenfield transmission,
 *    if the driver fills this value it should add
 *    %IEEE80211_RADIOTAP_MCS_HAVE_FMT
 *    to hw.radiotap_mcs_details to advertise that fact
 * @RX_ENC_FLAG_LDPC: LDPC was used
 * @RX_ENC_FLAG_STBC_MASK: STBC 2 bit bitmask. 1 - Nss=1, 2 - Nss=2, 3 - Nss=3
 * @RX_ENC_FLAG_BF: packet was beamformed
 */
enum mac80211_rx_encoding_flags {
    RX_ENC_FLAG_SHORTPRE        = BIT(0),
    RX_ENC_FLAG_SHORT_GI        = BIT(2),
    RX_ENC_FLAG_HT_GF        = BIT(3),
    RX_ENC_FLAG_STBC_MASK        = BIT(4) | BIT(5),
    RX_ENC_FLAG_LDPC        = BIT(6),
    RX_ENC_FLAG_BF            = BIT(7),
};

#define RX_ENC_FLAG_STBC_SHIFT        4

enum mac80211_rx_encoding {
    RX_ENC_LEGACY = 0,
    RX_ENC_HT,
    RX_ENC_VHT,
};


/** line 1159
 * struct ieee80211_rx_status - receive status
 *
 * The low-level driver should provide this information (the subset
 * supported by hardware) to the 802.11 code with each received
 * frame, in the skb's control buffer (cb).
 *
 * @mactime: value in microseconds of the 64-bit Time Synchronization Function
 *     (TSF) timer when the first data symbol (MPDU) arrived at the hardware.
 * @boottime_ns: CLOCK_BOOTTIME timestamp the frame was received at, this is
 *    needed only for beacons and probe responses that update the scan cache.
 * @device_timestamp: arbitrary timestamp for the device, mac80211 doesn't use
 *    it but can store it and pass it back to the driver for synchronisation
 * @band: the active band when this frame was received
 * @freq: frequency the radio was tuned to when receiving this frame, in MHz
 *    This field must be set for management frames, but isn't strictly needed
 *    for data (other) frames - for those it only affects radiotap reporting.
 * @signal: signal strength when receiving this frame, either in dBm, in dB or
 *    unspecified depending on the hardware capabilities flags
 *    @IEEE80211_HW_SIGNAL_*
 * @chains: bitmask of receive chains for which separate signal strength
 *    values were filled.
 * @chain_signal: per-chain signal strength, in dBm (unlike @signal, doesn't
 *    support dB or unspecified units)
 * @antenna: antenna used
 * @rate_idx: index of data rate into band's supported rates or MCS index if
 *    HT or VHT is used (%RX_FLAG_HT/%RX_FLAG_VHT)
 * @nss: number of streams (VHT and HE only)
 * @flag: %RX_FLAG_\*
 * @encoding: &enum mac80211_rx_encoding
 * @bw: &enum rate_info_bw
 * @enc_flags: uses bits from &enum mac80211_rx_encoding_flags
 * @rx_flags: internal RX flags for mac80211
 * @ampdu_reference: A-MPDU reference number, must be a different value for
 *    each A-MPDU but the same for each subframe within one A-MPDU
 * @ampdu_delimiter_crc: A-MPDU delimiter CRC
 */
struct ieee80211_rx_status {
    u64 mactime;
    u64 boottime_ns;
    u32 device_timestamp;
    u32 ampdu_reference;
    u32 flag;
    u16 freq;
    u8 enc_flags;
    u8 encoding:2, bw:3;
    u8 rate_idx;
    u8 nss;
    u8 rx_flags;
    u8 band;
    u8 antenna;
    s8 signal;
    u8 chains;
    s8 chain_signal[IEEE80211_MAX_CHAINS];
    u8 ampdu_delimiter_crc;
};


/** line 1249
 * enum ieee80211_conf_flags - configuration flags
 *
 * Flags to define PHY configuration options
 *
 * @IEEE80211_CONF_MONITOR: there's a monitor interface present -- use this
 *    to determine for example whether to calculate timestamps for packets
 *    or not, do not use instead of filter flags!
 * @IEEE80211_CONF_PS: Enable 802.11 power save mode (managed mode only).
 *    This is the power save mode defined by IEEE 802.11-2007 section 11.2,
 *    meaning that the hardware still wakes up for beacons, is able to
 *    transmit frames and receive the possible acknowledgment frames.
 *    Not to be confused with hardware specific wakeup/sleep states,
 *    driver is responsible for that. See the section "Powersave support"
 *    for more.
 * @IEEE80211_CONF_IDLE: The device is running, but idle; if the flag is set
 *    the driver should be prepared to handle configuration requests but
 *    may turn the device off as much as possible. Typically, this flag will
 *    be set when an interface is set UP but not associated or scanning, but
 *    it can also be unset in that case when monitor interfaces are active.
 * @IEEE80211_CONF_OFFCHANNEL: The device is currently not on its main
 *    operating channel.
 */
enum ieee80211_conf_flags {
    IEEE80211_CONF_MONITOR        = (1<<0),
    IEEE80211_CONF_PS        = (1<<1),
    IEEE80211_CONF_IDLE        = (1<<2),
    IEEE80211_CONF_OFFCHANNEL    = (1<<3),
};


/** line 1305
 * enum ieee80211_smps_mode - spatial multiplexing power save mode
 *
 * @IEEE80211_SMPS_AUTOMATIC: automatic
 * @IEEE80211_SMPS_OFF: off
 * @IEEE80211_SMPS_STATIC: static
 * @IEEE80211_SMPS_DYNAMIC: dynamic
 * @IEEE80211_SMPS_NUM_MODES: internal, don't use
 */
enum ieee80211_smps_mode {
    IEEE80211_SMPS_AUTOMATIC,
    IEEE80211_SMPS_OFF,
    IEEE80211_SMPS_STATIC,
    IEEE80211_SMPS_DYNAMIC,
    
    /* keep last */
    IEEE80211_SMPS_NUM_MODES,
};


/** line 1324
 * struct ieee80211_conf - configuration of the device
 *
 * This struct indicates how the driver shall configure the hardware.
 *
 * @flags: configuration flags defined above
 *
 * @listen_interval: listen interval in units of beacon interval
 * @ps_dtim_period: The DTIM period of the AP we're connected to, for use
 *    in power saving. Power saving will not be enabled until a beacon
 *    has been received and the DTIM period is known.
 * @dynamic_ps_timeout: The dynamic powersave timeout (in ms), see the
 *    powersave documentation below. This variable is valid only when
 *    the CONF_PS flag is set.
 *
 * @power_level: requested transmit power (in dBm), backward compatibility
 *    value only that is set to the minimum of all interfaces
 *
 * @chandef: the channel definition to tune to
 * @radar_enabled: whether radar detection is enabled
 *
 * @long_frame_max_tx_count: Maximum number of transmissions for a "long" frame
 *    (a frame not RTS protected), called "dot11LongRetryLimit" in 802.11,
 *    but actually means the number of transmissions not the number of retries
 * @short_frame_max_tx_count: Maximum number of transmissions for a "short"
 *    frame, called "dot11ShortRetryLimit" in 802.11, but actually means the
 *    number of transmissions not the number of retries
 *
 * @smps_mode: spatial multiplexing powersave mode; note that
 *    %IEEE80211_SMPS_STATIC is used when the device is not
 *    configured for an HT channel.
 *    Note that this is only valid if channel contexts are not used,
 *    otherwise each channel context has the number of chains listed.
 */
struct ieee80211_conf {
    u32 flags;
    int power_level, dynamic_ps_timeout;
    
    u16 listen_interval;
    u8 ps_dtim_period;
    
    u8 long_frame_max_tx_count, short_frame_max_tx_count;
    
    struct cfg80211_chan_def chandef;
    bool radar_enabled;
    enum ieee80211_smps_mode smps_mode;
};

/** line 1372
 * struct ieee80211_channel_switch - holds the channel switch data
 *
 * The information provided in this structure is required for channel switch
 * operation.
 *
 * @timestamp: value in microseconds of the 64-bit Time Synchronization
 *    Function (TSF) timer when the frame containing the channel switch
 *    announcement was received. This is simply the rx.mactime parameter
 *    the driver passed into mac80211.
 * @device_timestamp: arbitrary timestamp for the device, this is the
 *    rx.device_timestamp parameter the driver passed to mac80211.
 * @block_tx: Indicates whether transmission must be blocked before the
 *    scheduled channel switch, as indicated by the AP.
 * @chandef: the new channel to switch to
 * @count: the number of TBTT's until the channel switch event
 */
struct ieee80211_channel_switch {
    u64 timestamp;
    u32 device_timestamp;
    bool block_tx;
    struct cfg80211_chan_def chandef;
    u8 count;
};

/** line 1518
 * enum ieee80211_key_flags - key flags
 *
 * These flags are used for communication about keys between the driver
 * and mac80211, with the @flags parameter of &struct ieee80211_key_conf.
 *
 * @IEEE80211_KEY_FLAG_GENERATE_IV: This flag should be set by the
 *    driver to indicate that it requires IV generation for this
 *    particular key. Setting this flag does not necessarily mean that SKBs
 *    will have sufficient tailroom for ICV or MIC.
 * @IEEE80211_KEY_FLAG_GENERATE_MMIC: This flag should be set by
 *    the driver for a TKIP key if it requires Michael MIC
 *    generation in software.
 * @IEEE80211_KEY_FLAG_PAIRWISE: Set by mac80211, this flag indicates
 *    that the key is pairwise rather then a shared key.
 * @IEEE80211_KEY_FLAG_SW_MGMT_TX: This flag should be set by the driver for a
 *    CCMP/GCMP key if it requires CCMP/GCMP encryption of management frames
 *    (MFP) to be done in software.
 * @IEEE80211_KEY_FLAG_PUT_IV_SPACE: This flag should be set by the driver
 *    if space should be prepared for the IV, but the IV
 *    itself should not be generated. Do not set together with
 *    @IEEE80211_KEY_FLAG_GENERATE_IV on the same key. Setting this flag does
 *    not necessarily mean that SKBs will have sufficient tailroom for ICV or
 *    MIC.
 * @IEEE80211_KEY_FLAG_RX_MGMT: This key will be used to decrypt received
 *    management frames. The flag can help drivers that have a hardware
 *    crypto implementation that doesn't deal with management frames
 *    properly by allowing them to not upload the keys to hardware and
 *    fall back to software crypto. Note that this flag deals only with
 *    RX, if your crypto engine can't deal with TX you can also set the
 *    %IEEE80211_KEY_FLAG_SW_MGMT_TX flag to encrypt such frames in SW.
 * @IEEE80211_KEY_FLAG_GENERATE_IV_MGMT: This flag should be set by the
 *    driver for a CCMP/GCMP key to indicate that is requires IV generation
 *    only for managment frames (MFP).
 * @IEEE80211_KEY_FLAG_RESERVE_TAILROOM: This flag should be set by the
 *    driver for a key to indicate that sufficient tailroom must always
 *    be reserved for ICV or MIC, even when HW encryption is enabled.
 */
enum ieee80211_key_flags {
    IEEE80211_KEY_FLAG_GENERATE_IV_MGMT    = BIT(0),
    IEEE80211_KEY_FLAG_GENERATE_IV        = BIT(1),
    IEEE80211_KEY_FLAG_GENERATE_MMIC    = BIT(2),
    IEEE80211_KEY_FLAG_PAIRWISE        = BIT(3),
    IEEE80211_KEY_FLAG_SW_MGMT_TX        = BIT(4),
    IEEE80211_KEY_FLAG_PUT_IV_SPACE        = BIT(5),
    IEEE80211_KEY_FLAG_RX_MGMT        = BIT(6),
    IEEE80211_KEY_FLAG_RESERVE_TAILROOM    = BIT(7),
};

// line 1602
#define IEEE80211_MAX_PN_LEN    16

#define TKIP_PN_TO_IV16(pn) ((u16)(pn & 0xffff))
#define TKIP_PN_TO_IV32(pn) ((u32)((pn >> 16) & 0xffffffff))

/**
 * struct ieee80211_key_seq - key sequence counter
 *
 * @tkip: TKIP data, containing IV32 and IV16 in host byte order
 * @ccmp: PN data, most significant byte first (big endian,
 *    reverse order than in packet)
 * @aes_cmac: PN data, most significant byte first (big endian,
 *    reverse order than in packet)
 * @aes_gmac: PN data, most significant byte first (big endian,
 *    reverse order than in packet)
 * @gcmp: PN data, most significant byte first (big endian,
 *    reverse order than in packet)
 * @hw: data for HW-only (e.g. cipher scheme) keys
 */
struct ieee80211_key_seq {
    union {
        struct {
            u32 iv32;
            u16 iv16;
        } tkip;
        struct {
            u8 pn[6];
        } ccmp;
        struct {
            u8 pn[6];
        } aes_cmac;
        struct {
            u8 pn[6];
        } aes_gmac;
        struct {
            u8 pn[6];
        } gcmp;
        struct {
            u8 seq[IEEE80211_MAX_PN_LEN];
            u8 seq_len;
        } hw;
    };
};

/**
 * enum ieee80211_sta_state - station state
 *
 * @IEEE80211_STA_NOTEXIST: station doesn't exist at all,
 *    this is a special state for add/remove transitions
 * @IEEE80211_STA_NONE: station exists without special state
 * @IEEE80211_STA_AUTH: station is authenticated
 * @IEEE80211_STA_ASSOC: station is associated
 * @IEEE80211_STA_AUTHORIZED: station is authorized (802.1X)
 */
enum ieee80211_sta_state {
    /* NOTE: These need to be ordered correctly! */
    IEEE80211_STA_NOTEXIST,
    IEEE80211_STA_NONE,
    IEEE80211_STA_AUTH,
    IEEE80211_STA_ASSOC,
    IEEE80211_STA_AUTHORIZED,
};

/**
 * enum ieee80211_sta_rx_bandwidth - station RX bandwidth
 * @IEEE80211_STA_RX_BW_20: station can only receive 20 MHz
 * @IEEE80211_STA_RX_BW_40: station can receive up to 40 MHz
 * @IEEE80211_STA_RX_BW_80: station can receive up to 80 MHz
 * @IEEE80211_STA_RX_BW_160: station can receive up to 160 MHz
 *    (including 80+80 MHz)
 *
 * Implementation note: 20 must be zero to be initialized
 *    correctly, the values must be sorted.
 */
enum ieee80211_sta_rx_bandwidth {
    IEEE80211_STA_RX_BW_20 = 0,
    IEEE80211_STA_RX_BW_40,
    IEEE80211_STA_RX_BW_80,
    IEEE80211_STA_RX_BW_160,
};

/**
 * struct ieee80211_sta_rates - station rate selection table
 *
 * @rcu_head: RCU head used for freeing the table on update
 * @rate: transmit rates/flags to be used by default.
 *    Overriding entries per-packet is possible by using cb tx control.
 */
struct ieee80211_sta_rates {
    //struct rcu_head rcu_head;
    struct {
        s8 idx;
        u8 count;
        u8 count_cts;
        u8 count_rts;
        u16 flags;
    } rate[IEEE80211_TX_RATE_TABLE_SIZE];
};


/**
 * struct ieee80211_sta - station table entry
 *
 * A station table entry represents a station we are possibly
 * communicating with. Since stations are RCU-managed in
 * mac80211, any ieee80211_sta pointer you get access to must
 * either be protected by rcu_read_lock() explicitly or implicitly,
 * or you must take good care to not use such a pointer after a
 * call to your sta_remove callback that removed it.
 *
 * @addr: MAC address
 * @aid: AID we assigned to the station if we're an AP
 * @supp_rates: Bitmap of supported rates (per band)
 * @ht_cap: HT capabilities of this STA; restricted to our own capabilities
 * @vht_cap: VHT capabilities of this STA; restricted to our own capabilities
 * @max_rx_aggregation_subframes: maximal amount of frames in a single AMPDU
 *    that this station is allowed to transmit to us.
 *    Can be modified by driver.
 * @wme: indicates whether the STA supports QoS/WME (if local devices does,
 *    otherwise always false)
 * @drv_priv: data area for driver use, will always be aligned to
 *    sizeof(void \*), size is determined in hw information.
 * @uapsd_queues: bitmap of queues configured for uapsd. Only valid
 *    if wme is supported. The bits order is like in
 *    IEEE80211_WMM_IE_STA_QOSINFO_AC_*.
 * @max_sp: max Service Period. Only valid if wme is supported.
 * @bandwidth: current bandwidth the station can receive with
 * @rx_nss: in HT/VHT, the maximum number of spatial streams the
 *    station can receive at the moment, changed by operating mode
 *    notifications and capabilities. The value is only valid after
 *    the station moves to associated state.
 * @smps_mode: current SMPS mode (off, static or dynamic)
 * @rates: rate control selection table
 * @tdls: indicates whether the STA is a TDLS peer
 * @tdls_initiator: indicates the STA is an initiator of the TDLS link. Only
 *    valid if the STA is a TDLS peer in the first place.
 * @mfp: indicates whether the STA uses management frame protection or not.
 * @max_amsdu_subframes: indicates the maximal number of MSDUs in a single
 *    A-MSDU. Taken from the Extended Capabilities element. 0 means
 *    unlimited.
 * @support_p2p_ps: indicates whether the STA supports P2P PS mechanism or not.
 * @max_rc_amsdu_len: Maximum A-MSDU size in bytes recommended by rate control.
 * @txq: per-TID data TX queues (if driver uses the TXQ abstraction)
 */
struct ieee80211_sta {
    u32 supp_rates[NUM_NL80211_BANDS];
    u8 addr[ETH_ALEN];
    u16 aid;
    struct ieee80211_sta_ht_cap ht_cap;
    struct ieee80211_sta_vht_cap vht_cap;
    u8 max_rx_aggregation_subframes;
    bool wme;
    u8 uapsd_queues;
    u8 max_sp;
    u8 rx_nss;
    enum ieee80211_sta_rx_bandwidth bandwidth;
    enum ieee80211_smps_mode smps_mode;
    struct ieee80211_sta_rates __rcu *rates;
    bool tdls;
    bool tdls_initiator;
    bool mfp;
    u8 max_amsdu_subframes;
    
    /**
     * @max_amsdu_len:
     * indicates the maximal length of an A-MSDU in bytes.
     * This field is always valid for packets with a VHT preamble.
     * For packets with a HT preamble, additional limits apply:
     *
     * * If the skb is transmitted as part of a BA agreement, the
     *   A-MSDU maximal size is min(max_amsdu_len, 4065) bytes.
     * * If the skb is not part of a BA aggreement, the A-MSDU maximal
     *   size is min(max_amsdu_len, 7935) bytes.
     *
     * Both additional HT limits must be enforced by the low level
     * driver. This is defined by the spec (IEEE 802.11-2012 section
     * 8.3.2.2 NOTE 2).
     */
    u16 max_amsdu_len;
    bool support_p2p_ps;
    u16 max_rc_amsdu_len;
    
    struct ieee80211_txq *txq[IEEE80211_NUM_TIDS];
    
    /* must be last */
    u8 drv_priv[0] __aligned(sizeof(void *));
};

/** line 1567
 * struct ieee80211_key_conf - key information
 *
 * This key information is given by mac80211 to the driver by
 * the set_key() callback in &struct ieee80211_ops.
 *
 * @hw_key_idx: To be set by the driver, this is the key index the driver
 *    wants to be given when a frame is transmitted and needs to be
 *    encrypted in hardware.
 * @cipher: The key's cipher suite selector.
 * @tx_pn: PN used for TX keys, may be used by the driver as well if it
 *    needs to do software PN assignment by itself (e.g. due to TSO)
 * @flags: key flags, see &enum ieee80211_key_flags.
 * @keyidx: the key index (0-3)
 * @keylen: key material length
 * @key: key material. For ALG_TKIP the key is encoded as a 256-bit (32 byte)
 *     data block:
 *     - Temporal Encryption Key (128 bits)
 *     - Temporal Authenticator Tx MIC Key (64 bits)
 *     - Temporal Authenticator Rx MIC Key (64 bits)
 * @icv_len: The ICV length for this key type
 * @iv_len: The IV length for this key type
 */
struct ieee80211_key_conf {
    u64 tx_pn;
    u32 cipher;
    u8 icv_len;
    u8 iv_len;
    u8 hw_key_idx;
    u8 flags;
    s8 keyidx;
    u8 keylen;
    u8 key[0];
};





/** line 1877
 * enum ieee80211_hw_flags - hardware flags
 *
 * These flags are used to indicate hardware capabilities to
 * the stack. Generally, flags here should have their meaning
 * done in a way that the simplest hardware doesn't need setting
 * any particular flags. There are some exceptions to this rule,
 * however, so you are advised to review these flags carefully.
 *
 * @IEEE80211_HW_HAS_RATE_CONTROL:
 *    The hardware or firmware includes rate control, and cannot be
 *    controlled by the stack. As such, no rate control algorithm
 *    should be instantiated, and the TX rate reported to userspace
 *    will be taken from the TX status instead of the rate control
 *    algorithm.
 *    Note that this requires that the driver implement a number of
 *    callbacks so it has the correct information, it needs to have
 *    the @set_rts_threshold callback and must look at the BSS config
 *    @use_cts_prot for G/N protection, @use_short_slot for slot
 *    timing in 2.4 GHz and @use_short_preamble for preambles for
 *    CCK frames.
 *
 * @IEEE80211_HW_RX_INCLUDES_FCS:
 *    Indicates that received frames passed to the stack include
 *    the FCS at the end.
 *
 * @IEEE80211_HW_HOST_BROADCAST_PS_BUFFERING:
 *    Some wireless LAN chipsets buffer broadcast/multicast frames
 *    for power saving stations in the hardware/firmware and others
 *    rely on the host system for such buffering. This option is used
 *    to configure the IEEE 802.11 upper layer to buffer broadcast and
 *    multicast frames when there are power saving stations so that
 *    the driver can fetch them with ieee80211_get_buffered_bc().
 *
 * @IEEE80211_HW_SIGNAL_UNSPEC:
 *    Hardware can provide signal values but we don't know its units. We
 *    expect values between 0 and @max_signal.
 *    If possible please provide dB or dBm instead.
 *
 * @IEEE80211_HW_SIGNAL_DBM:
 *    Hardware gives signal values in dBm, decibel difference from
 *    one milliwatt. This is the preferred method since it is standardized
 *    between different devices. @max_signal does not need to be set.
 *
 * @IEEE80211_HW_SPECTRUM_MGMT:
 *     Hardware supports spectrum management defined in 802.11h
 *     Measurement, Channel Switch, Quieting, TPC
 *
 * @IEEE80211_HW_AMPDU_AGGREGATION:
 *    Hardware supports 11n A-MPDU aggregation.
 *
 * @IEEE80211_HW_SUPPORTS_PS:
 *    Hardware has power save support (i.e. can go to sleep).
 *
 * @IEEE80211_HW_PS_NULLFUNC_STACK:
 *    Hardware requires nullfunc frame handling in stack, implies
 *    stack support for dynamic PS.
 *
 * @IEEE80211_HW_SUPPORTS_DYNAMIC_PS:
 *    Hardware has support for dynamic PS.
 *
 * @IEEE80211_HW_MFP_CAPABLE:
 *    Hardware supports management frame protection (MFP, IEEE 802.11w).
 *
 * @IEEE80211_HW_REPORTS_TX_ACK_STATUS:
 *    Hardware can provide ack status reports of Tx frames to
 *    the stack.
 *
 * @IEEE80211_HW_CONNECTION_MONITOR:
 *    The hardware performs its own connection monitoring, including
 *    periodic keep-alives to the AP and probing the AP on beacon loss.
 *
 * @IEEE80211_HW_NEED_DTIM_BEFORE_ASSOC:
 *    This device needs to get data from beacon before association (i.e.
 *    dtim_period).
 *
 * @IEEE80211_HW_SUPPORTS_PER_STA_GTK: The device's crypto engine supports
 *    per-station GTKs as used by IBSS RSN or during fast transition. If
 *    the device doesn't support per-station GTKs, but can be asked not
 *    to decrypt group addressed frames, then IBSS RSN support is still
 *    possible but software crypto will be used. Advertise the wiphy flag
 *    only in that case.
 *
 * @IEEE80211_HW_AP_LINK_PS: When operating in AP mode the device
 *    autonomously manages the PS status of connected stations. When
 *    this flag is set mac80211 will not trigger PS mode for connected
 *    stations based on the PM bit of incoming frames.
 *    Use ieee80211_start_ps()/ieee8021_end_ps() to manually configure
 *    the PS mode of connected stations.
 *
 * @IEEE80211_HW_TX_AMPDU_SETUP_IN_HW: The device handles TX A-MPDU session
 *    setup strictly in HW. mac80211 should not attempt to do this in
 *    software.
 *
 * @IEEE80211_HW_WANT_MONITOR_VIF: The driver would like to be informed of
 *    a virtual monitor interface when monitor interfaces are the only
 *    active interfaces.
 *
 * @IEEE80211_HW_NO_AUTO_VIF: The driver would like for no wlanX to
 *    be created.  It is expected user-space will create vifs as
 *    desired (and thus have them named as desired).
 *
 * @IEEE80211_HW_SW_CRYPTO_CONTROL: The driver wants to control which of the
 *    crypto algorithms can be done in software - so don't automatically
 *    try to fall back to it if hardware crypto fails, but do so only if
 *    the driver returns 1. This also forces the driver to advertise its
 *    supported cipher suites.
 *
 * @IEEE80211_HW_SUPPORT_FAST_XMIT: The driver/hardware supports fast-xmit,
 *    this currently requires only the ability to calculate the duration
 *    for frames.
 *
 * @IEEE80211_HW_QUEUE_CONTROL: The driver wants to control per-interface
 *    queue mapping in order to use different queues (not just one per AC)
 *    for different virtual interfaces. See the doc section on HW queue
 *    control for more details.
 *
 * @IEEE80211_HW_SUPPORTS_RC_TABLE: The driver supports using a rate
 *    selection table provided by the rate control algorithm.
 *
 * @IEEE80211_HW_P2P_DEV_ADDR_FOR_INTF: Use the P2P Device address for any
 *    P2P Interface. This will be honoured even if more than one interface
 *    is supported.
 *
 * @IEEE80211_HW_TIMING_BEACON_ONLY: Use sync timing from beacon frames
 *    only, to allow getting TBTT of a DTIM beacon.
 *
 * @IEEE80211_HW_SUPPORTS_HT_CCK_RATES: Hardware supports mixing HT/CCK rates
 *    and can cope with CCK rates in an aggregation session (e.g. by not
 *    using aggregation for such frames.)
 *
 * @IEEE80211_HW_CHANCTX_STA_CSA: Support 802.11h based channel-switch (CSA)
 *    for a single active channel while using channel contexts. When support
 *    is not enabled the default action is to disconnect when getting the
 *    CSA frame.
 *
 * @IEEE80211_HW_SUPPORTS_CLONED_SKBS: The driver will never modify the payload
 *    or tailroom of TX skbs without copying them first.
 *
 * @IEEE80211_HW_SINGLE_SCAN_ON_ALL_BANDS: The HW supports scanning on all bands
 *    in one command, mac80211 doesn't have to run separate scans per band.
 *
 * @IEEE80211_HW_TDLS_WIDER_BW: The device/driver supports wider bandwidth
 *    than then BSS bandwidth for a TDLS link on the base channel.
 *
 * @IEEE80211_HW_SUPPORTS_AMSDU_IN_AMPDU: The driver supports receiving A-MSDUs
 *    within A-MPDU.
 *
 * @IEEE80211_HW_BEACON_TX_STATUS: The device/driver provides TX status
 *    for sent beacons.
 *
 * @IEEE80211_HW_NEEDS_UNIQUE_STA_ADDR: Hardware (or driver) requires that each
 *    station has a unique address, i.e. each station entry can be identified
 *    by just its MAC address; this prevents, for example, the same station
 *    from connecting to two virtual AP interfaces at the same time.
 *
 * @IEEE80211_HW_SUPPORTS_REORDERING_BUFFER: Hardware (or driver) manages the
 *    reordering buffer internally, guaranteeing mac80211 receives frames in
 *    order and does not need to manage its own reorder buffer or BA session
 *    timeout.
 *
 * @IEEE80211_HW_USES_RSS: The device uses RSS and thus requires parallel RX,
 *    which implies using per-CPU station statistics.
 *
 * @IEEE80211_HW_TX_AMSDU: Hardware (or driver) supports software aggregated
 *    A-MSDU frames. Requires software tx queueing and fast-xmit support.
 *    When not using minstrel/minstrel_ht rate control, the driver must
 *    limit the maximum A-MSDU size based on the current tx rate by setting
 *    max_rc_amsdu_len in struct ieee80211_sta.
 *
 * @IEEE80211_HW_TX_FRAG_LIST: Hardware (or driver) supports sending frag_list
 *    skbs, needed for zero-copy software A-MSDU.
 *
 * @IEEE80211_HW_REPORTS_LOW_ACK: The driver (or firmware) reports low ack event
 *    by ieee80211_report_low_ack() based on its own algorithm. For such
 *    drivers, mac80211 packet loss mechanism will not be triggered and driver
 *    is completely depending on firmware event for station kickout.
 *
 * @IEEE80211_HW_SUPPORTS_TX_FRAG: Hardware does fragmentation by itself.
 *    The stack will not do fragmentation.
 *    The callback for @set_frag_threshold should be set as well.
 *
 * @NUM_IEEE80211_HW_FLAGS: number of hardware flags, used for sizing arrays
 */
enum ieee80211_hw_flags {
    IEEE80211_HW_HAS_RATE_CONTROL,
    IEEE80211_HW_RX_INCLUDES_FCS,
    IEEE80211_HW_HOST_BROADCAST_PS_BUFFERING,
    IEEE80211_HW_SIGNAL_UNSPEC,
    IEEE80211_HW_SIGNAL_DBM,
    IEEE80211_HW_NEED_DTIM_BEFORE_ASSOC,
    IEEE80211_HW_SPECTRUM_MGMT,
    IEEE80211_HW_AMPDU_AGGREGATION,
    IEEE80211_HW_SUPPORTS_PS,
    IEEE80211_HW_PS_NULLFUNC_STACK,
    IEEE80211_HW_SUPPORTS_DYNAMIC_PS,
    IEEE80211_HW_MFP_CAPABLE,
    IEEE80211_HW_WANT_MONITOR_VIF,
    IEEE80211_HW_NO_AUTO_VIF,
    IEEE80211_HW_SW_CRYPTO_CONTROL,
    IEEE80211_HW_SUPPORT_FAST_XMIT,
    IEEE80211_HW_REPORTS_TX_ACK_STATUS,
    IEEE80211_HW_CONNECTION_MONITOR,
    IEEE80211_HW_QUEUE_CONTROL,
    IEEE80211_HW_SUPPORTS_PER_STA_GTK,
    IEEE80211_HW_AP_LINK_PS,
    IEEE80211_HW_TX_AMPDU_SETUP_IN_HW,
    IEEE80211_HW_SUPPORTS_RC_TABLE,
    IEEE80211_HW_P2P_DEV_ADDR_FOR_INTF,
    IEEE80211_HW_TIMING_BEACON_ONLY,
    IEEE80211_HW_SUPPORTS_HT_CCK_RATES,
    IEEE80211_HW_CHANCTX_STA_CSA,
    IEEE80211_HW_SUPPORTS_CLONED_SKBS,
    IEEE80211_HW_SINGLE_SCAN_ON_ALL_BANDS,
    IEEE80211_HW_TDLS_WIDER_BW,
    IEEE80211_HW_SUPPORTS_AMSDU_IN_AMPDU,
    IEEE80211_HW_BEACON_TX_STATUS,
    IEEE80211_HW_NEEDS_UNIQUE_STA_ADDR,
    IEEE80211_HW_SUPPORTS_REORDERING_BUFFER,
    IEEE80211_HW_USES_RSS,
    IEEE80211_HW_TX_AMSDU,
    IEEE80211_HW_TX_FRAG_LIST,
    IEEE80211_HW_REPORTS_LOW_ACK,
    IEEE80211_HW_SUPPORTS_TX_FRAG,
    
    /* keep last, obviously */
    NUM_IEEE80211_HW_FLAGS
};


/** line 2106
 * struct ieee80211_hw - hardware information and state
 *
 * This structure contains the configuration and hardware
 * information for an 802.11 PHY.
 *
 * @wiphy: This points to the &struct wiphy allocated for this
 *    802.11 PHY. You must fill in the @perm_addr and @dev
 *    members of this structure using SET_IEEE80211_DEV()
 *    and SET_IEEE80211_PERM_ADDR(). Additionally, all supported
 *    bands (with channels, bitrates) are registered here.
 *
 * @conf: &struct ieee80211_conf, device configuration, don't use.
 *
 * @priv: pointer to private area that was allocated for driver use
 *    along with this structure.
 *
 * @flags: hardware flags, see &enum ieee80211_hw_flags.
 *
 * @extra_tx_headroom: headroom to reserve in each transmit skb
 *    for use by the driver (e.g. for transmit headers.)
 *
 * @extra_beacon_tailroom: tailroom to reserve in each beacon tx skb.
 *    Can be used by drivers to add extra IEs.
 *
 * @max_signal: Maximum value for signal (rssi) in RX information, used
 *    only when @IEEE80211_HW_SIGNAL_UNSPEC or @IEEE80211_HW_SIGNAL_DB
 *
 * @max_listen_interval: max listen interval in units of beacon interval
 *    that HW supports
 *
 * @queues: number of available hardware transmit queues for
 *    data packets. WMM/QoS requires at least four, these
 *    queues need to have configurable access parameters.
 *
 * @rate_control_algorithm: rate control algorithm for this hardware.
 *    If unset (NULL), the default algorithm will be used. Must be
 *    set before calling ieee80211_register_hw().
 *
 * @vif_data_size: size (in bytes) of the drv_priv data area
 *    within &struct ieee80211_vif.
 * @sta_data_size: size (in bytes) of the drv_priv data area
 *    within &struct ieee80211_sta.
 * @chanctx_data_size: size (in bytes) of the drv_priv data area
 *    within &struct ieee80211_chanctx_conf.
 * @txq_data_size: size (in bytes) of the drv_priv data area
 *    within @struct ieee80211_txq.
 *
 * @max_rates: maximum number of alternate rate retry stages the hw
 *    can handle.
 * @max_report_rates: maximum number of alternate rate retry stages
 *    the hw can report back.
 * @max_rate_tries: maximum number of tries for each stage
 *
 * @max_rx_aggregation_subframes: maximum buffer size (number of
 *    sub-frames) to be used for A-MPDU block ack receiver
 *    aggregation.
 *    This is only relevant if the device has restrictions on the
 *    number of subframes, if it relies on mac80211 to do reordering
 *    it shouldn't be set.
 *
 * @max_tx_aggregation_subframes: maximum number of subframes in an
 *    aggregate an HT driver will transmit. Though ADDBA will advertise
 *    a constant value of 64 as some older APs can crash if the window
 *    size is smaller (an example is LinkSys WRT120N with FW v1.0.07
 *    build 002 Jun 18 2012).
 *
 * @max_tx_fragments: maximum number of tx buffers per (A)-MSDU, sum
 *    of 1 + skb_shinfo(skb)->nr_frags for each skb in the frag_list.
 *
 * @offchannel_tx_hw_queue: HW queue ID to use for offchannel TX
 *    (if %IEEE80211_HW_QUEUE_CONTROL is set)
 *
 * @radiotap_mcs_details: lists which MCS information can the HW
 *    reports, by default it is set to _MCS, _GI and _BW but doesn't
 *    include _FMT. Use %IEEE80211_RADIOTAP_MCS_HAVE_\* values, only
 *    adding _BW is supported today.
 *
 * @radiotap_vht_details: lists which VHT MCS information the HW reports,
 *    the default is _GI | _BANDWIDTH.
 *    Use the %IEEE80211_RADIOTAP_VHT_KNOWN_\* values.
 *
 * @radiotap_timestamp: Information for the radiotap timestamp field; if the
 *    'units_pos' member is set to a non-negative value it must be set to
 *    a combination of a IEEE80211_RADIOTAP_TIMESTAMP_UNIT_* and a
 *    IEEE80211_RADIOTAP_TIMESTAMP_SPOS_* value, and then the timestamp
 *    field will be added and populated from the &struct ieee80211_rx_status
 *    device_timestamp. If the 'accuracy' member is non-negative, it's put
 *    into the accuracy radiotap field and the accuracy known flag is set.
 *
 * @netdev_features: netdev features to be set in each netdev created
 *    from this HW. Note that not all features are usable with mac80211,
 *    other features will be rejected during HW registration.
 *
 * @uapsd_queues: This bitmap is included in (re)association frame to indicate
 *    for each access category if it is uAPSD trigger-enabled and delivery-
 *    enabled. Use IEEE80211_WMM_IE_STA_QOSINFO_AC_* to set this bitmap.
 *    Each bit corresponds to different AC. Value '1' in specific bit means
 *    that corresponding AC is both trigger- and delivery-enabled. '0' means
 *    neither enabled.
 *
 * @uapsd_max_sp_len: maximum number of total buffered frames the WMM AP may
 *    deliver to a WMM STA during any Service Period triggered by the WMM STA.
 *    Use IEEE80211_WMM_IE_STA_QOSINFO_SP_* for correct values.
 *
 * @n_cipher_schemes: a size of an array of cipher schemes definitions.
 * @cipher_schemes: a pointer to an array of cipher scheme definitions
 *    supported by HW.
 * @max_nan_de_entries: maximum number of NAN DE functions supported by the
 *    device.
 */
struct ieee80211_hw {
    struct ieee80211_conf conf;
    struct wiphy *wiphy;
    const char *rate_control_algorithm;
    void *priv;
    unsigned long flags[BITS_TO_LONGS(NUM_IEEE80211_HW_FLAGS)];
    unsigned int extra_tx_headroom;
    unsigned int extra_beacon_tailroom;
    int vif_data_size;
    int sta_data_size;
    int chanctx_data_size;
    int txq_data_size;
    u16 queues;
    u16 max_listen_interval;
    s8 max_signal;
    u8 max_rates;
    u8 max_report_rates;
    u8 max_rate_tries;
    u8 max_rx_aggregation_subframes;
    u8 max_tx_aggregation_subframes;
    u8 max_tx_fragments;
    u8 offchannel_tx_hw_queue;
    u8 radiotap_mcs_details;
    u16 radiotap_vht_details;
    struct {
        int units_pos;
        s16 accuracy;
    } radiotap_timestamp;
    netdev_features_t netdev_features;
    u8 uapsd_queues;
    u8 uapsd_max_sp_len;
    u8 n_cipher_schemes;
    const struct ieee80211_cipher_scheme *cipher_schemes;
    u8 max_nan_de_entries;
};

static inline bool _ieee80211_hw_check(struct ieee80211_hw *hw,
                                       enum ieee80211_hw_flags flg)
{
    return test_bit(flg, hw->flags);
}
#define ieee80211_hw_check(hw, flg)    _ieee80211_hw_check(hw, IEEE80211_HW_##flg)

static inline void _ieee80211_hw_set(struct ieee80211_hw *hw,
                                     enum ieee80211_hw_flags flg)
{
    return set_bit(flg, hw->flags);
}
#define ieee80211_hw_set(hw, flg)    _ieee80211_hw_set(hw, IEEE80211_HW_##flg)


// line 5683
static inline enum nl80211_iftype
ieee80211_iftype_p2p(enum nl80211_iftype type, bool p2p)
{
    if (p2p) {
        switch (type) {
            case NL80211_IFTYPE_STATION:
                return NL80211_IFTYPE_P2P_CLIENT;
            case NL80211_IFTYPE_AP:
                return NL80211_IFTYPE_P2P_GO;
            default:
                break;
        }
    }
    return type;
}

// line 5699
static inline enum nl80211_iftype
ieee80211_vif_type_p2p(struct ieee80211_vif *vif)
{
    return ieee80211_iftype_p2p(vif->type, vif->p2p);
}



/**
 * struct ieee80211_tx_queue_params - transmit queue configuration
 *
 * The information provided in this structure is required for QoS
 * transmit queue configuration. Cf. IEEE 802.11 7.3.2.29.
 *
 * @aifs: arbitration interframe space [0..255]
 * @cw_min: minimum contention window [a value of the form
 *    2^n-1 in the range 1..32767]
 * @cw_max: maximum contention window [like @cw_min]
 * @txop: maximum burst time in units of 32 usecs, 0 meaning disabled
 * @acm: is mandatory admission control required for the access category
 * @uapsd: is U-APSD mode enabled for the queue
 * @mu_edca: is the MU EDCA configured
 * @mu_edca_param_rec: MU EDCA Parameter Record for HE
 */
struct ieee80211_tx_queue_params {
    u16 txop;
    u16 cw_min;
    u16 cw_max;
    u8 aifs;
    bool acm;
    bool uapsd;
    bool mu_edca;
    struct ieee80211_he_mu_edca_param_ac_rec mu_edca_param_rec;
};


/**
 * enum ieee80211_max_queues - maximum number of queues
 *
 * @IEEE80211_MAX_QUEUES: Maximum number of regular device queues.
 * @IEEE80211_MAX_QUEUE_MAP: bitmap with maximum queues set
 */
enum ieee80211_max_queues {
    IEEE80211_MAX_QUEUES =        16,
    IEEE80211_MAX_QUEUE_MAP =    BIT(IEEE80211_MAX_QUEUES) - 1,
};

/**
 * struct ieee80211_cipher_scheme - cipher scheme
 *
 * This structure contains a cipher scheme information defining
 * the secure packet crypto handling.
 *
 * @cipher: a cipher suite selector
 * @iftype: a cipher iftype bit mask indicating an allowed cipher usage
 * @hdr_len: a length of a security header used the cipher
 * @pn_len: a length of a packet number in the security header
 * @pn_off: an offset of pn from the beginning of the security header
 * @key_idx_off: an offset of key index byte in the security header
 * @key_idx_mask: a bit mask of key_idx bits
 * @key_idx_shift: a bit shift needed to get key_idx
 *     key_idx value calculation:
 *      (sec_header_base[key_idx_off] & key_idx_mask) >> key_idx_shift
 * @mic_len: a mic length in bytes
 */
struct ieee80211_cipher_scheme {
    u32 cipher;
    u16 iftype;
    u8 hdr_len;
    u8 pn_len;
    u8 pn_off;
    u8 key_idx_off;
    u8 key_idx_mask;
    u8 key_idx_shift;
    u8 mic_len;
};

/**
 * enum ieee80211_ac_numbers - AC numbers as used in mac80211
 * @IEEE80211_AC_VO: voice
 * @IEEE80211_AC_VI: video
 * @IEEE80211_AC_BE: best effort
 * @IEEE80211_AC_BK: background
 */
enum ieee80211_ac_numbers {
    IEEE80211_AC_VO        = 0,
    IEEE80211_AC_VI        = 1,
    IEEE80211_AC_BE        = 2,
    IEEE80211_AC_BK        = 3,
};

/* there are 40 bytes if you don't need the rateset to be kept */
#define IEEE80211_TX_INFO_DRIVER_DATA_SIZE 40

/* if you do need the rateset, then you have less space */
#define IEEE80211_TX_INFO_RATE_DRIVER_DATA_SIZE 24

/* maximum number of rate stages */
#define IEEE80211_TX_MAX_RATES    4

/* maximum number of rate table entries */
#define IEEE80211_TX_RATE_TABLE_SIZE    4

/**
 * struct ieee80211_tx_rate - rate selection/status
 *
 * @idx: rate index to attempt to send with
 * @flags: rate control flags (&enum mac80211_rate_control_flags)
 * @count: number of tries in this rate before going to the next rate
 *
 * A value of -1 for @idx indicates an invalid rate and, if used
 * in an array of retry rates, that no more rates should be tried.
 *
 * When used for transmit status reporting, the driver should
 * always report the rate along with the flags it used.
 *
 * &struct ieee80211_tx_info contains an array of these structs
 * in the control information, and it will be filled by the rate
 * control algorithm according to what should be sent. For example,
 * if this array contains, in the format { <idx>, <count> } the
 * information::
 *
 *    { 3, 2 }, { 2, 2 }, { 1, 4 }, { -1, 0 }, { -1, 0 }
 *
 * then this means that the frame should be transmitted
 * up to twice at rate 3, up to twice at rate 2, and up to four
 * times at rate 1 if it doesn't get acknowledged. Say it gets
 * acknowledged by the peer after the fifth attempt, the status
 * information should then contain::
 *
 *   { 3, 2 }, { 2, 2 }, { 1, 1 }, { -1, 0 } ...
 *
 * since it was transmitted twice at rate 3, twice at rate 2
 * and once at rate 1 after which we received an acknowledgement.
 */
struct ieee80211_tx_rate {
    s8 idx;
    u16 count:5,
flags:11;
};

/**
 * struct ieee80211_tx_info - skb transmit information
 *
 * This structure is placed in skb->cb for three uses:
 *  (1) mac80211 TX control - mac80211 tells the driver what to do
 *  (2) driver internal use (if applicable)
 *  (3) TX status information - driver tells mac80211 what happened
 *
 * @flags: transmit info flags, defined above
 * @band: the band to transmit on (use for checking for races)
 * @hw_queue: HW queue to put the frame on, skb_get_queue_mapping() gives the AC
 * @ack_frame_id: internal frame ID for TX status, used internally
 * @control: union for control data
 * @status: union for status data
 * @driver_data: array of driver_data pointers
 * @ampdu_ack_len: number of acked aggregated frames.
 *     relevant only if IEEE80211_TX_STAT_AMPDU was set.
 * @ampdu_len: number of aggregated frames.
 *     relevant only if IEEE80211_TX_STAT_AMPDU was set.
 * @ack_signal: signal strength of the ACK frame
 */
struct ieee80211_tx_info {
    /* common information */
    u32 flags;
    u8 band;
    
    u8 hw_queue;
    
    u16 ack_frame_id;
    
    union {
        struct {
            union {
                /* rate control */
                struct {
                    struct ieee80211_tx_rate rates[
                                                   IEEE80211_TX_MAX_RATES];
                    s8 rts_cts_rate_idx;
                    u8 use_rts:1;
                    u8 use_cts_prot:1;
                    u8 short_preamble:1;
                    u8 skip_table:1;
                    /* 2 bytes free */
                };
                /* only needed before rate control */
//                unsigned long jiffies;
            };
            /* NB: vif can be NULL for injected frames */
            struct ieee80211_vif *vif;
            struct ieee80211_key_conf *hw_key;
            u32 flags;
//            codel_time_t enqueue_time;
            user_time_t enqueue_time;
        } control;
        struct {
            u64 cookie;
        } ack;
        struct {
            struct ieee80211_tx_rate rates[IEEE80211_TX_MAX_RATES];
            s32 ack_signal;
            u8 ampdu_ack_len;
            u8 ampdu_len;
            u8 antenna;
            u16 tx_time;
            bool is_valid_ack_signal;
            void *status_driver_data[19 / sizeof(void *)];
        } status;
        struct {
            struct ieee80211_tx_rate driver_rates[
                                                  IEEE80211_TX_MAX_RATES];
            u8 pad[4];
            
            void *rate_driver_data[
                                   IEEE80211_TX_INFO_RATE_DRIVER_DATA_SIZE / sizeof(void *)];
        };
        void *driver_data[
                          IEEE80211_TX_INFO_DRIVER_DATA_SIZE / sizeof(void *)];
    };
};


/**
 * ieee80211_alloc_hw_nm - Allocate a new hardware device
 *
 * This must be called once for each hardware device. The returned pointer
 * must be used to refer to this device when calling other functions.
 * mac80211 allocates a private data area for the driver pointed to by
 * @priv in &struct ieee80211_hw, the size of this area is given as
 * @priv_data_len.
 *
 * @priv_data_len: length of private data
 * @ops: callbacks for this device
 * @requested_name: Requested name for this device.
 *    NULL is valid value, and means use the default naming (phy%d)
 *
 * Return: A pointer to the new hardware device, or %NULL on error.
 */
struct ieee80211_hw *ieee80211_alloc_hw_nm(size_t priv_data_len,
                                           const struct ieee80211_ops *ops,
                                           const char *requested_name);

/**
 * ieee80211_alloc_hw - Allocate a new hardware device
 *
 * This must be called once for each hardware device. The returned pointer
 * must be used to refer to this device when calling other functions.
 * mac80211 allocates a private data area for the driver pointed to by
 * @priv in &struct ieee80211_hw, the size of this area is given as
 * @priv_data_len.
 *
 * @priv_data_len: length of private data
 * @ops: callbacks for this device
 *
 * Return: A pointer to the new hardware device, or %NULL on error.
 */
static inline
struct ieee80211_hw *ieee80211_alloc_hw(size_t priv_data_len,
                                        const struct ieee80211_ops *ops)
{
    return ieee80211_alloc_hw_nm(priv_data_len, ops, NULL);
}

/**
 * wiphy_to_ieee80211_hw - return a mac80211 driver hw struct from a wiphy
 *
 * @wiphy: the &struct wiphy which we want to query
 *
 * mac80211 drivers can use this to get to their respective
 * &struct ieee80211_hw. Drivers wishing to get to their own private
 * structure can then access it via hw->priv. Note that mac802111 drivers should
 * not use wiphy_priv() to try to get their private driver structure as this
 * is already used internally by mac80211.
 *
 * Return: The mac80211 driver hw struct of @wiphy.
 */
struct ieee80211_hw *wiphy_to_ieee80211_hw(struct wiphy *wiphy);

/**
 * enum ieee80211_interface_iteration_flags - interface iteration flags
 * @IEEE80211_IFACE_ITER_NORMAL: Iterate over all interfaces that have
 *    been added to the driver; However, note that during hardware
 *    reconfiguration (after restart_hw) it will iterate over a new
 *    interface and over all the existing interfaces even if they
 *    haven't been re-added to the driver yet.
 * @IEEE80211_IFACE_ITER_RESUME_ALL: During resume, iterate over all
 *    interfaces, even if they haven't been re-added to the driver yet.
 * @IEEE80211_IFACE_ITER_ACTIVE: Iterate only active interfaces (netdev is up).
 */
enum ieee80211_interface_iteration_flags {
    IEEE80211_IFACE_ITER_NORMAL    = 0,
    IEEE80211_IFACE_ITER_RESUME_ALL    = BIT(0),
    IEEE80211_IFACE_ITER_ACTIVE    = BIT(1),
};
#endif /* mac80211_h */


