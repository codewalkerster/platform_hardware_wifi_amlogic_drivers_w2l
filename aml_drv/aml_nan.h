/**
 ****************************************************************************************
 *
 * @file aml_nan.h
 *
 * @brief nan api
 *
 * Copyright (C) Amlogic 2024-2034
 *
 ****************************************************************************************
 */

#ifndef _AML_NAN_H_
#define _AML_NAN_H_

#include <linux/types.h>

#ifndef MAC2STR
#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
#endif

/* Definitions for error constants. */
#define AML_OK          (0)       /* value indicating success (no error) */
#define AML_FAIL        (-1)      /* indicating failure */

#define NAN_WIFI_NAN_MAX_SVC_SUPPORTED  (2)
#define NAN_WIFI_MAX_SVC_NAME_LEN       (32)
#define NAN_WIFI_MAX_FILTER_LEN         (64)
#define NAN_WIFI_MAX_SVC_INFO_LEN       (255)

/*Max publish + subscribe numbers 4*/
#define NAN_MAX_PUBLISH_NUM 2
#define NAN_MAX_SUBSCRIBE_NUM 2

/* NAN Service Name Hash Length */
#define NAN_SERVICE_HASH_LENGTH 6

#define NAN_MAX_PEERS_RECORD    15
#define NAN_PUBLISH         2
#define NAN_SUBSCRIBE       1

#define NAN_SDA_SERVICE_CONTROL_TYPE_PUBLISH 0
#define NAN_SDA_SERVICE_CONTROL_TYPE_SUBSCRIBE BIT(0)
#define NAN_SDA_SERVICE_CONTROL_TYPE_FOLLOWUP BIT(1)

/**
  * @brief NAN Discovery start configuration
  *
  */
typedef struct  {
    //uint8_t vif_idx;
    uint8_t op_channel;    /* NAN Discovery operating channel */
    uint8_t master_pref;   /* Device's preference value to serve as NAN Master */
    uint8_t random_factor;
    uint8_t scan_time;     /* Scan time in seconds while searching for a NAN cluster */
    uint16_t warm_up_sec;  /* Warm up time before assuming NAN Anchor Master role */
} wifi_nan_cfg;

/**
  * @brief NAN Services types
  *
  */
typedef enum {
    NAN_PUBLISH_SOLICITED,  /* Send unicast Publish frame to Subscribers that match the requirement */
    NAN_PUBLISH_UNSOLICITED,/* Send broadcast Publish frames in every Discovery Window(DW) */
    NAN_SUBSCRIBE_ACTIVE,   /* Send broadcast Subscribe frames in every DW */
    NAN_SUBSCRIBE_PASSIVE,  /* Passively listens to Publish frames */
} wifi_nan_service_type_t;

/**
  * @brief NAN Publish service configuration parameters
  *
  */
typedef struct {
    uint8_t publish_id;
    uint8_t inst_id;                                /**< Own service instance id */
    uint8_t peer_inst_id;                           /**< Peer's service instance id */
    wifi_nan_service_type_t type;                   /**< Service type */
    uint8_t service_name_len;
    uint8_t service_name[NAN_WIFI_MAX_SVC_NAME_LEN];   /* Service name identifier */
    uint8_t svc_info_len;
    uint8_t svc_info[NAN_WIFI_MAX_SVC_INFO_LEN];       /* Service info shared in Subscribe frame */
    uint16_t match_filter_len;
    uint8_t matching_filter[NAN_WIFI_MAX_FILTER_LEN];  /* Comma separated filters for filtering services */
    uint8_t service_name_hash[NAN_SERVICE_HASH_LENGTH];
    uint8_t peer_mac[6];
} wifi_nan_publish_cfg;

/**
  * @brief NAN Subscribe service configuration parameters
  *
  */
typedef struct {
    uint8_t subscribe_id;
    wifi_nan_service_type_t type;                   /* Service type */
    uint8_t service_name_len;
    uint8_t service_name[NAN_WIFI_MAX_SVC_NAME_LEN];   /* Service name identifier */
    uint8_t svc_info_len;
    uint8_t svc_info[NAN_WIFI_MAX_SVC_INFO_LEN];       /* Service info shared in Subscribe frame */
    uint16_t match_filter_len;
    uint8_t matching_filter[NAN_WIFI_MAX_FILTER_LEN];  /* Comma separated filters for filtering services */
    uint8_t service_name_hash[NAN_SERVICE_HASH_LENGTH];
} wifi_nan_subscribe_cfg;

/**
  * @brief NAN Follow-up parameters
  *
  */
typedef struct {
    uint8_t inst_id;                         /* Own service instance id */
    uint8_t peer_inst_id;                    /* Peer's service instance id */
    uint8_t peer_mac[6];                     /* Peer's MAC address */
    uint8_t svc_info_len;
    uint8_t svc_info[NAN_WIFI_MAX_SVC_INFO_LEN];/* Service info(or message) to be shared */
    uint8_t service_name_hash[NAN_SERVICE_HASH_LENGTH];
} wifi_nan_followup_cfg;


/** Parameters of a peer service record */
typedef struct {
    uint8_t peer_svc_id;   /**< Identifier of Peer's service */
    uint8_t own_svc_id;    /**< Identifier of own service associated with Peer */
    uint8_t peer_svc_type; /**< Peer's service type (Publish/Subscribe) */
} nan_peer_record;

struct peer_svc_info_list {
    struct list_head next;
    uint8_t peer_svc_info[NAN_WIFI_MAX_SVC_INFO_LEN];   /**< Information for followup message */
    uint8_t svc_id;                                     /**< Identifier of peer's service */
    uint8_t own_svc_id;                                 /**< Identifier for own service  */
    uint8_t type;                                       /**< Service type (Publish/Subscribe) */
    uint8_t peer_nmi[6];                                /**< Peer's NAN Management Interface address */
};

struct peer_svc_info {
    uint8_t peer_svc_info[NAN_WIFI_MAX_SVC_INFO_LEN];   /**< Information for followup message */
    uint8_t svc_id;                                     /**< Identifier of peer's service */
    uint8_t own_svc_id;                                 /**< Identifier for own service  */
    uint8_t type;                                       /**< Service type (Publish/Subscribe) */
    uint8_t peer_nmi[6];                                /**< Peer's NAN Management Interface address */
    uint8_t service_name_hash[NAN_SERVICE_HASH_LENGTH];
};

struct own_svc_info {
    struct list_head peer_list;                         /**< List of peers matched for specific service */
    uint8_t svc_name[NAN_WIFI_MAX_SVC_NAME_LEN];           /**< Name identifying a service */
    uint8_t svc_id;                                     /**< Identifier for a service */
    uint8_t type;                                       /**< Service type (Publish/Subscribe) */
    uint8_t num_peer_records;                           /**< Count of peer records associated with svc_id */
};

typedef struct {
    uint8_t state;
    uint8_t nan_svc_num;
    struct own_svc_info own_svc[NAN_WIFI_NAN_MAX_SVC_SUPPORTED]; /**< Record of own service(s) */
} nan_ctx_t;

/// Structure containing the parameters of the @ref PRIV_NAN_ENABLE_CFM message.
struct nan_enable_cfm
{
    /// Status of the AP starting procedure
    uint8_t status;
    /// Index of the VIF for which the AP is started
    uint8_t vif_idx;
    /// Index of the channel context attached to the VIF
    uint8_t ch_idx;
    /// Index of the STA used for BC/MC traffic
    uint8_t bcmc_idx;
};

/**
  * @brief      Start NAN Discovery with provided configuration
  *
  * @attention  This API should be called after aml_cfg80211_init().
  *
  * @param      nan_cfg  NAN related parameters to be configured.
  *
  * @return
  *    - ESP_OK: succeed
  *    - others: failed
  */
uint32_t aml_wifi_nan_start(const wifi_nan_cfg *nan_cfg);

/**
  * @brief      Stop NAN Discovery, end NAN Services and Datapaths
  *
  * @return
  *    - ESP_OK: succeed
  *    - others: failed
  */
uint32_t aml_wifi_nan_stop(void);

/**
  * @brief      Start Publishing a service to the NAN Peers in vicinity
  *
  * @attention  This API should be called after aml_wifi_nan_start().
  *
  * @param      publish_cfg  Configuration parameters for publishing a service.
  *
  * @return
  *    - non-zero: Publish service identifier
  *    - zero: failed
  */
uint32_t aml_nan_publish_req(wifi_nan_publish_cfg *publish_conf);
uint32_t aml_nan_subscribe_req(wifi_nan_subscribe_cfg *subscribe_conf);
uint32_t aml_nan_followup_send(wifi_nan_followup_cfg *fup_params);
int hwaddr_aton2(const char *txt, uint8_t *addr);

uint32_t aml_wifi_nan_publish_service(const wifi_nan_publish_cfg *publish_cfg);

/**
  * @brief      Subscribe for a service within the NAN cluster
  *
  * @attention  This API should be called after aml_wifi_nan_start().
  *
  * @param      subscribe_cfg  Configuration parameters for subscribing for a service.
  *
  * @return
  *    - non-zero: Subscribe service identifier
  *    - zero: failed
  */
uint32_t aml_wifi_nan_subscribe_service(const wifi_nan_subscribe_cfg *subscribe_cfg);

/**
  * @brief      Send a follow-up message to the NAN Peer with matched service
  *
  * @attention  This API should be called after a NAN service is discovered due to a match.
  *
  * @param      fup_params  Configuration parameters for sending a Follow-up message.
  *
  * @return
  *    - AML_OK: succeed
  *    - others: failed
  */
uint32_t aml_wifi_nan_send_message(wifi_nan_followup_cfg *fup_params);

/**
  * @brief      Cancel a NAN service
  *
  * @param      service_id Publish/Subscribe service id to be cancelled.
  *
  * @return
  *    - AML_OK: succeed
  *    - others: failed
  */
uint32_t aml_wifi_nan_cancel_service(uint8_t service_id);


/**
 * brief         Get own Service information from Service ID OR Name.
 *
 * @attention    If service information is to be fetched from service name, set own_svc_id as zero.
 *
 * @param[inout] own_svc_id As input, it indicates Service ID to search for.
 *                          As output, it indicates Service ID of the service found using Service Name.
 * @param[inout] svc_name   As input, it indicates Service Name to search for.
 *                          As output, it indicates Service Name of the service found using Service ID.
 * @param[out]   num_peer_records  Number of peers discovered by corresponding service.
 * @return
 *   - AML_OK: succeed
 *   - AML_FAIL: failed
 */
uint32_t aml_wifi_nan_get_own_svc_info(uint8_t *own_svc_id, char *svc_name, int *num_peer_records);

/**
 * brief         Get a list of Peers discovered by the given Service.
 *
 * @param[inout] num_peer_records As input param, it stores max peers peer_record can hold.
 *               As output param, it specifies the actual number of peers this API returns.
 * @param        own_svc_id  Service ID of own service.
 * @param[out]   peer_record Pointer to first peer record.
 * @return
 *   - AML_OK: succeed
 *   - AML_FAIL: failed
 */
uint32_t aml_wifi_nan_get_peer_records(int *num_peer_records, uint8_t own_svc_id, nan_peer_record *peer_record);

/**
 * brief         Find Peer's Service information using Peer MAC and optionally Service Name.
 *
 * @param       svc_name    Service Name of the published/subscribed service.
 * @param       peer_mac    Peer's NAN Management Interface MAC address.
 * @param[out]  peer_info   Peer's service information structure.
 * @return
 *   - AML_OK: succeed
 *   - AML_FAIL: failed
 */
uint32_t aml_wifi_nan_get_peer_info(char *svc_name, uint8_t *peer_mac, nan_peer_record *peer_info);

#endif
