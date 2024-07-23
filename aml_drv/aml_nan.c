/**
 ******************************************************************************
 *
 * @file aml_msg_tx.c
 *
 * @brief nan function definitions
 *
 * Copyright (C) Amlogic 2012-2021
 *
 ******************************************************************************
 */

#include "aml_sha256_i.h"
#include "aml_nan.h"

uint8_t g_ucInstanceID = 0;
static nan_ctx_t s_nan_ctx = {0};

void nan_util_dump(uint8_t *msg, uint8_t *content, uint32_t len)
{
    uint8_t aucBuf[16];

    AML_INFO("%s, len:%d\n", msg, len);

    while (len >= 16) {
        AML_INFO("%p: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
               content, content[0], content[1], content[2],
               content[3], content[4], content[5],
               content[6], content[7], content[8],
               content[9], content[10], content[11],
               content[12], content[13], content[14],
               content[15]);

        len -= 16;
        content += 16;
    }

    if (len > 0) {
        memset(aucBuf, 0, 16);
        memcpy(aucBuf, content, len);

        AML_INFO("%p: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
               content, aucBuf[0], aucBuf[1], aucBuf[2], aucBuf[3],
               aucBuf[4], aucBuf[5], aucBuf[6], aucBuf[7], aucBuf[8],
               aucBuf[9], aucBuf[10], aucBuf[11], aucBuf[12],
               aucBuf[13], aucBuf[14], aucBuf[15]);
    }
}

static int hex2num(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

/**
 * hwaddr_aton2 - Convert ASCII string to MAC address (in any known format)
 * @txt: MAC address as a string (e.g., 00:11:22:33:44:55 or 0011.2233.4455)
 * @addr: Buffer for the MAC address (ETH_ALEN = 6 bytes)
 * Returns: Characters used (> 0) on success, -1 on failure
 */
int hwaddr_aton2(const char *txt, u8 *addr)
{
    int i;
    const char *pos = txt;

    for (i = 0; i < 6; i++) {
        int a, b;

        while (*pos == ':' || *pos == '.' || *pos == '-')
            pos++;

        a = hex2num(*pos++);
        if (a < 0)
            return -1;
        b = hex2num(*pos++);
        if (b < 0)
            return -1;
        *addr++ = (a << 4) | b;
    }

    return pos - txt;
}

static void aml_nan_record_own_svc(uint8_t id, uint8_t type, const char svc_name[])
{
    struct own_svc_info *p_svc = NULL;

    for (int i = 0; i < NAN_WIFI_NAN_MAX_SVC_SUPPORTED; i++) {
        if (s_nan_ctx.own_svc[i].svc_id == 0) {
            p_svc = &s_nan_ctx.own_svc[i];
            break;
        }
    }

    if (!p_svc) {
        return;
    }

    p_svc->svc_id = id;
    p_svc->type = type;
    strlcpy(p_svc->svc_name, svc_name, NAN_WIFI_MAX_SVC_NAME_LEN);
    INIT_LIST_HEAD(&(p_svc->peer_list));
}

static struct own_svc_info *nan_find_own_svc(uint8_t svc_id)
{
    struct own_svc_info *p_svc = NULL;

    if (svc_id == 0) {
        AML_INFO("Service id cannot be 0!");
        return NULL;
    }

    for (int i = 0; i < NAN_WIFI_NAN_MAX_SVC_SUPPORTED; i++) {
        if (s_nan_ctx.own_svc[i].svc_id == svc_id) {
            p_svc = &s_nan_ctx.own_svc[i];
            break;
        }
    }

    return p_svc;
}

static void aml_nan_get_service_name_hash(uint8_t service_name_hash[], char svc_name[], int svc_name_len)
{
    char aucServiceName[256] = {0};
    struct sha256_state r_SHA_256_state = {0};
    uint8_t auc_tk[32] = {0};
    uint32_t u4Idx = 0;

    memcpy(aucServiceName, svc_name, svc_name_len);
    for (u4Idx = 0; u4Idx < strlen(aucServiceName); u4Idx++) {
        if ((aucServiceName[u4Idx] >= 'A') &&
            (aucServiceName[u4Idx] <= 'Z'))
            aucServiceName[u4Idx] = aucServiceName[u4Idx] + 32;
    }
    sha256_init(&r_SHA_256_state);
    sha256_process(&r_SHA_256_state, aucServiceName, strlen(aucServiceName));
    sha256_done(&r_SHA_256_state, auc_tk);
    memcpy(service_name_hash, auc_tk, NAN_SERVICE_HASH_LENGTH);
    nan_util_dump("service hash", auc_tk, NAN_SERVICE_HASH_LENGTH);
}

uint32_t aml_nan_publish_req(wifi_nan_publish_cfg *publish_conf)
{
    char aucServiceName[256] = {0};
    struct sha256_state r_SHA_256_state = {0};
    uint8_t auc_tk[32] = {0};
    uint32_t u4Idx = 0;

    if (s_nan_ctx.nan_svc_num >= NAN_WIFI_NAN_MAX_SVC_SUPPORTED) {
        AML_INFO("Exceed max number, publish fail");
        //return 0;
    }

    if (publish_conf->publish_id == 0) {
        publish_conf->publish_id = ++g_ucInstanceID;
    }

    s_nan_ctx.nan_svc_num++;

    if (g_ucInstanceID == 255)
        g_ucInstanceID = 0;

    publish_conf->service_name_len = strlen(publish_conf->service_name);
    if (publish_conf->service_name_len > NAN_WIFI_MAX_SVC_NAME_LEN)
        publish_conf->svc_info_len = NAN_WIFI_MAX_SVC_NAME_LEN;

    aml_nan_get_service_name_hash(publish_conf->service_name_hash, publish_conf->service_name, publish_conf->service_name_len);

    publish_conf->svc_info_len = strlen(publish_conf->svc_info);
    if (publish_conf->svc_info_len > NAN_WIFI_MAX_SVC_INFO_LEN)
        publish_conf->svc_info_len = NAN_WIFI_MAX_SVC_INFO_LEN;

    AML_INFO("svc_name: %s, svc_len: %d, svc_info: %s, svc_info_len: %d",
        publish_conf->service_name, publish_conf->service_name_len, publish_conf->svc_info, publish_conf->svc_info_len);

    aml_nan_record_own_svc(publish_conf->publish_id, NAN_PUBLISH, publish_conf->service_name);

    return publish_conf->publish_id;
}

uint32_t aml_nan_subscribe_req(wifi_nan_subscribe_cfg *subscribe_conf)
{
    char aucServiceName[256] = {0};
    struct sha256_state r_SHA_256_state = {0};
    uint8_t auc_tk[32] = {0};
    uint32_t u4Idx = 0;

    if (s_nan_ctx.nan_svc_num >= NAN_WIFI_NAN_MAX_SVC_SUPPORTED) {
        AML_INFO("Exceed max number, subscribe fail");
        //return 0;
    }

    if (subscribe_conf->subscribe_id == 0) {
        subscribe_conf->subscribe_id = ++g_ucInstanceID;
    }
    s_nan_ctx.nan_svc_num++;

    if (g_ucInstanceID == 255)
        g_ucInstanceID = 0;

    subscribe_conf->service_name_len = strlen(subscribe_conf->service_name);
    if (subscribe_conf->service_name_len > NAN_WIFI_MAX_SVC_NAME_LEN)
        subscribe_conf->service_name_len = NAN_WIFI_MAX_SVC_NAME_LEN;

    aml_nan_get_service_name_hash(subscribe_conf->service_name_hash, subscribe_conf->service_name, subscribe_conf->service_name_len);

    subscribe_conf->svc_info_len = strlen(subscribe_conf->svc_info);
    if (subscribe_conf->svc_info_len > NAN_WIFI_MAX_SVC_INFO_LEN)
        subscribe_conf->svc_info_len = NAN_WIFI_MAX_SVC_INFO_LEN;

    aml_nan_record_own_svc(subscribe_conf->subscribe_id, NAN_SUBSCRIBE, subscribe_conf->service_name);

    return subscribe_conf->subscribe_id;
}

uint32_t aml_nan_followup_send(wifi_nan_followup_cfg *fup_params)
{
    struct own_svc_info *p_svc = NULL;

    p_svc = nan_find_own_svc(fup_params->inst_id);
    if (p_svc) {
        aml_nan_get_service_name_hash(fup_params->service_name_hash, p_svc->svc_name, strlen(p_svc->svc_name));
        fup_params->svc_info_len = strlen(fup_params->svc_info);
        if (fup_params->svc_info_len > NAN_WIFI_MAX_SVC_INFO_LEN)
            fup_params->svc_info_len = NAN_WIFI_MAX_SVC_INFO_LEN;
    } else {
        AML_INFO("[NAN] Cannot send Follow-up, service not exist.");
        return AML_FAIL;
    }

    return AML_OK;
}

int aml_nan_service_recv(struct peer_svc_info *peer_svc)
{
    switch (peer_svc->type) {
        case NAN_SDA_SERVICE_CONTROL_TYPE_PUBLISH:
            // recv publish service
            AML_INFO("===recv publish service:======================");
            AML_INFO("service id:            "MACSTR"", MAC2STR(peer_svc->service_name_hash));
            AML_INFO("peer_mac:              "MACSTR"", MAC2STR(peer_svc->peer_nmi));
            AML_INFO("instance_id:           %d", peer_svc->svc_id);
            AML_INFO("request_instance_id:   %d", peer_svc->own_svc_id);
            AML_INFO("peer_svc_info:         %s", peer_svc->peer_svc_info);
            AML_INFO("==============================================");
            // check own subscribe svc is this publish svc, if yes, store this peer svc.
            break;
        case NAN_SDA_SERVICE_CONTROL_TYPE_SUBSCRIBE:
            // recv subscribe service
            AML_INFO("===recv subscribe service:====================");
            AML_INFO("service id:            "MACSTR"", MAC2STR(peer_svc->service_name_hash));
            AML_INFO("peer_mac:              "MACSTR"", MAC2STR(peer_svc->peer_nmi));
            AML_INFO("instance_id:           %d", peer_svc->svc_id);
            AML_INFO("request_instance_id:   %d", peer_svc->own_svc_id);
            AML_INFO("peer_svc_info:         %s", peer_svc->peer_svc_info);
            AML_INFO("==============================================");
            break;
        case NAN_SDA_SERVICE_CONTROL_TYPE_FOLLOWUP:
            // recv follow up msg
            AML_INFO("===recv follow up service:====================");
            AML_INFO("service id:            "MACSTR"", MAC2STR(peer_svc->service_name_hash));
            AML_INFO("peer_mac:              "MACSTR"", MAC2STR(peer_svc->peer_nmi));
            AML_INFO("instance_id:           %d", peer_svc->svc_id);
            AML_INFO("request_instance_id:   %d", peer_svc->own_svc_id);
            AML_INFO("peer_svc_info:         %s", peer_svc->peer_svc_info);
            AML_INFO("==============================================");
            break;
        default:
            break;
    }

    return 0;
}

