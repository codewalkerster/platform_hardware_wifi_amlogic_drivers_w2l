#include "aml_debugfs.h"
#include "aml_defs.h"
//#include "aml_version_gen.h"
//#include "aml_platform.h"
#include "linux/proc_fs.h"
#include "aml_main.h"
#include "aml_msg_tx.h"
#include "aml_regdom.h"
#include <linux/ctype.h>

#define PROC_FILE_READ_FUNC(name)                                       \
    static ssize_t aml_proc_##name##_read(struct file *file,            \
                                          char __user *user_buf,        \
                                          size_t count, loff_t *ppos);

#define PROC_FILE_WRITE_FUNC(name)                                      \
    static ssize_t aml_proc_##name##_write(struct file *file,           \
                                           const char __user *user_buf, \
                                           size_t count, loff_t *ppos);

//#define PROCDATGA_VARIABLE_MODE
#ifdef PROCDATGA_VARIABLE_MODE

static int aml_drv_proc_open(struct inode *inode, struct file *file);

#define AML_PROC_FILE_WR_OPS(name)                              \
    PROC_FILE_READ_FUNC(name);                                  \
    PROC_FILE_WRITE_FUNC(name);                                 \
static const struct file_operations aml_proc_##name##_ops = {   \
    .owner = THIS_MODULE,                                       \
    .write = aml_proc_##name##_write,                           \
    .open = aml_drv_proc_open,                                  \
    .read = aml_proc_##name##_read,                             \
    .llseek = seq_lseek,                                        \
    .release = single_release,                                  \
};

#define AML_PROC_FILE_R_OPS(name)                               \
static const struct file_operations aml_proc_##name##_ops = {   \
    .owner = THIS_MODULE,                                       \
    .open = aml_drv_proc_open,                                  \
    .read = aml_proc_##name##_read,                             \
    .llseek = seq_lseek,                                        \
    .release = single_release,                                  \
};

#define AML_PROC_ADD_FILE(name, parent, mode) do {                          \
        struct proc_dir_entry *__tmp;                                       \
        __tmp = proc_create_data(#name, mode, parent, &aml_proc_##name##_ops, aml_hw);   \
        if (IS_ERR_OR_NULL(__tmp))                                          \
            goto err;                                                       \
    } while (0)

#define SYSFS_RO_FILE_OPS(name) DEVICE_ATTR(name, S_IRUGO, aml_sysfs_##name##_read, NULL)
#define SYSFS_WO_FILE_OPS(name) DEVICE_ATTR(name, S_IWUSR|S_IWGRP, NULL, aml_sysfs_##name##_write)
#define SYSFS_RW_FILE_OPS(name)     \
    DEVICE_ATTR(name, S_IWUSR|S_IWGRP|S_IRUGO, aml_sysfs_##name##_read, aml_sysfs_##name##_write)


extern struct aml_pm_type g_wifi_pm;
static ssize_t aml_proc_drv_state_read(struct file *file,
                                       char __user *user_buf,
                                       size_t count, loff_t *ppos)
{
    struct aml_hw *aml_hw = ((struct seq_file *)file->private_data)->private;
    int i;
    char buf[20];
    int len = 0;
    int max_len = min_t(size_t, sizeof(buf) - 1, count);
    ssize_t read;

    printk("yzy test __delay_info_show aml_hw:%p\n", aml_hw);

    if (!atomic_read(&g_wifi_pm.wifi_enable)) {
        len += scnprintf(&buf[len], max_len - len, "not_ready\n");
    }
#ifdef CONFIG_AML_RECOVERY
    else if (aml_recy_flags_chk(AML_RECY_STATE_ONGOING)) {
        len += scnprintf(&buf[len], max_len - len, "recovery\n");
    }
#endif
    else {
        len += scnprintf(&buf[len], max_len - len, "ready\n");
    }

    read = simple_read_from_buffer(user_buf, count, ppos, buf, len);

    return read;
}
AML_PROC_FILE_R_OPS(drv_state);

static ssize_t aml_proc_driver_read(struct file *file,
                                     char __user *user_buf,
                                     size_t count, loff_t *ppos)
{
    struct aml_hw *aml_hw = ((struct seq_file *)file->private_data)->private;
    struct wiphy *wiphy = aml_hw->wiphy;
    int i;
    char buf[1000];
    char dfs_buf[200] = {0};
    int max_len = min_t(size_t, sizeof(buf) - 1, count);
    int len = 0;
    int dfs_buf_len = 0;
    ssize_t read;
    int bw;

    len += scnprintf(&buf[len], max_len - len,
                     "drv_version:%s\n", AML_VERS_BANNER);

    len += scnprintf(&buf[len], max_len - len,
                     "country:%c%c\n", aml_hw->customer_priv.alpha2[0], aml_hw->customer_priv.alpha2[1]);

    if (wiphy->bands[NL80211_BAND_2GHZ] != NULL) {
        struct ieee80211_supported_band *b = wiphy->bands[NL80211_BAND_2GHZ];
        for (i = 0; i < b->n_channels; i++) {
            if (b->channels[i].flags & IEEE80211_CHAN_RADAR) {
                dfs_buf_len += scnprintf(&dfs_buf[dfs_buf_len], sizeof(dfs_buf) - dfs_buf_len,
                                 "%d,", ieee80211_frequency_to_channel(b->channels[i].center_freq));
            }
            else {
                bw = 160;
                if (b->channels[i].flags & IEEE80211_CHAN_NO_160MHZ)
                    bw = 80;
                if (b->channels[i].flags & IEEE80211_CHAN_NO_80MHZ)
                    bw = 40;
                if (b->channels[i].flags &
                    (IEEE80211_CHAN_NO_HT40PLUS | IEEE80211_CHAN_NO_HT40MINUS))
                    bw = 20;
                if (bw > 80)
                    bw = 40;
                if ((b->channels[i].flags & IEEE80211_CHAN_DISABLED) != IEEE80211_CHAN_DISABLED)
                    len += scnprintf(&buf[len], max_len - len, "CH-%d:\tBW_%dMHz\t(flag=0x%x)\n",
                                     ieee80211_frequency_to_channel(b->channels[i].center_freq),
                                     bw, b->channels[i].flags);
            }
        }
    }

    if (wiphy->bands[NL80211_BAND_5GHZ] != NULL) {
        struct ieee80211_supported_band *b = wiphy->bands[NL80211_BAND_5GHZ];
        for (i = 0; i < b->n_channels; i++) {
            if (b->channels[i].flags & IEEE80211_CHAN_RADAR) {
                dfs_buf_len += scnprintf(&dfs_buf[dfs_buf_len], sizeof(dfs_buf) - dfs_buf_len,
                                 "%d,", ieee80211_frequency_to_channel(b->channels[i].center_freq));
            }
            else {
                bw = 160;
                if (b->channels[i].flags & IEEE80211_CHAN_NO_160MHZ)
                    bw = 80;
                if (b->channels[i].flags & IEEE80211_CHAN_NO_80MHZ)
                    bw = 40;
                if (b->channels[i].flags &
                    (IEEE80211_CHAN_NO_HT40PLUS | IEEE80211_CHAN_NO_HT40MINUS))
                    bw = 20;
                if (bw > 80)
                    bw = 80;
                if ((b->channels[i].flags & IEEE80211_CHAN_DISABLED) != IEEE80211_CHAN_DISABLED)
                    len += scnprintf(&buf[len], max_len - len, "CH-%d:\tBW_%dMHz\t(flag=0x%x)\n",
                                     ieee80211_frequency_to_channel(b->channels[i].center_freq),
                                     bw, b->channels[i].flags);
            }
        }
    }

    len += scnprintf(&buf[len], max_len - len,
                     "bypassdfs:%d\n", aml_hw->customer_priv.dfs_on);

    if (aml_hw->customer_priv.dfs_on) {
        int tmp = strlen(dfs_buf);
        if (tmp > 0) {
            printk("%c %c %c %c %c\n", dfs_buf[tmp - 2], dfs_buf[tmp - 1], dfs_buf[tmp], dfs_buf[tmp + 1], dfs_buf[tmp + 2]);
            dfs_buf[tmp - 1] = '\n';

            len += scnprintf(&buf[len], max_len - len, "DFS channel:");
            len += scnprintf(&buf[len], max_len - len, "%s\n", dfs_buf);
        }
    }

    read = simple_read_from_buffer(user_buf, count, ppos, buf, len);

    return read;
}

static ssize_t aml_proc_driver_write(struct file *file,
                                     const char __user *user_buf,
                                     size_t count, loff_t *ppos)
{
    struct aml_hw *aml_hw = ((struct seq_file *)file->private_data)->private;
    char buf[300] = {0};
    bool is_change = false;

    if (count >= 300) {
        AML_INFO("length error\n");
        return -ENOMEM;
    }

    if (copy_from_user(buf , user_buf, count)) {
        AML_INFO("copy_from_user fail\n");
        return -EFAULT;
    }

    if (strncmp(buf, "bypassdfs", strlen("bypassdfs")) == 0) {
        if ((buf[count - 2] == '1') && (aml_hw->customer_priv.dfs_on == false)) {
            aml_hw->customer_priv.dfs_on = true;
            is_change = true;
        }
        else if ((buf[count - 2] == '0') && (aml_hw->customer_priv.dfs_on == true)) {
            aml_hw->customer_priv.dfs_on = false;
            is_change = true;
        }
        else {
            AML_OUTPUT("bypassdfs info input error! or don't set again\n");
            return -EINVAL;
        }
    }

    if (is_change) {
        aml_send_me_chan_config_req(aml_hw);
    }

    return count;
}
AML_PROC_FILE_WR_OPS(driver);

static ssize_t aml_proc_country_read(struct file *file,
                                     char __user *user_buf,
                                     size_t count, loff_t *ppos)
{
    struct aml_hw *aml_hw = ((struct seq_file *)file->private_data)->private;
    char buf[200];
    int len = 0;
    ssize_t read;

    len += scnprintf(&buf[len], min_t(size_t, sizeof(buf) - 1, count),
                     "country:%c%c\n", aml_hw->customer_priv.alpha2[0], aml_hw->customer_priv.alpha2[1]);

    read = simple_read_from_buffer(user_buf, count, ppos, buf, len);

    return read;
}

static ssize_t aml_proc_country_write(struct file *file,
                                       const char __user *user_buf,
                                       size_t count, loff_t *ppos)
{
    struct aml_hw *aml_hw = ((struct seq_file *)file->private_data)->private;
    char buf[3] = {0};
    bool len = min_t(size_t, sizeof(buf), count);

    if (copy_from_user(buf , user_buf, len)) {
        AML_INFO("copy_from_user fail\n");
        return -EFAULT;
    }

    AML_INFO("set new country:%s\n", buf);
    // TODO set country

    return len;
}
AML_PROC_FILE_WR_OPS(country);

static ssize_t aml_proc_dbglevel_read(struct file *file,
                                     char __user *user_buf,
                                     size_t count, loff_t *ppos)
{
    struct aml_hw *aml_hw = ((struct seq_file *)file->private_data)->private;
    struct aml_vif *vif;
    char buf[30];
    int len = 0;
    ssize_t read;

    len += scnprintf(&buf[len], min_t(size_t, sizeof(buf) - 1, count),
                     "dbglevel:%d\n", aml_hw->customer_priv.dbg_level);

    read = simple_read_from_buffer(user_buf, count, ppos, buf, len);

    return read;
}

static ssize_t aml_proc_dbglevel_write(struct file *file,
                                       const char __user *user_buf,
                                       size_t count, loff_t *ppos)
{
    struct aml_hw *aml_hw = ((struct seq_file *)file->private_data)->private;
    char buf[32];
    int idx = 0;
    size_t len = min_t(size_t, count, sizeof(buf) - 1);

    if (copy_from_user(buf, user_buf, len))
        return -EFAULT;
    buf[len] = '\0';

#define AML_DBG_TOKEN(str, val)                                \
    if (strncmp(&buf[idx], str, sizeof(str) - 1) == 0) {        \
        idx += sizeof(str) - 1;                                 \
        dbg = val;                                              \
        goto dbg_done;                                          \
    }

    while ((idx + 4) < len) {
        if (strncmp(&buf[idx], "DBG:", 4) == 0) {
            u32 dbg = 0;
            idx += 4;
            AML_DBG_TOKEN("NONE", 0);
            AML_DBG_TOKEN("CRT",  1);
            AML_DBG_TOKEN("ERR",  2);
            AML_DBG_TOKEN("WRN",  3);
            AML_DBG_TOKEN("INF",  4);
            AML_DBG_TOKEN("VRB",  5);
            idx++;
            continue;
          dbg_done:
            aml_send_dbg_set_sev_filter_req(aml_hw, dbg);
        } else {
            idx++;
        }
    }

    return count;
}
AML_PROC_FILE_WR_OPS(dbglevel);

static ssize_t aml_proc_cfg_read(struct file *file,
                                     char __user *user_buf,
                                     size_t count, loff_t *ppos)
{
    struct aml_hw *aml_hw = ((struct seq_file *)file->private_data)->private;
    struct aml_vif *vif;
    struct aml_sta_stats *stats; // = &sta->stats;
    char buf[100];
    int len = 0;
    ssize_t read;

    // TODO : get driver capability or link information ?
    len += scnprintf(&buf[len], min_t(size_t, sizeof(buf) - 1, count),
                     "StaHTBfee|0\n");
    len += scnprintf(&buf[len], min_t(size_t, sizeof(buf) - 1, count),
                     "StaVHTBfee|%d\n", aml_hw->mod_params->bfmer);
    len += scnprintf(&buf[len], min_t(size_t, sizeof(buf) - 1, count),
                     "StaVHTMuBfee|%d\n", aml_hw->customer_priv.vht_mu_bfmee);
    len += scnprintf(&buf[len], min_t(size_t, sizeof(buf) - 1, count),
                     "Sta5gBw|%d\n", aml_hw->mod_params->use_80 ? 2 : (aml_hw->mod_params->use_2040 ? 1 : 0));
    len += scnprintf(&buf[len], min_t(size_t, sizeof(buf) - 1, count),
                     "TxRetryLimit|%d\n", aml_hw->customer_priv.retry_cnt);


    read = simple_read_from_buffer(user_buf, count, ppos, buf, len);

    return read;
}

static ssize_t aml_proc_cfg_write(struct file *file,
                                    const char __user *user_buf,
                                    size_t count, loff_t *ppos)
{
    struct aml_hw *aml_hw = ((struct seq_file *)file->private_data)->private;
    struct aml_vif *vif;
    struct aml_sta_stats *stats; // = &sta->stats;
    char buf[100] = {0};
    size_t len = min_t(size_t, count, sizeof(buf) - 1);
    ssize_t read;

    if (copy_from_user(buf, user_buf, len))
        return -EFAULT;

    if (strncmp(buf, "StaHTBfee", strlen("StaHTBfee")) == 0) {
        return count;
    }

    if (strncmp(buf, "StaVHTBfee", strlen("StaVHTBfee")) == 0) {
        if (buf[len - 2] == '1')
            aml_hw->mod_params->bfmee = true;
        else
            aml_hw->mod_params->bfmee = false;
        //aml_send_bfmer_enable(aml_hw, sta, &aml_hw->customer_priv.vht_capa);
    }
    if (strncmp(buf, "StaVHTMuBfee", strlen("StaVHTMuBfee")) == 0) {
        if (buf[len - 2] == '1')
            aml_hw->mod_params->bfmee = true;
        else
            aml_hw->mod_params->bfmee = false;
        //aml_send_bfmer_enable(aml_hw, sta, &aml_hw->customer_priv.vht_capa);
    }
    if (strncmp(buf, "Sta5gBw", strlen("Sta5gBw")) == 0) {
        if (buf[len - 2] == '2') {
            aml_hw->mod_params->use_80 = true;
            aml_hw->mod_params->use_2040 = true;
        } else if (buf[len - 2] == '1') {
            aml_hw->mod_params->use_80 = false;
            aml_hw->mod_params->use_2040 = true;
        } else {
            aml_hw->mod_params->use_80 = false;
            aml_hw->mod_params->use_2040 = false;
        }
    }
    if (strncmp(buf, "TxRetryLimit", strlen("TxRetryLimit")) == 0) {
        aml_hw->customer_priv.retry_cnt = (buf[len - 2] > 16) ? 16 : buf[len - 2];
        // TODO reinit rc
        // aml_send_sta_rc_reinit(aml_hw, staid, aml_hw->customer_priv.retry_cnt);
    }

    return read;
}
AML_PROC_FILE_WR_OPS(cfg);

static ssize_t aml_proc_disconnect_info_read(struct file *file,
                                     char __user *user_buf,
                                     size_t count, loff_t *ppos)
{
    struct aml_hw *aml_hw = ((struct seq_file *)file->private_data)->private;
    char buf[30];
    int len = 0;
    ssize_t read;

    len += scnprintf(&buf[len], min_t(size_t, sizeof(buf) - 1, count),
                                 "disconnect_info:%d\n", aml_hw->customer_priv.disconnect_reason_code);

    read = simple_read_from_buffer(user_buf, count, ppos, buf, len);

    return read;
}
AML_PROC_FILE_R_OPS(disconnect_info);

static ssize_t aml_proc_wow_reason_read(struct file *file,
                                     char __user *user_buf,
                                     size_t count, loff_t *ppos)
{
    struct aml_hw *aml_hw = ((struct seq_file *)file->private_data)->private;
    char buf[30];
    int len = 0;
    ssize_t read;

    len += scnprintf(&buf[len], min_t(size_t, sizeof(buf) - 1, count),
                                 "wow_reason:%d\n", aml_hw->customer_priv.wake_reason);

    read = simple_read_from_buffer(user_buf, count, ppos, buf, len);

    return read;
}
AML_PROC_FILE_R_OPS(wow_reason);

#define AML_PROC_HDL_TYPE_SEQ	0
#define AML_PROC_HDL_TYPE_SSEQ	1
#define AML_PROC_HDL_TYPE_SZSEQ	2

struct aml_proc_hdl {
    char *name;
    u8 type;
    union {
        int (*show)(struct seq_file *, void *);
        struct seq_operations *seq_op;
        struct {
            int (*show)(struct seq_file *, void *);
            size_t size;
        } sz;
    } u;
    ssize_t (*write)(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data);
};
#define AML_PROC_HDL_SSEQ(x) \
    { .name = #x, .type = AML_PROC_HDL_TYPE_SSEQ, .u.show = NULL, .write = NULL}

const struct aml_proc_hdl drv_proc_hdls[] = {
    AML_PROC_HDL_SSEQ(driver),
    AML_PROC_HDL_SSEQ(cfg),
    AML_PROC_HDL_SSEQ(country),
    AML_PROC_HDL_SSEQ(dbglevel),
    AML_PROC_HDL_SSEQ(disconnect_info),
    AML_PROC_HDL_SSEQ(drv_state),
 //   AML_PROC_HDL_SSEQ(AML_RVRINFO_NAME, rvrinfoRead, NULL),
 //   AML_PROC_HDL_SSEQ(AML_SCANPARAM_NAME, scanparamRead, NULL),
};

static int proc_get_dummy(struct seq_file *m, void *v)
{
    printk("aml wifi proc_get_dummy\n");
    return 0;
}

static int aml_drv_proc_open(struct inode *inode, struct file *file)
{
    ssize_t index = (ssize_t)PDE_DATA(inode);
    const struct aml_proc_hdl *hdl = drv_proc_hdls + index;
    void *private = PDE_DATA(inode);

    if (hdl->type == AML_PROC_HDL_TYPE_SEQ) {
        int res = seq_open(file, hdl->u.seq_op);

        if (res == 0)
            ((struct seq_file *)file->private_data)->private = private;

        return res;
    } else if (hdl->type == AML_PROC_HDL_TYPE_SSEQ) {
        int (*show)(struct seq_file *, void *) = hdl->u.show ? hdl->u.show : proc_get_dummy;

        return single_open(file, NULL, private);
    } else if (hdl->type == AML_PROC_HDL_TYPE_SZSEQ) {
        int (*show)(struct seq_file *, void *) = hdl->u.sz.show ? hdl->u.sz.show : proc_get_dummy;
        #if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0))
        return single_open_size(file, show, private, hdl->u.sz.size);
        #else
        return single_open(file, show, private);
        #endif
    } else {
        return -EROFS;
    }
}

void aml_destroy_proc_dir(struct aml_hw *aml_hw)
{
    if (aml_hw->customer_priv.proc_dir)
        proc_remove(aml_hw->customer_priv.proc_dir);
}

int32_t aml_create_proc_dir(struct aml_hw *aml_hw)
{
    struct proc_dir_entry *aml_proc;
    struct proc_dir_entry *proc_dir = aml_hw->customer_priv.proc_dir;
    umode_t mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH;
    umode_t read_mode = S_IRUSR | S_IRGRP | S_IROTH;

    AML_PROC_ADD_FILE(cfg, proc_dir, mode);
    AML_PROC_ADD_FILE(driver, proc_dir, mode);
    AML_PROC_ADD_FILE(dbglevel, proc_dir, mode);
    AML_PROC_ADD_FILE(drv_state, proc_dir, read_mode);
    AML_PROC_ADD_FILE(country, proc_dir, mode);
    AML_PROC_ADD_FILE(disconnect_info, proc_dir, read_mode);
    //AML_PROC_ADD_FILE(AML_RVRINFO_NAME, proc_dir, mode);
    //AML_PROC_ADD_FILE(AML_SCANPARAM_NAME, proc_dir, mode);
    AML_PROC_ADD_FILE(wow_reason, proc_dir, read_mode);
    return 0;

err:
    AML_INFO("create proc error\n");
    aml_destroy_proc_dir(aml_hw);
    return -1;
}

extern struct aml_hw *g_aml_hw;
#else

#define AML_PROC_FILE_WR_OPS(name)                          \
    PROC_FILE_READ_FUNC(name);                              \
    PROC_FILE_WRITE_FUNC(name);                             \
static const struct file_operations aml_proc_##name##_ops = {  \
    .owner = THIS_MODULE,                                   \
    .read = aml_proc_##name##_read,                         \
    .write = aml_proc_##name##_write,                       \
};

#define AML_PROC_FILE_R_OPS(name)                           \
    PROC_FILE_READ_FUNC(name);                              \
static const struct file_operations aml_proc_##name##_ops = {  \
    .owner = THIS_MODULE,                                   \
    .read = aml_proc_##name##_read,                         \
};

#define AML_PROC_ADD_FILE(name, parent, mode) do {                          \
        struct proc_dir_entry *__tmp;                                       \
        __tmp = proc_create(#name, mode, parent, &aml_proc_##name##_ops);   \
        if (IS_ERR_OR_NULL(__tmp))                                          \
            goto err;                                                       \
    } while (0)

#define SYSFS_RO_FILE_OPS(name) DEVICE_ATTR(name, S_IRUGO, aml_sysfs_##name##_read, NULL)
#define SYSFS_WO_FILE_OPS(name) DEVICE_ATTR(name, S_IWUSR|S_IWGRP, NULL, aml_sysfs_##name##_write)
#define SYSFS_RW_FILE_OPS(name)     \
    DEVICE_ATTR(name, S_IWUSR|S_IWGRP|S_IRUGO, aml_sysfs_##name##_read, aml_sysfs_##name##_write)

extern struct aml_hw *g_aml_hw;
extern struct aml_pm_type g_wifi_pm;
static ssize_t aml_proc_drv_state_read(struct file *file,
                                       char __user *user_buf,
                                       size_t count, loff_t *ppos)
{
    struct aml_hw *aml_hw = g_aml_hw;   //file->private_data;
    int i;
    char buf[20];
    int len = 0;
    int max_len = min_t(size_t, sizeof(buf) - 1, count);
    ssize_t read;

    if (!atomic_read(&g_wifi_pm.wifi_enable)) {
        len += scnprintf(&buf[len], max_len - len, "not_ready\n");
    }
#ifdef CONFIG_AML_RECOVERY
    else if (aml_recy_flags_chk(AML_RECY_STATE_ONGOING)) {
        len += scnprintf(&buf[len], max_len - len, "recovery\n");
    }
#endif
    else {
        len += scnprintf(&buf[len], max_len - len, "ready\n");
    }

    read = simple_read_from_buffer(user_buf, count, ppos, buf, len);

    return read;
}
AML_PROC_FILE_R_OPS(drv_state);

static ssize_t aml_proc_driver_read(struct file *file,
                                     char __user *user_buf,
                                     size_t count, loff_t *ppos)
{
    struct aml_hw *aml_hw = g_aml_hw;   //file->private_data;
    struct wiphy *wiphy = aml_hw->wiphy;
    int i;
    char buf[1000];
    char dfs_buf[200] = {0};
    int max_len = min_t(size_t, sizeof(buf) - 1, count);
    int len = 0;
    int dfs_buf_len = 0;
    ssize_t read;
    int bw;

    len += scnprintf(&buf[len], max_len - len,
                     "drv_version:%s\n", 1);   //roku to do

    len += scnprintf(&buf[len], max_len - len,
                     "country:%c%c\n", aml_hw->customer_priv.alpha2[0], aml_hw->customer_priv.alpha2[1]);

    if (wiphy->bands[NL80211_BAND_2GHZ] != NULL) {
        struct ieee80211_supported_band *b = wiphy->bands[NL80211_BAND_2GHZ];
        for (i = 0; i < b->n_channels; i++) {
            if (b->channels[i].flags & IEEE80211_CHAN_RADAR) {
                dfs_buf_len += scnprintf(&dfs_buf[dfs_buf_len], sizeof(dfs_buf) - dfs_buf_len,
                                 "%d,", ieee80211_frequency_to_channel(b->channels[i].center_freq));
            }
            else {
                bw = 160;
                if (b->channels[i].flags & IEEE80211_CHAN_NO_160MHZ)
                    bw = 80;
                if (b->channels[i].flags & IEEE80211_CHAN_NO_80MHZ)
                    bw = 40;
                if (b->channels[i].flags &
                    (IEEE80211_CHAN_NO_HT40PLUS | IEEE80211_CHAN_NO_HT40MINUS))
                    bw = 20;
                if (bw > 80)
                    bw = 40;
                if ((b->channels[i].flags & IEEE80211_CHAN_DISABLED) != IEEE80211_CHAN_DISABLED)
                    len += scnprintf(&buf[len], max_len - len, "CH-%d:\tBW_%dMHz\t(flag=0x%x)\n",
                                     ieee80211_frequency_to_channel(b->channels[i].center_freq),
                                     bw, b->channels[i].flags);
            }
        }
    }

    if (wiphy->bands[NL80211_BAND_5GHZ] != NULL) {
        struct ieee80211_supported_band *b = wiphy->bands[NL80211_BAND_5GHZ];
        for (i = 0; i < b->n_channels; i++) {
            if (b->channels[i].flags & IEEE80211_CHAN_RADAR) {
                dfs_buf_len += scnprintf(&dfs_buf[dfs_buf_len], sizeof(dfs_buf) - dfs_buf_len,
                                 "%d,", ieee80211_frequency_to_channel(b->channels[i].center_freq));
            }
            else {
                bw = 160;
                if (b->channels[i].flags & IEEE80211_CHAN_NO_160MHZ)
                    bw = 80;
                if (b->channels[i].flags & IEEE80211_CHAN_NO_80MHZ)
                    bw = 40;
                if (b->channels[i].flags &
                    (IEEE80211_CHAN_NO_HT40PLUS | IEEE80211_CHAN_NO_HT40MINUS))
                    bw = 20;
                if (bw > 80)
                    bw = 80;
                if ((b->channels[i].flags & IEEE80211_CHAN_DISABLED) != IEEE80211_CHAN_DISABLED)
                    len += scnprintf(&buf[len], max_len - len, "CH-%d:\tBW_%dMHz\t(flag=0x%x)\n",
                                     ieee80211_frequency_to_channel(b->channels[i].center_freq),
                                     bw, b->channels[i].flags);
            }
        }
    }

    len += scnprintf(&buf[len], max_len - len,
                     "bypassdfs:%d\n", aml_hw->customer_priv.dfs_on);

    if (aml_hw->customer_priv.dfs_on) {
        int tmp = strlen(dfs_buf);
        if (tmp > 0) {
            dfs_buf[tmp - 1] = '\n';    // '\n' replace ','
            len += scnprintf(&buf[len], max_len - len, "DFS channel:");
            len += scnprintf(&buf[len], max_len - len, "%s\n", dfs_buf);
        }
    }

    read = simple_read_from_buffer(user_buf, count, ppos, buf, len);

    return read;
}

static ssize_t aml_proc_driver_write(struct file *file,
                                     const char __user *user_buf,
                                     size_t count, loff_t *ppos)
{
    struct aml_hw *aml_hw = g_aml_hw; //file->private_data;
    char buf[300] = {0};
    bool is_change = false;

    if (count >= 300) {
        AML_INFO("length error\n");
        return -ENOMEM;
    }

    if (copy_from_user(buf , user_buf, count)) {
        AML_INFO("copy_from_user fail\n");
        return -EFAULT;
    }

    if (strncmp(buf, "bypassdfs", strlen("bypassdfs")) == 0) {
        if ((buf[count - 2] == '1') && (aml_hw->customer_priv.dfs_on == false)) {
            aml_hw->customer_priv.dfs_on = true;
            is_change = true;
        }
        else if ((buf[count - 2] == '0') && (aml_hw->customer_priv.dfs_on == true)) {
            aml_hw->customer_priv.dfs_on = false;
            is_change = true;
        }
        else {
            AML_OUTPUT("bypassdfs info input error! or don't set again\n");
            return -EINVAL;
        }
    }

    if (is_change) {
        aml_send_me_chan_config_req(aml_hw);
    }

    return count;
}
AML_PROC_FILE_WR_OPS(driver);

static ssize_t aml_proc_country_read(struct file *file,
                                     char __user *user_buf,
                                     size_t count, loff_t *ppos)
{
    struct aml_hw *aml_hw = g_aml_hw; //file->private_data;
    char buf[200];
    int len = 0;
    ssize_t read;

    len += scnprintf(&buf[len], min_t(size_t, sizeof(buf) - 1, count),
                     "country:%c%c\n", aml_hw->customer_priv.alpha2[0], aml_hw->customer_priv.alpha2[1]);

    read = simple_read_from_buffer(user_buf, count, ppos, buf, len);

    return read;
}

static ssize_t aml_proc_country_write(struct file *file,
                                       const char __user *user_buf,
                                       size_t count, loff_t *ppos)
{
    struct aml_hw *aml_hw = g_aml_hw;
    char buf[3] = {0};
    int len = min_t(size_t, sizeof(buf), count);

    if (copy_from_user(buf , user_buf, len)) {
        AML_INFO("copy_from_user fail\n");
        return -EFAULT;
    }

    AML_INFO("set new country:%s\n", buf);

    buf[2] = '\0';

    aml_apply_regdom(aml_hw, aml_hw->wiphy, buf);
    aml_send_me_chan_config_req(aml_hw);

    return len;
}
AML_PROC_FILE_WR_OPS(country);

static ssize_t aml_proc_dbglevel_read(struct file *file,
                                     char __user *user_buf,
                                     size_t count, loff_t *ppos)
{
    struct aml_hw *aml_hw = g_aml_hw; //file->private_data;
    struct aml_vif *vif;
    char buf[30];
    int len = 0;
    ssize_t read;

    len += scnprintf(&buf[len], min_t(size_t, sizeof(buf) - 1, count),
                     "dbglevel:%d\n", aml_hw->customer_priv.dbg_level);

    read = simple_read_from_buffer(user_buf, count, ppos, buf, len);

    return read;
}

static ssize_t aml_proc_dbglevel_write(struct file *file,
                                       const char __user *user_buf,
                                       size_t count, loff_t *ppos)
{
    struct aml_hw *priv = g_aml_hw; //file->private_data;
    char buf[32];
    int idx = 0;
    size_t len = min_t(size_t, count, sizeof(buf) - 1);

    if (copy_from_user(buf, user_buf, len))
        return -EFAULT;
    buf[len] = '\0';

#define AML_DBG_TOKEN(str, val)                                \
    if (strncmp(&buf[idx], str, sizeof(str) - 1) == 0) {        \
        idx += sizeof(str) - 1;                                 \
        dbg = val;                                              \
        goto dbg_done;                                          \
    }

    while ((idx + 4) < len) {
        if (strncmp(&buf[idx], "DBG:", 4) == 0) {
            u32 dbg = 0;
            idx += 4;
            AML_DBG_TOKEN("NONE", 0);
            AML_DBG_TOKEN("CRT",  1);
            AML_DBG_TOKEN("ERR",  2);
            AML_DBG_TOKEN("WRN",  3);
            AML_DBG_TOKEN("INF",  4);
            AML_DBG_TOKEN("VRB",  5);
            idx++;
            continue;
          dbg_done:
            aml_send_dbg_set_sev_filter_req(priv, dbg);
        } else {
            idx++;
        }
    }

    return count;
}
AML_PROC_FILE_WR_OPS(dbglevel);

static ssize_t aml_proc_cfg_read(struct file *file,
                                     char __user *user_buf,
                                     size_t count, loff_t *ppos)
{
    struct aml_hw *aml_hw = g_aml_hw; //file->private_data;
    struct aml_vif *vif;
    struct aml_sta_stats *stats; // = &sta->stats;
    char buf[100];
    int len = 0;
    ssize_t read;

    // TODO : get driver capability or link information ?
    len += scnprintf(&buf[len], min_t(size_t, sizeof(buf) - 1, count),
                     "StaHTBfee|0\n");
    len += scnprintf(&buf[len], min_t(size_t, sizeof(buf) - 1, count),
                     "StaVHTBfee|%d\n", aml_hw->mod_params->bfmer);
    len += scnprintf(&buf[len], min_t(size_t, sizeof(buf) - 1, count),
                     "StaVHTMuBfee|%d\n", aml_hw->customer_priv.vht_mu_bfmee);
    len += scnprintf(&buf[len], min_t(size_t, sizeof(buf) - 1, count),
                     "Sta5gBw|%d\n", aml_hw->mod_params->use_80 ? 2 : (aml_hw->mod_params->use_2040 ? 1 : 0));
    len += scnprintf(&buf[len], min_t(size_t, sizeof(buf) - 1, count),
                     "TxRetryLimit|%d\n", aml_hw->customer_priv.retry_cnt);


    read = simple_read_from_buffer(user_buf, count, ppos, buf, len);

    return read;
}

size_t aml_get_value_from_str(unsigned char *str, size_t len, int base, u8 *out)
{
    unsigned char *endptr;
    unsigned char *startstr = str;
    unsigned long result;

    while (len) {
        if (isspace(*str)) {
            str++;
            len--;
            continue;
        }
        else
            break;
    }
    printk("aml_get_value_from_str:%s\n", str);
    result = simple_strtoul(str, &endptr, base);

    *out = result;

    printk("aml_get_value_from_str:%s, value:%d, result:%d\n", str, *out, result);

    return endptr - startstr;
}

static ssize_t aml_proc_cfg_write(struct file *file,
                                    const char __user *user_buf,
                                    size_t count, loff_t *ppos)
{
    struct aml_hw *aml_hw = g_aml_hw; //file->private_data;
    struct aml_vif *vif;
    struct aml_sta_stats *stats; // = &sta->stats;
    char buf[100] = {0};
    size_t len = min_t(size_t, count, sizeof(buf) - 1);
    ssize_t read;
    u8 retry = 0xff;
    u8 bfmee = 0xff;
    u8 bw = 0xff;
    char *pos = buf;
    char *endptr = buf + len;

    if (copy_from_user(buf, user_buf, len))
        return -EFAULT;

    if (strncmp(buf, "StaHTBfee", strlen("StaHTBfee")) == 0) {
        return count;
    }

    while (len) {
        if (isspace(*pos)) {
            pos++;
            len--;
            continue;
        }
        if (strncmp(pos, "StaVHTMuBfee", strlen("StaVHTMuBfee")) == 0) {
            int offset = strlen("StaVHTMuBfee");
            size_t add_len;
            pos += offset;
            add_len = aml_get_value_from_str(pos, endptr - pos, 10, &bfmee);     // 10 is decimalism
            if (bfmee == 1)
                aml_hw->mod_params->bfmee = true;
            else if (bfmee == 0)
                aml_hw->mod_params->bfmee = false;
            else {
                printk("error param %d : %s, expect: 1 or 0, actuality: %d,\n", __LINE__, pos, bfmee);
                return -EINVAL;
            }
            pos += add_len;
            len -= (offset + add_len);
        }
        else if (strncmp(pos, "StaVHTBfee", strlen("StaVHTBfee")) == 0) {
            int offset = strlen("StaVHTBfee");
            size_t add_len;
            pos += offset;
            add_len = aml_get_value_from_str(pos, endptr - pos, 10, &bfmee);     // 10 is decimalism
            if (bfmee == 1)
                aml_hw->mod_params->bfmee = true;
            else if (bfmee == 0)
                aml_hw->mod_params->bfmee = false;
            else {
                printk("error param %d : %s, expect: 1 or 0, actuality: %d,\n", __LINE__, pos, bfmee);
                return -EINVAL;
            }
            pos += add_len;
            len -= (offset + add_len);
        }
        else if (strncmp(pos, "Sta5gBw", strlen("Sta5gBw")) == 0) {
            int offset = strlen("Sta5gBw");
            size_t add_len;
            pos += offset;
            add_len= aml_get_value_from_str(pos, endptr - pos, 10, &bw);     // 10 is decimalism
            if (bw == 2) {
                aml_hw->mod_params->use_80 = true;
                aml_hw->mod_params->use_2040 = true;
            } else if (bw == 1) {
                aml_hw->mod_params->use_80 = false;
                aml_hw->mod_params->use_2040 = true;
            } else if (bw == 0) {
                aml_hw->mod_params->use_80 = false;
                aml_hw->mod_params->use_2040 = false;
            } else {
                printk("error param %d : %s, expect: 0 or 1 or 2, actuality: %d,\n", __LINE__, pos, bw);
                return -EINVAL;
            }
            pos += add_len;
            len -= (offset + add_len);
        }
        else if (strncmp(pos, "TxRetryLimit", strlen("TxRetryLimit")) == 0) {
            int offset = strlen("TxRetryLimit");
            size_t add_len;
            pos += offset;
            add_len = aml_get_value_from_str(pos, endptr - pos, 10, &retry);     // 10 is decimalism
            if (aml_hw->customer_priv.retry_cnt == retry) {
                AML_INFO("TxRetryLimit has already be :%d\n", retry);
                retry = 0xff;
            } else {
                if (retry >= 32) {
                    AML_INFO("TxRetryLimit should litter than 32\n");
                    retry = 0xff;
                }
                else
                    aml_hw->customer_priv.retry_cnt = retry;
            }
            pos += add_len;
            len -= (offset + add_len);
        }
        else{
            int error_len = strlen(pos);
            printk("error param %d : %s\n", __LINE__, pos);
            printk("expect:"    \
                   "echo \"StaVHTBfee 0|1 StaVHTMuBfee 0|1 Sta5gBw 0|1|2 TxRetryLimit [0~15]\" > cfg\n");
            pos += error_len;
            len -= error_len;
        }
    }
    printk("retry:%d bfmee:%d, bw:%d, buf:%s\n", retry, bfmee, bw, buf);

    if (aml_send_cfg_req(aml_hw, bfmee, bfmee, bw, retry) == 0) {
        aml_hw->customer_priv.retry_cnt = retry;
    }

    return count;
}
AML_PROC_FILE_WR_OPS(cfg);

static ssize_t aml_proc_disconnect_info_read(struct file *file,
                                     char __user *user_buf,
                                     size_t count, loff_t *ppos)
{
    struct aml_hw *aml_hw = g_aml_hw; //file->private_data;
    char buf[30];
    int len = 0;
    ssize_t read;

    len += scnprintf(&buf[len], min_t(size_t, sizeof(buf) - 1, count),
                                 "disconnect_info:%d\n", aml_hw->customer_priv.disconnect_reason_code);

    read = simple_read_from_buffer(user_buf, count, ppos, buf, len);

    return read;
}
AML_PROC_FILE_R_OPS(disconnect_info);

static ssize_t aml_proc_wow_reason_read(struct file *file,
                                     char __user *user_buf,
                                     size_t count, loff_t *ppos)
{
    struct aml_hw *aml_hw = g_aml_hw; //file->private_data;
    char buf[30];
    int len = 0;
    ssize_t read;

    if (aml_hw->customer_priv.wake_reason != 0xff)
        len += scnprintf(&buf[len], min_t(size_t, sizeof(buf) - 1, count),
                         "wow_reason:%d\n", aml_hw->customer_priv.wake_reason);

    read = simple_read_from_buffer(user_buf, count, ppos, buf, len);

    return read;
}
AML_PROC_FILE_R_OPS(wow_reason);

static ssize_t aml_proc_rvrinfo_Read(struct file *file, char __user *user_buf,
                                           size_t count, loff_t *ppos)
{
    struct aml_hw *aml_hw = g_aml_hw; //file->private_data;
    struct aml_vif *aml_vif;
    char buf[200];
    int len = 0;
    int buf_size = sizeof(buf);
    ssize_t read;
    uint32_t reg_data;

    list_for_each_entry(aml_vif, &aml_hw->vifs, list) {
        if (aml_vif->up && (aml_vif->ndev != NULL) && (AML_VIF_TYPE(aml_vif) == NL80211_IFTYPE_STATION)) {
            if (aml_vif->sta.ap != NULL) {
                struct aml_sta *sta = aml_vif->sta.ap;
                uint16_t idx;
                struct rx_vector_1 *last_rx;
                struct me_rc_stats_cfm me_rc_stats_cfm;
                unsigned int fmt, pre, bw, nss, mcs, gi, dcm = 0;

                reg_data = AML_REG_READ(aml_hw->plat, AML_ADDR_MAC_PHY, REG_OF_SYNC_RSSI); // & 0xffff0000) >> 16) - 256)
                len += scnprintf(&buf[len], min_t(size_t, buf_size - len - 1, count),
                    "avg_rssi:%d\n", ((reg_data & 0xffff0000) >> 16) - 256);
                len += scnprintf(&buf[len], min_t(size_t, buf_size - len - 1, count),
                    "avg_bcn_rssi:%d\n", reg_data & 0x0000ffff);
                len += scnprintf(&buf[len], min_t(size_t, buf_size - len - 1, count),
                    "avg_snr:%d\n", AML_REG_READ(aml_hw->plat, AML_ADDR_MAC_PHY, REG_OF_SYNC_SNR) & 0xffff);
                len += scnprintf(&buf[len], min_t(size_t, buf_size - len - 1, count),
                    "snr_qdb:%d\n", 0);   // TODO
                len += scnprintf(&buf[len], min_t(size_t, buf_size - len - 1, count),
                    "noise_f:%d\n", 0);   // TODO

                // get txrate
                if (0 == aml_send_me_rc_stats(aml_hw, sta->sta_idx, &me_rc_stats_cfm)) {
                    len += scnprintf(&buf[len], min_t(size_t, buf_size - len - 1, count), "txRate:");
                    idx = me_rc_stats_cfm.retry_step_idx[me_rc_stats_cfm.sw_retry_step];
                    len += print_rate_from_cfg(&buf[len], min_t(size_t, buf_size - len - 1, count),
                                              me_rc_stats_cfm.rate_stats[idx].rate_config, NULL, 0, 0);
                    len += scnprintf(&buf[len], min_t(size_t, buf_size - len - 1, count), "\n");
                }

                // get rxrate
                len += scnprintf(&buf[len], min_t(size_t, buf_size - len - 1, count), "rxRate:");
                last_rx = &sta->stats.last_rx.rx_vect1;
                fmt = last_rx->format_mod;
                bw = last_rx->ch_bw;
                pre = last_rx->pre_type;
                if (fmt >= FORMATMOD_HE_SU) {
                    mcs = last_rx->he.mcs;
                    nss = last_rx->he.nss;
                    gi = last_rx->he.gi_type;
                    if ((fmt == FORMATMOD_HE_MU) || (fmt == FORMATMOD_HE_ER))
                        bw = last_rx->he.ru_size;
                    dcm = last_rx->he.dcm;
                } else if (fmt == FORMATMOD_VHT) {
                    mcs = last_rx->vht.mcs;
                    nss = last_rx->vht.nss;
                    gi = last_rx->vht.short_gi;
                } else if (fmt >= FORMATMOD_HT_MF) {
                    mcs = last_rx->ht.mcs % 8;
                    nss = last_rx->ht.mcs / 8;
                    gi = last_rx->ht.short_gi;
                } else {
                    BUG_ON((mcs = legrates_lut[last_rx->leg_rate].idx) == -1);
                    nss = 0;
                    gi = 0;
                }
                len += print_rate(&buf[len], 30, fmt, nss, mcs, bw, gi, pre, dcm, NULL, 0);
                len += scnprintf(&buf[len], min_t(size_t, buf_size - len - 1, count), "\n");

                len += scnprintf(&buf[len], min_t(size_t, buf_size - len - 1, count),
                    "BW:%dMHz\n", sta->stats.bw_max);
            }
        }
    }

    read = simple_read_from_buffer(user_buf, count, ppos, buf, len);

    return read;
}
AML_PROC_FILE_R_OPS(rvrinfo);

void aml_destroy_proc_dir(struct aml_hw *aml_hw)
{
    if (aml_hw->customer_priv.proc_dir)
        proc_remove(aml_hw->customer_priv.proc_dir);
}

int32_t aml_create_proc_dir(struct aml_hw *aml_hw)
{
    struct proc_dir_entry *aml_proc;
    struct proc_dir_entry *proc_dir = aml_hw->customer_priv.proc_dir;
    umode_t mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH;
    umode_t read_mode = S_IRUSR | S_IRGRP | S_IROTH;

    AML_PROC_ADD_FILE(cfg, proc_dir, mode);
    AML_PROC_ADD_FILE(driver, proc_dir, mode);
    AML_PROC_ADD_FILE(dbglevel, proc_dir, mode);
    AML_PROC_ADD_FILE(drv_state, proc_dir, read_mode);
    AML_PROC_ADD_FILE(country, proc_dir, mode);
    AML_PROC_ADD_FILE(disconnect_info, proc_dir, read_mode);
    //AML_PROC_ADD_FILE(AML_RVRINFO_NAME, proc_dir, mode);
    //AML_PROC_ADD_FILE(AML_SCANPARAM_NAME, proc_dir, mode);
    AML_PROC_ADD_FILE(wow_reason, proc_dir, read_mode);
    return 0;

err:
    AML_INFO("create proc error\n");
    aml_destroy_proc_dir(aml_hw);
    return -1;
}
#endif

static ssize_t aml_sysfs_hal_spec_read(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct net_device *ndev = container_of(dev, struct net_device, dev);
    struct aml_vif *vif = netdev_priv(ndev);
    struct aml_hw *aml_hw = vif->aml_hw;
    u32_l phy_feat = aml_hw->version_cfm.version_phy_1;
    u32_l sys_feat = aml_hw->version_cfm.features;
    int bw = (phy_feat & MDM_CHBW_MASK) >> MDM_CHBW_LSB;
    int len = 0;

    len += sprintf(buf, "Tx_Nss:%d\n", aml_hw->mod_params->nss);
    len += sprintf(&buf[len], "Rx_Nss:%d\n", aml_hw->mod_params->nss);

    // Check supported BW
    bw = (phy_feat & MDM_CHBW_MASK) >> MDM_CHBW_LSB;
    // Check if 80MHz BW is supported
    if (bw == 2) {
        len += sprintf(&buf[len], "2gBW:40M\n");
        len += sprintf(&buf[len], "5gBW:80M\n");
    }
    else if (bw == 1) {
        len += sprintf(&buf[len], "2gBW:40M\n");
        len += sprintf(&buf[len], "5gBW:40M\n");
    }
    else {
        len += sprintf(&buf[len], "2gBW:20M\n");
        len += sprintf(&buf[len], "5gBW:20M\n");
    }

    // Check if HE is supported
    if (sys_feat & BIT(MM_FEAT_HE_BIT)) {
        len += sprintf(&buf[len], "max_proto:ax\n");
    }
    else if (sys_feat & BIT(MM_FEAT_VHT_BIT)) {
        len += sprintf(&buf[len], "max_proto:ac\n");
    }
    else {
        len += sprintf(&buf[len], "max_proto:n\n");
    }

    return len;
}
static SYSFS_RO_FILE_OPS(hal_spec);

static ssize_t aml_sysfs_cur_channel_read(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct net_device *ndev = container_of(dev, struct net_device, dev);
    struct aml_vif *vif = netdev_priv(ndev);
    struct aml_hw *aml_hw = vif->aml_hw;
    int len = 0;

    if (vif->up && (vif->ch_index != AML_CH_NOT_SET) && (AML_VIF_TYPE(vif) == NL80211_IFTYPE_STATION))
        len += sprintf(&buf[len], "cur_channel :%d\n",
                     ieee80211_frequency_to_channel(vif->sta.ap->center_freq)); // prim20
    else
        len += sprintf(&buf[len], "dev:%s channel:error\n", vif->ndev->name);

    return len;
}
static SYSFS_RO_FILE_OPS(cur_channel);

static ssize_t aml_sysfs_bandwidth_read(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct net_device *ndev = container_of(dev, struct net_device, dev);
    struct aml_vif *vif = netdev_priv(ndev);
    struct aml_hw *aml_hw = vif->aml_hw;
    int len = 0;
    const char bw_info[3][4] = {"20M", "40M", "80M"};

    if (vif->up && (vif->ch_index != AML_CH_NOT_SET) && (AML_VIF_TYPE(vif) == NL80211_IFTYPE_STATION) &&
        ((aml_hw->chanctx_table[vif->ch_index].chan_def.width - NL80211_CHAN_WIDTH_20) < 3))
    {
        len += sprintf(&buf[len], "bandwidth :%s\n",
                       bw_info[aml_hw->chanctx_table[vif->ch_index].chan_def.width - NL80211_CHAN_WIDTH_20]);
    }
    else
        len += sprintf(&buf[len], "bandwidth :error\n");

    return len;
}
static SYSFS_RO_FILE_OPS(bandwidth);

static ssize_t aml_sysfs_rRssi_read(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct net_device *ndev = container_of(dev, struct net_device, dev);
    struct aml_vif *vif = netdev_priv(ndev);
    struct aml_hw *aml_hw = vif->aml_hw;
    int len = 0;

    len += sprintf(&buf[len], "Rssi :%d\n", ((AML_REG_READ(aml_hw->plat,
                   AML_ADDR_MAC_PHY, REG_OF_SYNC_RSSI) & 0xffff0000) >> 16) - 256);

    return len;
}
static SYSFS_RO_FILE_OPS(rRssi);

static ssize_t aml_sysfs_iNoise_read(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct net_device *ndev = container_of(dev, struct net_device, dev);
    struct aml_vif *vif = netdev_priv(ndev);
    struct aml_hw *aml_hw = vif->aml_hw;
    int len = 0;
    u32 agc_state;
    u32 agc1, agc2;
    s32 noise1, noise2;

    // W2 donot support, W2L support
    agc_state = AML_REG_READ(aml_hw->plat, AML_ADDR_SYSTEM, 0xc0b7d4);
    AML_REG_WRITE(0x29, aml_hw->plat, AML_ADDR_SYSTEM, 0xc0b7d4);
    agc1 = AML_REG_READ(aml_hw->plat, AML_ADDR_SYSTEM, 0xc0b7d8);
    agc2 = AML_REG_READ(aml_hw->plat, AML_ADDR_SYSTEM, 0xc0b7dc);
    AML_REG_WRITE(agc_state, aml_hw->plat, AML_ADDR_SYSTEM, 0xc0b7d4);

    noise1 = (agc1 >> 21) - 2048;
    noise2 = (agc2 >> 21) - 2048;
    len += sprintf(&buf[len], "Noise :%d\n", (noise1 + noise2) / 2);

    return len;
}
static SYSFS_RO_FILE_OPS(iNoise);

static ssize_t aml_sysfs_iSnr_read(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct net_device *ndev = container_of(dev, struct net_device, dev);
    struct aml_vif *vif = netdev_priv(ndev);
    struct aml_hw *aml_hw = vif->aml_hw;
    int len = 0;

    len += sprintf(&buf[len], "Snr :%d\n",
                   AML_REG_READ(aml_hw->plat, AML_ADDR_MAC_PHY, REG_OF_SYNC_SNR) & 0xffff);

    return len;
}
static SYSFS_RO_FILE_OPS(iSnr);

static ssize_t aml_sysfs_ssid_read(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct net_device *ndev = container_of(dev, struct net_device, dev);
    struct aml_vif *vif = netdev_priv(ndev);
    struct aml_hw *aml_hw = vif->aml_hw;
    int len = 0;

    if (vif->up && (vif->sta.ap != NULL) && (AML_VIF_TYPE(vif) == NL80211_IFTYPE_STATION))
        len += sprintf(&buf[len], "SSID:%s\n", vif->sta.assoc_ssid);

    return len;
}
static SYSFS_RO_FILE_OPS(ssid);

static ssize_t aml_sysfs_bssid_read(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct net_device *ndev = container_of(dev, struct net_device, dev);
    struct aml_vif *vif = netdev_priv(ndev);
    int len = 0;

    if (vif->up && (vif->sta.ap != NULL) && (AML_VIF_TYPE(vif) == NL80211_IFTYPE_STATION))
        len += sprintf(&buf[len], "BSSID:"MACFMT"\n", MACARG(vif->sta.ap->mac_addr));

    return len;
}
static SYSFS_RO_FILE_OPS(bssid);

static ssize_t aml_sysfs_TxPktNum_read(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct net_device *ndev = container_of(dev, struct net_device, dev);
    struct aml_vif *vif = netdev_priv(ndev);
    int len = 0;

    if (vif->up && (vif->sta.ap != NULL) &&
        (AML_VIF_TYPE(vif) == NL80211_IFTYPE_STATION)) {
        len += sprintf(&buf[len], "TxPktNum :%d\n", vif->sta.ap->stats.tx_pkts);
    }

    return len;
}
static SYSFS_RO_FILE_OPS(TxPktNum);

static ssize_t aml_sysfs_TxFailNum_read(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct net_device *ndev = container_of(dev, struct net_device, dev);
    struct aml_vif *vif = netdev_priv(ndev);
    int len = 0;

    if (vif->up && (vif->sta.ap != NULL) &&
        (AML_VIF_TYPE(vif) == NL80211_IFTYPE_STATION)) {
        len += sprintf(&buf[len], "TxFailNum :%d\n", vif->sta.ap->stats.tx_fails);
    }

    return len;
}
static SYSFS_RO_FILE_OPS(TxFailNum);


static ssize_t aml_sysfs_TxRate_read(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct net_device *ndev = container_of(dev, struct net_device, dev);
    struct aml_vif *vif = netdev_priv(ndev);
    struct aml_hw *aml_hw = vif->aml_hw;
    struct me_rc_stats_cfm me_rc_stats_cfm;
    int len = 0;
    int error = 0;
    int idx;

    if (vif->up && (vif->sta.ap != NULL) &&
        (AML_VIF_TYPE(vif) == NL80211_IFTYPE_STATION)) {
        // it can refer to aml_tx_statistic
        if ((error = aml_send_me_rc_stats(aml_hw, vif->sta.ap->sta_idx, &me_rc_stats_cfm)))
            return 0;

        idx = me_rc_stats_cfm.retry_step_idx[me_rc_stats_cfm.sw_retry_step];
        len = print_rate_from_cfg(&buf[len], 100,    // TODO 30
                              me_rc_stats_cfm.rate_stats[idx].rate_config, NULL, 0, 0);
    }

    len += sprintf(&buf[len], "\n");

    return len;
}
static SYSFS_RO_FILE_OPS(TxRate);

static ssize_t aml_sysfs_RxPktNum_read(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct net_device *ndev = container_of(dev, struct net_device, dev);
    struct aml_vif *vif = netdev_priv(ndev);
    int len = 0;

    if (vif->up && (vif->sta.ap != NULL) && (AML_VIF_TYPE(vif) == NL80211_IFTYPE_STATION))
        len += sprintf(&buf[len], "RxPktNum :%d\n", vif->sta.ap->stats.rx_pkts);
    else
        len += sprintf(&buf[len], "RxPktNum :not connect\n");

    return len;
}
static SYSFS_RO_FILE_OPS(RxPktNum);

static ssize_t aml_sysfs_RxFailNum_read(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct net_device *ndev = container_of(dev, struct net_device, dev);
    struct aml_vif *vif = netdev_priv(ndev);
    struct aml_hw *aml_hw = vif->aml_hw;
    int len = 0;
    uint32_t rxfailnum = aml_send_get_rxfail_cnt(aml_hw, vif->vif_index);

    len += sprintf(&buf[len], "RxFailNum :%d\n", rxfailnum);

    return len;
}
static SYSFS_RO_FILE_OPS(RxFailNum);

static ssize_t aml_sysfs_Average_RxRate_read(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct net_device *ndev = container_of(dev, struct net_device, dev);
    struct aml_vif *vif = netdev_priv(ndev);
    struct aml_hw *aml_hw = vif->aml_hw;
    int len = 0;

    if (vif->up && (vif->sta.ap != NULL) &&
        (AML_VIF_TYPE(vif) == NL80211_IFTYPE_STATION)) {
        len += sprintf(&buf[len], "average_rxrate :%d\n", aml_hw->customer_priv.rx_average_rate);
    }

    return len;
}
static SYSFS_RO_FILE_OPS(Average_RxRate);

static ssize_t aml_sysfs_Data_RxRate_read(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct net_device *ndev = container_of(dev, struct net_device, dev);
    struct aml_vif *vif = netdev_priv(ndev);
    struct aml_hw *aml_hw = vif->aml_hw;
    struct rx_vector_1 *last_rx;
    int len = 0;
    unsigned int fmt, pre, bw, nss, mcs, gi, dcm = 0;

    if (vif->up && (vif->sta.ap != NULL) &&
        (AML_VIF_TYPE(vif) == NL80211_IFTYPE_STATION)) {
        last_rx = &vif->sta.ap->stats.last_rx.rx_vect1;
        fmt = last_rx->format_mod;
        bw = last_rx->ch_bw;
        pre = last_rx->pre_type;
        if (fmt >= FORMATMOD_HE_SU) {
            mcs = last_rx->he.mcs;
            nss = last_rx->he.nss;
            gi = last_rx->he.gi_type;
            if ((fmt == FORMATMOD_HE_MU) || (fmt == FORMATMOD_HE_ER))
                bw = last_rx->he.ru_size;
            dcm = last_rx->he.dcm;
        } else if (fmt == FORMATMOD_VHT) {
            mcs = last_rx->vht.mcs;
            nss = last_rx->vht.nss;
            gi = last_rx->vht.short_gi;
        } else if (fmt >= FORMATMOD_HT_MF) {
            mcs = last_rx->ht.mcs % 8;
            nss = last_rx->ht.mcs / 8;
            gi = last_rx->ht.short_gi;
        } else {
            BUG_ON((mcs = legrates_lut[last_rx->leg_rate].idx) == -1);
            nss = 0;
            gi = 0;
        }

        len += print_rate(&buf[len], 30, fmt, nss, mcs, bw, gi, pre, dcm, NULL, 0);
    }

    len += sprintf(&buf[len], "\n");

    return len;
}
static SYSFS_RO_FILE_OPS(Data_RxRate);

static ssize_t aml_sysfs_i4RSSI0_read(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct net_device *ndev = container_of(dev, struct net_device, dev);
    struct aml_vif *vif = netdev_priv(ndev);
    struct aml_hw *aml_hw = vif->aml_hw;
    struct aml_sta *sta;
    struct aml_txq *txq;
    int i = 20;
    int len = 0;
    int rssi;
    uint32_t agcpow_ct2;

    if (vif->up && (vif->sta.ap != NULL) && (AML_VIF_TYPE(vif) == NL80211_IFTYPE_STATION)) {
        sta = vif->sta.ap;
        txq = aml_txq_sta_get(sta, 0, aml_hw);
        while (i-- > 0) {
            if (txq->status & AML_TXQ_STOP_CHAN) {
                msleep(10);
                continue;
            } else {
                // make sure cur_channel is vif when get rssi
                agcpow_ct2 = AML_REG_READ(aml_hw->plat, AML_ADDR_SYSTEM, 0xc0b7d8);
                rssi = (agcpow_ct2 >> 21) - 2048;
                len += sprintf(&buf[len], "i4RSSI0 :%d\n", rssi);
                break;
            }
        }
    }

    return len;
}
static SYSFS_RO_FILE_OPS(i4RSSI0);

static ssize_t aml_sysfs_i4RSSI1_read(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct net_device *ndev = container_of(dev, struct net_device, dev);
    struct aml_vif *vif = netdev_priv(ndev);
    struct aml_hw *aml_hw = vif->aml_hw;
    struct aml_sta *sta;
    struct aml_txq *txq;
    int i = 20;
    int len = 0;
    int rssi;
    uint32_t agcpow_ct2;

    if (vif->up && (vif->sta.ap != NULL) && (AML_VIF_TYPE(vif) == NL80211_IFTYPE_STATION)) {
        sta = vif->sta.ap;
        txq = aml_txq_sta_get(sta, 0, aml_hw);
        while (i-- > 0) {
            if (txq->status & AML_TXQ_STOP_CHAN) {
                msleep(10);
                continue;
            } else {
                msleep(10);
                // make sure cur_channel is vif when get rssi
                agcpow_ct2 = AML_REG_READ(aml_hw->plat, AML_ADDR_SYSTEM, 0xc0b7dc);
                rssi = (agcpow_ct2 >> 21) - 2048;
                len += sprintf(&buf[len], "i4RSSI1 :%d\n", rssi);
                break;
            }
        }
    }

    return len;
}
static SYSFS_RO_FILE_OPS(i4RSSI1);


static ssize_t aml_sysfs_iSnrR0Phase2_read(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct net_device *ndev = container_of(dev, struct net_device, dev);
    struct aml_vif *vif = netdev_priv(ndev);
    struct aml_hw *aml_hw = vif->aml_hw;
    int len = 0;

    len += sprintf(&buf[len], "iSnrR0Phase2 :%d\n", AML_REG_READ(aml_hw->plat, AML_ADDR_MAC_PHY, REG_OF_SYNC_SNR) & 0xffff);

    return len;
}
static SYSFS_RO_FILE_OPS(iSnrR0Phase2);

static ssize_t aml_sysfs_iSnrR1Phase2_read(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct net_device *ndev = container_of(dev, struct net_device, dev);
    struct aml_vif *vif = netdev_priv(ndev);
    struct aml_hw *aml_hw = vif->aml_hw;
    int len = 0;

    len += sprintf(&buf[len], "iSnrR1Phase2 :%d\n", AML_REG_READ(aml_hw->plat, AML_ADDR_MAC_PHY, REG_OF_SYNC_SNR) & 0xffff);

    return len;
}
static SYSFS_RO_FILE_OPS(iSnrR1Phase2);

static ssize_t aml_sysfs_Glitch_Total_read(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct net_device *ndev = container_of(dev, struct net_device, dev);
    struct aml_vif *vif = netdev_priv(ndev);
    struct aml_hw *aml_hw = vif->aml_hw;
    int len = 0;

    len += sprintf(&buf[len], "\n");

    return 0; //len;
}
static SYSFS_RO_FILE_OPS(Glitch_Total);

static ssize_t aml_sysfs_Glitch_Diff_read(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct net_device *ndev = container_of(dev, struct net_device, dev);
    struct aml_vif *vif = netdev_priv(ndev);
    struct aml_hw *aml_hw = vif->aml_hw;
    int len = 0;

    len += sprintf(&buf[len], "\n");

    return 0; //len;
}
static SYSFS_RO_FILE_OPS(Glitch_Diff);

static ssize_t aml_sysfs_band_info_read(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct net_device *ndev = container_of(dev, struct net_device, dev);
    struct aml_vif *vif = netdev_priv(ndev);
    int len = 0;
    char band_info_str[2][4] = {"2G", "5G"};

    if (vif->up && (vif->sta.ap != NULL) &&
        (AML_VIF_TYPE(vif) == NL80211_IFTYPE_STATION)) {
        if (vif->sta.ap->band >= 2) {
            len += sprintf(&buf[len], "band :error\n");
            return len;
        }
        len += sprintf(&buf[len], "band :%s\n", band_info_str[vif->sta.ap->band]);
    }

    return len;
}
static SYSFS_RO_FILE_OPS(band_info);

static ssize_t aml_sysfs_band_query_flag_read(struct device *dev, struct device_attribute *attr, char *buf)
{
    int len = 0;

    return len;
}
static SYSFS_RO_FILE_OPS(band_query_flag);

static ssize_t aml_sysfs_wake_on_pno_read(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct net_device *ndev = container_of(dev, struct net_device, dev);
    struct aml_vif *vif = netdev_priv(ndev);
    struct aml_hw *aml_hw = vif->aml_hw;
    int len = 0;

    len += sprintf(&buf[len], "wake_on_pno :%d\n", aml_hw->customer_priv.wake_on_pno);

    return len;
}

static ssize_t aml_sysfs_wake_on_pno_write(struct device *dev, struct device_attribute *attr,
			 const char *user_buf, size_t count)
{
    struct net_device *ndev = container_of(dev, struct net_device, dev);
    struct aml_vif *vif = netdev_priv(ndev);
    struct aml_hw *aml_hw = vif->aml_hw;
    char buf[10] = {0};
    u8 new_state;
    bool len = min_t(size_t, sizeof(buf), count);

    if (copy_from_user(buf , user_buf, len)) {
        AML_INFO("copy_from_user fail\n");
        return -EFAULT;
    }

    new_state = buf[0] - '0';
    AML_INFO("input param:%c, new_state:%d, old_state:%d\n", buf[0], new_state, aml_hw->customer_priv.wake_on_pno);
    aml_hw->customer_priv.wake_on_pno = new_state;

    return count;
}
static SYSFS_RW_FILE_OPS(wake_on_pno);

/************************ P2P *********************************/

static ssize_t aml_sysfs_p2p_channel_read(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct net_device *ndev = container_of(dev, struct net_device, dev);
    struct aml_vif *vif = netdev_priv(ndev);
    struct aml_hw *aml_hw = vif->aml_hw;
    int len = 0;

    list_for_each_entry(vif, &aml_hw->vifs, list) {
        if (!vif->up || (vif->ch_index == AML_CH_NOT_SET))
            continue;
        if ((AML_VIF_TYPE(vif) == NL80211_IFTYPE_P2P_GO) &&
            (aml_hw->chanctx_table[vif->ch_index].chan_def.chan != NULL)) {
            len += sprintf(&buf[len], "p2p_channel :%d\n",
                ieee80211_frequency_to_channel(aml_hw->chanctx_table[vif->ch_index].chan_def.chan->center_freq));
        }
    }

    return len;
}
static SYSFS_RO_FILE_OPS(p2p_channel);

static ssize_t aml_sysfs_p2p_DevNum_read(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct net_device *ndev = container_of(dev, struct net_device, dev);
    struct aml_vif *vif = netdev_priv(ndev);
    struct aml_sta *sta, *tmp;
    int len = 0;
    int devnum = 0;

    if (AML_VIF_TYPE(vif) == NL80211_IFTYPE_P2P_GO) {
        list_for_each_entry_safe(sta, tmp, &vif->ap.sta_list, list) {
            devnum++;
        }
    }

    len += sprintf(&buf[len], "DevNum :%d\n", devnum);

    return len;
}
static SYSFS_RO_FILE_OPS(p2p_DevNum);

static ssize_t aml_sysfs_p2p_bandwidth_read(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct net_device *ndev = container_of(dev, struct net_device, dev);
    struct aml_vif *vif = netdev_priv(ndev);
    struct aml_hw *aml_hw = vif->aml_hw;
    int len = 0;
    const char bw_info[3][4] = {"20M", "40M", "80M"};

    if (vif->up && (vif->ch_index != AML_CH_NOT_SET) && (AML_VIF_TYPE(vif) == NL80211_IFTYPE_P2P_GO)) {
        if ((aml_hw->chanctx_table[vif->ch_index].chan_def.width - NL80211_CHAN_WIDTH_20) < 3)
            len += sprintf(&buf[len], "p2p_bandwidth :%d\n",
                           aml_hw->chanctx_table[vif->ch_index].chan_def.width - NL80211_CHAN_WIDTH_20);
        else
            len += sprintf(&buf[len], "dev:%s width:error\n", vif->ndev->name);
    }

    return len;
}
static SYSFS_RO_FILE_OPS(p2p_bandwidth);

static ssize_t aml_sysfs_p2p_iRssi_read(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct net_device *ndev = container_of(dev, struct net_device, dev);
    struct aml_vif *vif = netdev_priv(ndev);
    struct aml_hw *aml_hw = vif->aml_hw;
    int len = 0;

    len += sprintf(&buf[len], "p2p_Rssi :%d\n",
                   ((AML_REG_READ(aml_hw->plat, AML_ADDR_MAC_PHY, REG_OF_SYNC_P2P_RSSI) & 0xffff0000) >> 16) - 256);

    return len;
}
static SYSFS_RO_FILE_OPS(p2p_iRssi);

static ssize_t aml_sysfs_p2p_iNoise_read(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct net_device *ndev = container_of(dev, struct net_device, dev);
    struct aml_vif *vif = netdev_priv(ndev);
    struct aml_hw *aml_hw = vif->aml_hw;
    int len = 0;
    u32 agc_state;
    u32 agc1, agc2;
    s32 noise1, noise2;

    // W2 donot support, W2L support
    agc_state = AML_REG_READ(aml_hw->plat, AML_ADDR_SYSTEM, 0xc0b7d4);
    AML_REG_WRITE(0x29, aml_hw->plat, AML_ADDR_SYSTEM, 0xc0b7d4);
    agc1 = AML_REG_READ(aml_hw->plat, AML_ADDR_SYSTEM, 0xc0b7d8);
    agc2 = AML_REG_READ(aml_hw->plat, AML_ADDR_SYSTEM, 0xc0b7dc);
    AML_REG_WRITE(agc_state, aml_hw->plat, AML_ADDR_SYSTEM, 0xc0b7d4);

    noise1 = (agc1 >> 21) - 2048;
    noise2 = (agc2 >> 21) - 2048;
    len += sprintf(&buf[len], "p2p_iNoise :%d\n", (noise1 + noise2) / 2);

    return len;
}
static SYSFS_RO_FILE_OPS(p2p_iNoise);

static ssize_t aml_sysfs_p2p_iSnr_read(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct net_device *ndev = container_of(dev, struct net_device, dev);
    struct aml_vif *vif = netdev_priv(ndev);
    struct aml_hw *aml_hw = vif->aml_hw;
    int len = 0;

    len += sprintf(&buf[len], "p2p_iSnr :%d\n",
                   AML_REG_READ(aml_hw->plat, AML_ADDR_MAC_PHY, REG_OF_SYNC_SNR) >> 16);

    return len;
}
static SYSFS_RO_FILE_OPS(p2p_iSnr);

static ssize_t aml_sysfs_p2p_i4Rssi0_read(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct net_device *ndev = container_of(dev, struct net_device, dev);
    struct aml_vif *vif = netdev_priv(ndev);
    struct aml_hw *aml_hw = vif->aml_hw;
    int len = 0;
    int rssi;
    uint32_t agcpow_ct2 = AML_REG_READ(aml_hw->plat, AML_ADDR_SYSTEM, 0xc0b7d8);

    // TODO get and save in fw
    rssi = (agcpow_ct2 >> 21) - 2048;
    len += sprintf(&buf[len], "p2p_i4Rssi0:%d\n", rssi);

    return len;
}
static SYSFS_RO_FILE_OPS(p2p_i4Rssi0);

static ssize_t aml_sysfs_p2p_i4Rssi1_read(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct net_device *ndev = container_of(dev, struct net_device, dev);
    struct aml_vif *vif = netdev_priv(ndev);
    struct aml_hw *aml_hw = vif->aml_hw;
    int len = 0;
    int rssi;
    uint32_t agcpow_ct2 = AML_REG_READ(aml_hw->plat, AML_ADDR_SYSTEM, 0xc0b7dc);

    // TODO get and save in fw
    rssi = (agcpow_ct2 >> 21) - 2048;
    len += sprintf(&buf[len], "p2p_i4Rssi1:%d\n", rssi);

    return len;
}
static SYSFS_RO_FILE_OPS(p2p_i4Rssi1);

static ssize_t aml_sysfs_p2p_iSnrR0Phase2_read(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct net_device *ndev = container_of(dev, struct net_device, dev);
    struct aml_vif *vif = netdev_priv(ndev);
    struct aml_hw *aml_hw = vif->aml_hw;
    int len = 0;

    len += sprintf(&buf[len], "p2p_iSnrR0Phase2 :%d\n", AML_REG_READ(aml_hw->plat, AML_ADDR_MAC_PHY, REG_OF_SYNC_SNR) & 0xffff);

    return len;
}
static SYSFS_RO_FILE_OPS(p2p_iSnrR0Phase2);

static ssize_t aml_sysfs_p2p_iSnrR1Phase2_read(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct net_device *ndev = container_of(dev, struct net_device, dev);
    struct aml_vif *vif = netdev_priv(ndev);
    struct aml_hw *aml_hw = vif->aml_hw;
    int len = 0;

    len += sprintf(&buf[len], "p2p_iSnrR1Phase2 :%d\n", AML_REG_READ(aml_hw->plat, AML_ADDR_MAC_PHY, REG_OF_SYNC_SNR) & 0xffff);

    return len;
}
static SYSFS_RO_FILE_OPS(p2p_iSnrR1Phase2);

static ssize_t aml_sysfs_p2p_TxPkt_read(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct net_device *ndev = container_of(dev, struct net_device, dev);
    struct aml_vif *vif = netdev_priv(ndev);
    struct aml_sta *sta, *tmp;
    int len = 0;
    int txpktnum = 0;

    if (vif->up && (vif->sta.ap != NULL) && (AML_VIF_TYPE(vif) == NL80211_IFTYPE_P2P_CLIENT))
        txpktnum = vif->sta.ap->stats.tx_pkts;

    if (vif->up && (AML_VIF_TYPE(vif) == NL80211_IFTYPE_P2P_GO)) {
        list_for_each_entry_safe(sta, tmp, &vif->ap.sta_list, list)
            txpktnum += sta->stats.tx_pkts;
    }

    len += sprintf(&buf[len], "p2p_TxPkt :%d\n", txpktnum);

    return len;
}
static SYSFS_RO_FILE_OPS(p2p_TxPkt);

static ssize_t aml_sysfs_p2p_TxFailPkt_read(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct net_device *ndev = container_of(dev, struct net_device, dev);
    struct aml_vif *vif = netdev_priv(ndev);
    struct aml_sta *sta, *tmp;
    int len = 0;
    int txfailnum = 0;

    if (vif->up && (vif->sta.ap != NULL) && (AML_VIF_TYPE(vif) == NL80211_IFTYPE_P2P_CLIENT))
        len += sprintf(&buf[len], "p2p_TxFail :%d\n", vif->sta.ap->stats.tx_fails);

    if (vif->up && (AML_VIF_TYPE(vif) == NL80211_IFTYPE_P2P_GO)) {
        list_for_each_entry_safe(sta, tmp, &vif->ap.sta_list, list)
            txfailnum += sta->stats.tx_fails;

        len += sprintf(&buf[len], "p2p_TxFail :%d\n", txfailnum);
    }

    return len;
}
static SYSFS_RO_FILE_OPS(p2p_TxFailPkt);

static ssize_t aml_sysfs_p2p_RxPkt_read(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct net_device *ndev = container_of(dev, struct net_device, dev);
    struct aml_vif *vif = netdev_priv(ndev);
    struct aml_sta *sta, *tmp;
    int len = 0;
    int rx_pkts = 0;

    if (vif->up && (vif->sta.ap != NULL) && (AML_VIF_TYPE(vif) == NL80211_IFTYPE_P2P_CLIENT))
        len += sprintf(&buf[len], "p2p_RxPkt :%d\n", vif->sta.ap->stats.rx_pkts);

    if (vif->up && (AML_VIF_TYPE(vif) == NL80211_IFTYPE_P2P_GO)) {
        list_for_each_entry_safe(sta, tmp, &vif->ap.sta_list, list)
            rx_pkts += sta->stats.rx_pkts;

        len += sprintf(&buf[len], "p2p_RxPkt :%d\n", rx_pkts);
    }

    return len;
}
static SYSFS_RO_FILE_OPS(p2p_RxPkt);

static ssize_t aml_sysfs_p2p_RxFail_read(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct net_device *ndev = container_of(dev, struct net_device, dev);
    struct aml_vif *vif = netdev_priv(ndev);
    struct aml_hw *aml_hw = vif->aml_hw;
    int len = 0;
    uint32_t rxfailnum = aml_send_get_rxfail_cnt(aml_hw, vif->vif_index);

    len += sprintf(&buf[len], "p2p_RxFail :%d\n", rxfailnum);

    return len;
}
static SYSFS_RO_FILE_OPS(p2p_RxFail);

static ssize_t aml_sysfs_p2p_Glitch_read(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct net_device *ndev = container_of(dev, struct net_device, dev);
    struct aml_vif *vif = netdev_priv(ndev);
    struct aml_hw *aml_hw = vif->aml_hw;
    int len = 0;

    len += sprintf(&buf[len], "\n");

    return 0; //len;
}
static SYSFS_RO_FILE_OPS(p2p_Glitch);

static ssize_t aml_sysfs_p2p_Mac_read(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct net_device *ndev = container_of(dev, struct net_device, dev);
    struct aml_vif *vif = netdev_priv(ndev);
    struct aml_sta *sta, *tmp;
    int len = 0;
    int count = 0;

    if (vif->up && (AML_VIF_TYPE(vif) == NL80211_IFTYPE_P2P_GO)) {
        list_for_each_entry_safe(sta, tmp, &vif->ap.sta_list, list) {
            len += sprintf(&buf[len], "p2p[%d]: %pM\n", count, sta->mac_addr);
            count++;
        }
    }

    return len;
}
static SYSFS_RO_FILE_OPS(p2p_Mac);

static ssize_t aml_sysfs_p2p_RxRate_read(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct net_device *ndev = container_of(dev, struct net_device, dev);
    struct aml_vif *vif = netdev_priv(ndev);
    struct rx_vector_1 *last_rx;
    struct aml_sta *sta, *tmp, *last_rx_sta = NULL;
    int len = 0;
    unsigned int fmt, pre, bw, nss, mcs, gi, dcm = 0;
    unsigned long last_act;

    len += sprintf(&buf[len], "p2p_Rx_rate :");
    if (vif->up && (AML_VIF_TYPE(vif) == NL80211_IFTYPE_P2P_GO)) {
        list_for_each_entry_safe(sta, tmp, &vif->ap.sta_list, list) {
            if (!last_rx_sta || (last_act < sta->stats.last_act)) {
                last_act = sta->stats.last_act;
                last_rx_sta = sta;
            }
        }

        do {
            if (last_rx_sta == NULL)
                break;

            last_rx = &last_rx_sta->stats.last_rx.rx_vect1;
            fmt = last_rx->format_mod;
            bw = last_rx->ch_bw;
            pre = last_rx->pre_type;
            if (fmt >= FORMATMOD_HE_SU) {
                mcs = last_rx->he.mcs;
                nss = last_rx->he.nss;
                gi = last_rx->he.gi_type;
                if ((fmt == FORMATMOD_HE_MU) || (fmt == FORMATMOD_HE_ER))
                    bw = last_rx->he.ru_size;
                dcm = last_rx->he.dcm;
            } else if (fmt == FORMATMOD_VHT) {
                mcs = last_rx->vht.mcs;
                nss = last_rx->vht.nss;
                gi = last_rx->vht.short_gi;
            } else if (fmt >= FORMATMOD_HT_MF) {
                mcs = last_rx->ht.mcs % 8;
                nss = last_rx->ht.mcs / 8;
                gi = last_rx->ht.short_gi;
            } else {
                BUG_ON((mcs = legrates_lut[last_rx->leg_rate].idx) == -1);
                nss = 0;
                gi = 0;
            }

            len += print_rate(&buf[len], 30, fmt, nss, mcs, bw, gi, pre, dcm, NULL, 0);
        } while (0);
    }

    len += sprintf(&buf[len], "\n");

    return len;
}
static SYSFS_RO_FILE_OPS(p2p_RxRate);

static ssize_t aml_sysfs_bypass_dfs_read(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct net_device *ndev = container_of(dev, struct net_device, dev);
    struct aml_vif *vif = netdev_priv(ndev);
    struct aml_hw *aml_hw = vif->aml_hw;
    int len = 0;

    len += sprintf(&buf[len], "bypass_dfs :%d\n", aml_hw->customer_priv.dfs_on);

    return len;
}

static ssize_t aml_sysfs_bypass_dfs_write(struct device *dev, struct device_attribute *attr,
			 const char *user_buf, size_t count)
{
    struct net_device *ndev = container_of(dev, struct net_device, dev);
    struct aml_vif *vif = netdev_priv(ndev);
    struct aml_hw *aml_hw = vif->aml_hw;
    u8 new_state = *user_buf - '0';

    if (new_state > 1) {
        AML_INFO("param error\n");
        return -EINVAL;
    }

    if (aml_hw->customer_priv.dfs_on == new_state) {
        AML_INFO("already set\n");
        return count;
    }
    else {
        aml_hw->customer_priv.dfs_on = new_state;
        aml_send_me_chan_config_req(aml_hw);
    }

    return count;
}
static SYSFS_RW_FILE_OPS(bypass_dfs);

static ssize_t aml_sysfs_go_hidden_mode_read(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct net_device *ndev = container_of(dev, struct net_device, dev);
    struct aml_vif *vif = netdev_priv(ndev);
    struct aml_hw *aml_hw = vif->aml_hw;
    int len = 0;

    len += sprintf(&buf[len], "go_hidden_mode:%d\n", aml_hw->customer_priv.go_hidden_mode);

    return len;
}
static ssize_t aml_sysfs_go_hidden_mode_write(struct device *dev, struct device_attribute *attr,
			 const char *user_buf, size_t count)
{
    struct net_device *ndev = container_of(dev, struct net_device, dev);
    struct aml_vif *vif = netdev_priv(ndev);
    struct aml_hw *aml_hw = vif->aml_hw;
    u8 new_state = *user_buf - '0';

    if (new_state > 1) {
        AML_INFO("param error\n");
        return -EINVAL;
    }

    if (aml_hw->customer_priv.go_hidden_mode == new_state) {
        AML_INFO("already set\n");
        return count;
    }

    // TODO update beacon
    printk("plz use wpa_cli cmd\n");

    return count;
}
static SYSFS_RW_FILE_OPS(go_hidden_mode);

static struct attribute *aml_sysfs_sta_entries[] = {
    &dev_attr_hal_spec.attr,
    &dev_attr_cur_channel.attr,
    &dev_attr_bandwidth.attr,
    &dev_attr_rRssi.attr,
    &dev_attr_iNoise.attr,
    &dev_attr_iSnr.attr,
    &dev_attr_ssid.attr,
    &dev_attr_bssid.attr,
    &dev_attr_TxPktNum.attr,
    &dev_attr_TxFailNum.attr,
    &dev_attr_TxRate.attr,
    &dev_attr_RxPktNum.attr,
    &dev_attr_RxFailNum.attr,
    &dev_attr_Average_RxRate.attr,
    &dev_attr_Data_RxRate.attr,
    &dev_attr_i4RSSI0.attr,
    &dev_attr_i4RSSI1.attr,
    &dev_attr_iSnrR0Phase2.attr,
    &dev_attr_iSnrR1Phase2.attr,
    &dev_attr_Glitch_Total.attr,
    &dev_attr_Glitch_Diff.attr,
    &dev_attr_band_info.attr,
    &dev_attr_band_query_flag.attr,
    &dev_attr_wake_on_pno.attr,
    NULL,
};

static struct attribute *aml_sysfs_p2p_entries[] = {
    &dev_attr_p2p_DevNum.attr,
    &dev_attr_p2p_channel.attr,
    &dev_attr_p2p_bandwidth.attr,
    &dev_attr_p2p_iRssi.attr,
    &dev_attr_p2p_iNoise.attr,
    &dev_attr_p2p_iSnr.attr,
    &dev_attr_p2p_i4Rssi0.attr,
    &dev_attr_p2p_i4Rssi1.attr,
    &dev_attr_p2p_iSnrR0Phase2.attr,
    &dev_attr_p2p_iSnrR1Phase2.attr,
    &dev_attr_p2p_TxPkt.attr,
    &dev_attr_p2p_TxFailPkt.attr,
    &dev_attr_p2p_RxPkt.attr,
    &dev_attr_p2p_RxFail.attr,
    &dev_attr_p2p_RxRate.attr,
    &dev_attr_p2p_Glitch.attr,
    &dev_attr_p2p_Mac.attr,
    &dev_attr_bypass_dfs.attr,
    &dev_attr_go_hidden_mode.attr,
    NULL,
};

static struct attribute_group aml_sta_attribute_group = {
        .attrs = aml_sysfs_sta_entries,
};

static struct attribute_group aml_p2p_attribute_group = {
        .attrs = aml_sysfs_p2p_entries,
};

int aml_register_netdevice_sysfs(struct net_device *ndev, enum nl80211_iftype type)
{
    struct attribute_group *temp;
    int ret;

    AML_INFO("name :%s\n", ndev->name);

    if (memcmp(ndev->name, "wlan", 4) == 0)
        temp = &aml_sta_attribute_group;
    else if (memcmp(ndev->name, "p2p", 3) == 0)
        temp = &aml_p2p_attribute_group;
    else
        return 0;

    ret = sysfs_create_group(&ndev->dev.kobj, temp);
    if (ret < 0)
        AML_INFO("ERROR init sysfs failed\n");

    return ret;
}

int aml_unregister_netdevice_sysfs(struct net_device *ndev, enum nl80211_iftype type)
{
    struct kobject *kobj = &ndev->dev.kobj;
    AML_INFO("sd:%p\n", kobj->sd);
/*
    if (type == NL80211_IFTYPE_STATION)
        sysfs_remove_group(&ndev->dev.kobj, &aml_sta_attribute_group);

    if (type == NL80211_IFTYPE_P2P_GO)
        sysfs_remove_group(&ndev->dev.kobj, &aml_p2p_attribute_group);
*/
}

int customer_dbgfs_unregister(struct aml_hw *aml_hw)
{
    aml_destroy_proc_dir(aml_hw);
}

int customer_dbgfs_register(struct aml_hw *aml_hw, const char *name)
{
    if (init_net.proc_net == (struct proc_dir_entry *)NULL) {
        ERROR_DEBUG_OUT("init proc fail: proc_net == NULL\n");
        return -ENOENT;
    }

    aml_hw->customer_priv.proc_dir = proc_mkdir("wlan", init_net.proc_net);
    if (!aml_hw->customer_priv.proc_dir) {
        ERROR_DEBUG_OUT("aml_hw->customer_priv.proc_dir == NULL, ERROR\n");
        return -ENOENT;
    }

    aml_create_proc_dir(aml_hw);
    g_aml_hw = aml_hw;

    aml_hw->customer_priv.registering = true;
    aml_hw->customer_priv.dfs_on = 0; // invalid;
    aml_hw->customer_priv.retry_cnt = 7; // invalid;
    aml_hw->customer_priv.disconnect_reason_code = 65535; // invalid;
    aml_hw->customer_priv.dbg_level = 2; // default;
    aml_hw->customer_priv.wake_reason = 0xff; // invalid;
    aml_hw->customer_priv.wake_on_pno = 0;

    return 0;
err:
    customer_dbgfs_unregister(aml_hw);
    return -ENOMEM;
}


