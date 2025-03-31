
#define AML_MODULE  COMMON

#include "usb_common.h"
#include "chip_ana_reg.h"
#include "wifi_intf_addr.h"
#include "sg_common.h"
#include "fi_sdio.h"
#include "w2_usb.h"
#include "aml_interface.h"
#include "fi_w2_sdio.h"
#include "chip_intf_reg.h"
#include "aml_interface.h"
#include "aml_log.h"
#include "chip_bt_pmu_reg.h"

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 14, 0))
#include <linux/sched/clock.h>
#endif

struct auc_hif_ops g_auc_hif_ops;
struct usb_device *g_udev = NULL;
struct aml_hwif_usb g_hwif_usb;
unsigned char auc_driver_insmoded;
unsigned char auc_wifi_in_insmod;
unsigned char g_usb_after_probe;
unsigned char g_chip_function_ctrl = 0;
unsigned int auc_prob_cnt = 0;
struct crg_msc_cbw *g_cmd_buf = NULL;
unsigned char *g_kmalloc_buf = NULL;
struct mutex auc_usb_mutex;


extern unsigned char wifi_drv_rmmod_ongoing;
extern struct aml_bus_state_detect bus_state_detect;
extern struct aml_pm_type g_wifi_pm;
extern void auc_w2_ops_init(void);
extern void extern_wifi_set_enable(int is_on);
/*for bluetooth get read/write point*/
int bt_wt_ptr = 0;
int bt_rd_ptr = 0;
/*co-exist flag for bt/wifi mode*/
int coex_flag = 0;
//use for suspend(kill)/resume(submit) usb_urb
struct urb *g_usb_urb = NULL;
struct urb * auc_alloc_urb(int iso_packets, gfp_t mem_flags)
{
    g_usb_urb = usb_alloc_urb(0, GFP_ATOMIC);
    return g_usb_urb;
}

void chip_function_select_usb(void) {
    switch (g_udev->descriptor.idProduct) {
        case W2lu_W265U2_PRODUCT_B_AMLOGIC_EFUSE:
            g_chip_function_ctrl |= CHIP_FUNCTION_DISABLE_154;
            break;

        case W2lu_W255U1_PRODUCT_B_AMLOGIC_EFUSE:
            g_chip_function_ctrl |= CHIP_FUNCTION_DISABLE_154;
            g_chip_function_ctrl |= CHIP_FUNCTION_DISABLE_11AX;
            break;

        case W2lu_W265U2M_PRODUCT_B_AMLOGIC_EFUSE:
        default:
            g_chip_function_ctrl = 0;//full function
            break;
    }
}

static int auc_probe(struct usb_interface *interface, const struct usb_device_id *id)
{
    g_udev = usb_get_dev(interface_to_usbdev(interface));
    memset(g_kmalloc_buf, 0,  1024*20);
    memset(g_cmd_buf, 0, sizeof(struct crg_msc_cbw ));

    auc_prob_cnt++;

    auc_w2_ops_init();
    g_auc_hif_ops.hi_enable_scat();

#ifdef CONFIG_PM
    if (atomic_read(&g_wifi_pm.bus_suspend_cnt)) {
        atomic_set(&g_wifi_pm.bus_suspend_cnt, 0);
    }
#endif

    if (auc_prob_cnt > 1) {
        PRINT("update udev new:0x%08x , old: 0x%08x, auc prob cnt %d\n", g_udev, g_usb_urb->dev, auc_prob_cnt);
        bus_state_detect.bus_err = 1;
        g_usb_urb->dev = g_udev;
    }

    g_usb_after_probe = 1;
    chip_function_select_usb();

    PRINT("%s(%d), pid is %04x, function ctrl:%02x\n",
        __func__, __LINE__, g_udev->descriptor.idProduct, g_chip_function_ctrl);

    return 0;
}


static void auc_disconnect(struct usb_interface *interface)
{
    usb_set_intfdata(interface, NULL);
    usb_put_dev(g_udev);
    g_usb_after_probe = 0;
    atomic_set(&g_wifi_pm.bus_suspend_cnt, 0);
    PRINT("--------aml_usb:disconnect-------\n");
}

#ifdef CONFIG_PM
static int auc_reset_resume(struct usb_interface *interface)
{
    int ret = 0;
    atomic_set(&g_wifi_pm.bus_suspend_cnt, 0);

    USB_BEGIN_LOCK();
    ret = usb_submit_urb(g_usb_urb, GFP_ATOMIC);
    USB_END_LOCK();
    if (ret < 0) {
        ERROR_DEBUG_OUT("usb_submit_urb failed %d\n", ret);
    }

    PRINT("--------aml_usb:reset done-------\n");
    return 0;
}

static int auc_suspend(struct usb_interface *interface,pm_message_t state)
{
    int cnt = 0;
    unsigned int ret = 0;

	//bt open
	if ((auc_read_word_by_ep_for_bt(RG_BT_PMU_A16, USB_EP2) & BIT(31)))
	{
		//bt drv suspend set bit26
		while (!(auc_read_word_by_ep_for_bt(RG_AON_A24, USB_EP2) & BIT(26)))
		{
			msleep(50);
			cnt++;
			if (cnt > 1000)
			{
				PRINT("bt drv suspend fail \n");
				return -1;
			}
		}
	}

    if (atomic_read(&g_wifi_pm.wifi_enable))
    {
        while (atomic_read(&g_wifi_pm.drv_suspend_cnt) == 0)
        {
            msleep(50);
            cnt++;
            if (cnt > 40)
            {
                PRINT("wifi suspend fail \n");
                return -1;
            }
        }
    }

    atomic_set(&g_wifi_pm.bus_suspend_cnt, 1);
    USB_BEGIN_LOCK();
    if (g_usb_urb && g_usb_urb->status != 0) {
        PRINT("usb_kill_urb\n");
        usb_kill_urb((g_usb_urb));
    }
    USB_END_LOCK();

    PRINT("---------aml_usb suspend-------\n");
    return 0;
}

static int auc_resume(struct usb_interface *interface)
{
    int ret = 0;

    USB_BEGIN_LOCK();
    ret = usb_submit_urb(g_usb_urb, GFP_ATOMIC);
    USB_END_LOCK();
    if (ret < 0) {
        ERROR_DEBUG_OUT("usb_submit_urb failed %d\n", ret);
    }

    atomic_set(&g_wifi_pm.bus_suspend_cnt, 0);
    PRINT("---------aml_usb auc_resume -------\n");
    return 0;
}
#endif

extern lp_shutdown_func g_lp_wifi_shutdown_func;
extern bt_shutdown_func g_bt_shutdown_func;
void auc_shutdown(struct device *dev)
{
    //Mask interrupt reporting to the host
    atomic_set(&g_wifi_pm.is_shut_down, 2);
    AML_INFO("aml_usb_shutdown begin \n" );

    // Notify fw to enter shutdown mode
    if (g_bt_shutdown_func != NULL)
    {
        g_bt_shutdown_func();
    }
    if (g_lp_wifi_shutdown_func != NULL)
    {
        g_lp_wifi_shutdown_func();
    }
    //notify fw shutdown
    auc_write_word_by_ep_for_wifi(RG_AON_A16, auc_read_word_by_ep_for_wifi(RG_AON_A16, USB_EP1)|BIT(28) ,USB_EP1);

    atomic_set(&g_wifi_pm.is_shut_down, 1);
}


static const struct usb_device_id auc_devices[] =
{
    {USB_DEVICE(W2_VENDOR,W2_PRODUCT)},
    {USB_DEVICE(W2u_VENDOR_AMLOGIC_EFUSE,W2u_PRODUCT_A_AMLOGIC_EFUSE)},
    {USB_DEVICE(W2u_VENDOR_AMLOGIC_EFUSE,W2u_PRODUCT_B_AMLOGIC_EFUSE)},
    {USB_DEVICE(W2u_VENDOR_AMLOGIC_EFUSE,W2lu_W265U2M_PRODUCT_A_AMLOGIC_EFUSE)},
    {USB_DEVICE(W2u_VENDOR_AMLOGIC_EFUSE,W2lu_W265U2_PRODUCT_A_AMLOGIC_EFUSE)},
    {USB_DEVICE(W2u_VENDOR_AMLOGIC_EFUSE,W2lu_W255U1_PRODUCT_A_AMLOGIC_EFUSE)},
    {USB_DEVICE(W2u_VENDOR_AMLOGIC_EFUSE,W2lu_W265U2M_PRODUCT_B_AMLOGIC_EFUSE)},
    {USB_DEVICE(W2u_VENDOR_AMLOGIC_EFUSE,W2lu_W265U2_PRODUCT_B_AMLOGIC_EFUSE)},
    {USB_DEVICE(W2u_VENDOR_AMLOGIC_EFUSE,W2lu_W255U1_PRODUCT_B_AMLOGIC_EFUSE)},
    {}
};

MODULE_DEVICE_TABLE(usb, auc_devices);

static struct usb_driver aml_usb_common_driver = {

    .name = "aml_usb_common",
    .id_table = auc_devices,
    .probe = auc_probe,
    .disconnect = auc_disconnect,
#ifdef CONFIG_PM
    .reset_resume = auc_reset_resume,
    .suspend = auc_suspend,
    .resume = auc_resume,
#endif
#if LINUX_VERSION_CODE <= KERNEL_VERSION(6, 8, 0)
    .drvwrap.driver.shutdown = auc_shutdown,
#endif
};


int aml_usb_insmod(void)
{
    int err = 0;

    g_cmd_buf = ZMALLOC(sizeof(*g_cmd_buf), "cmd stage", GFP_DMA | GFP_ATOMIC);
    if (!g_cmd_buf) {
        PRINT("g_cmd_buf malloc fail\n");
        return -ENOMEM;
    }
    g_kmalloc_buf = (unsigned char *)ZMALLOC(20*1024, "reg tmp", GFP_DMA | GFP_ATOMIC);
    if (!g_kmalloc_buf) {
        ERROR_DEBUG_OUT("data malloc fail\n");
        FREE(g_cmd_buf, "cmd stage");
        return -ENOMEM;
    }
    err = usb_register(&aml_usb_common_driver);
    if (err) {
        PRINT("failed to register usb driver: %d \n", err);
    }

    auc_driver_insmoded = 1;
    auc_wifi_in_insmod = 0;
    USB_LOCK_INIT();
    PRINT("%s(%d) aml common driver insmod\n", __func__, __LINE__);

    return err;
}

void aml_usb_rmmod(void)
{
    usb_deregister(&aml_usb_common_driver);
    auc_driver_insmoded = 0;
    wifi_drv_rmmod_ongoing = 0;
    auc_prob_cnt = 0;
    g_auc_hif_ops.hi_cleanup_scat();
    FREE(g_cmd_buf, "cmd stage");
    FREE(g_kmalloc_buf, "reg tmp");
    USB_LOCK_DESTROY();
#ifndef CONFIG_PT_MODE
#ifndef CONFIG_LINUXPC_VERSION
    extern_wifi_set_enable(0);
    msleep(100);
    extern_wifi_set_enable(1);
#endif
#endif

   PRINT("%s(%d) aml common driver rmsmod\n",__func__, __LINE__);
}
void aml_usb_reset(void)
{
    uint32_t count = 0;
    uint32_t try_cnt = 0;

Try_again:
    AML_INFO("******* usb reset begin *******\n");

#ifndef CONFIG_PT_MODE

#ifndef CONFIG_LINUXPC_VERSION
    extern_wifi_set_enable(0);
    while (g_usb_after_probe) {
        msleep(5);
        count++;
        if (count > 40 && try_cnt <= 3) {
            count = 0;
            try_cnt++;
            extern_wifi_set_enable(1);
            msleep(50);
            AML_ERR("usb reset fail, try again(%d)\n", try_cnt);
            goto Try_again;
        }
    }
    extern_wifi_set_enable(1);
#endif

    count = 0;
    try_cnt = 0;
    while ((!g_usb_after_probe) && try_cnt <= 3) {
        msleep(5);
        count++;
        if (count > 200) {
            count = 0;
            try_cnt++;
            AML_ERR("usb reset fail, try again(%d)\n", try_cnt);
            goto Try_again;
        }
    };
    bus_state_detect.bus_reset_ongoing = 0;
    bus_state_detect.bus_err = 0;
    AML_INFO("******* usb reset end *******\n");

    return;
#endif
}
EXPORT_SYMBOL(aml_usb_reset);
EXPORT_SYMBOL(aml_usb_insmod);
EXPORT_SYMBOL(aml_usb_rmmod);
EXPORT_SYMBOL(auc_alloc_urb);
EXPORT_SYMBOL(g_cmd_buf);
EXPORT_SYMBOL(g_auc_hif_ops);
EXPORT_SYMBOL(g_udev);
EXPORT_SYMBOL(auc_driver_insmoded);
EXPORT_SYMBOL(auc_wifi_in_insmod);
EXPORT_SYMBOL(auc_usb_mutex);
EXPORT_SYMBOL(g_usb_after_probe);
EXPORT_SYMBOL(bt_wt_ptr);
EXPORT_SYMBOL(bt_rd_ptr);
EXPORT_SYMBOL(coex_flag);
EXPORT_SYMBOL(g_chip_function_ctrl);

