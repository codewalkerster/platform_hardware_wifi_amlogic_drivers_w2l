
#define AML_MODULE     CSI

#include <aml_defs.h>
#include <net/sock.h>
#include "aml_csi.h"

#define AML_CSI_NL_PROTOCOL (29)

struct aml_csi_nl_info {
    struct sock * fw_csi_sock;
    int user_pid;
    int enable;
};

struct csi_nl_msg_info {
    int msg_type;
    int msg_len;
};

struct aml_csi_nl_info g_csi_nl_info;

int aml_send_csi_data_to_user(char *pbuf, uint16_t len, int msg_type)
{
    struct sk_buff *nl_skb;
    struct nlmsghdr *nlh = NULL;   //msg head
    struct csi_nl_msg_info * nl_csi_info = NULL;
    int ret;
    static int seq_num = 0;
    int buf_len = NLMSG_SPACE(len + sizeof(struct csi_nl_msg_info));

    if (!g_csi_nl_info.fw_csi_sock || !g_csi_nl_info.user_pid ||
        !g_csi_nl_info.enable) {
        AML_ERR("kernel trace nl sock para invalid , can not upload msg to user\n");
        return -1;
    }
    //create sk_buff
    nl_skb = nlmsg_new(buf_len, GFP_ATOMIC);
    if (!nl_skb)
    {
        AML_ERR("netlink alloc failure\n");
        return -1;
    }

    /* build netlink msg head */
    nlh = nlmsg_put(nl_skb, 0, 0, AML_CSI_NL_PROTOCOL, buf_len, 0);
    if (nlh == NULL)
    {
        AML_ERR("nlmsg_put failure \n");
        nlmsg_free(nl_skb);
        return -1;
    }
    NETLINK_CB(nl_skb).portid = 0;
    NETLINK_CB(nl_skb).dst_group = 0;
    nl_csi_info = (struct nl_csi_info*)nlmsg_data(nlh);
    nl_csi_info->msg_len = len;
    nl_csi_info->msg_type = msg_type;
    nlh->nlmsg_seq = seq_num++;

    /* copy data and send it */
    if (pbuf) {
        memcpy(nlmsg_data(nlh) + sizeof(struct csi_nl_msg_info), pbuf, len);
    }
    ret = netlink_unicast(g_csi_nl_info.fw_csi_sock, nl_skb, g_csi_nl_info.user_pid, 0);

    AML_INFO("==== kernel upload csi data to user result: %d, seq: %d\n", ret, seq_num - 1);
    return ret;

}

// recv msg handl function
static void aml_recv_csi_netlink(struct sk_buff *skb)
{
    struct nlmsghdr *nlh;
    struct csi_nl_msg_info * nl_log_info = NULL;
    nlh = nlmsg_hdr(skb); // get msg body
    AML_INFO("kernel rcv msg type: %d, pid: %d, len: %d, flag: %d, seq: %d\n",
        nlh->nlmsg_type, nlh->nlmsg_pid, nlh->nlmsg_len, nlh->nlmsg_flags, nlh->nlmsg_seq);
    AML_INFO("receive data from user process: %s\n", (char *)NLMSG_DATA(nlh));

    nl_log_info = (struct nl_log_info*)NLMSG_DATA(nlh);
    switch (nl_log_info->msg_type) {
        case AML_CSI_FUNC_START:
            g_csi_nl_info.user_pid = nlh->nlmsg_pid;
            g_csi_nl_info.enable = 1;
            AML_INFO("user space process (pid: %d) start recv csi data !!!!\n", g_csi_nl_info.user_pid);
            break;
        case AML_CSI_FUNC_STOP:
            g_csi_nl_info.enable = 0;
            AML_INFO("user space process (pid: %d) stop recv csi data !!!!\n", g_csi_nl_info.user_pid);
            break;
        default:
            AML_ERR("unknown msg (0x%x) from user space process (pid: %d), ignore !!!!\n",
                nl_log_info->msg_type, g_csi_nl_info.user_pid);
            break;
    }

    return;
}

int aml_csi_nl_init(void)
{
    memset(&g_csi_nl_info, 0, sizeof(struct csi_nl_msg_info));
    struct netlink_kernel_cfg cfg = {
        .input = aml_recv_csi_netlink,
    };
    g_csi_nl_info.fw_csi_sock = netlink_kernel_create(&init_net, AML_CSI_NL_PROTOCOL, &cfg);
    if (!g_csi_nl_info.fw_csi_sock) {
        AML_ERR("aml csi netlink init failed");
        return -1;
    }

    AML_INFO("aml csi netlink init OK!\n");
    return 0;
}

void aml_csi_nl_destroy(void)
{
    if (g_csi_nl_info.fw_csi_sock) {
        netlink_kernel_release(g_csi_nl_info.fw_csi_sock);
    }
    AML_INFO("fw csi upload socket destroy!!!\n");

    return;
}

