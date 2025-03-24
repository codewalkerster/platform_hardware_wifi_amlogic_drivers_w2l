/**
 ******************************************************************************
 *
 * @file aml_irqs.c
 *
 * Copyright (C) Amlogic 2012-2021
 *
 ******************************************************************************
 */

#define AML_MODULE     IRQ

#include <linux/interrupt.h>
#include "aml_defs.h"
#include "ipc_host.h"
#include "aml_prof.h"
#include "aml_irqs.h"
#include "wifi_top_addr.h"

extern struct aml_pm_type g_wifi_pm;
extern struct aml_bus_state_detect bus_state_detect;
int aml_dat1_irq_handler(struct aml_hw *aml_hw);
u32 irq_handler_done;

#ifdef SDIO_MODE_ON
void aml_sdio_dat1_release(struct aml_hw *aml_hw)
{
    while (!irq_handler_done) {
        AML_RLMT_ERR("irq release need wait !!!\n");
        usleep_range(2,3);
    }

    struct sdio_func *func = aml_priv_to_func(SDIO_FUNC1);
    sdio_claim_host(func);
    sdio_release_irq(func);
    sdio_release_host(func);
    AML_RLMT_INFO("irq release\n");
}

void aml_sdio_dat1_claim(struct aml_hw *aml_hw)
{
    struct sdio_func *func = aml_priv_to_func(SDIO_FUNC1);
    dev_set_drvdata(&func->dev, aml_hw);
    sdio_claim_host(func);
    sdio_claim_irq(func, aml_irq_sdio_hdlr_for_pt);
    sdio_release_host(func);
    AML_RLMT_INFO("irq claim\n");
}

void aml_enable_sdio_irq(struct aml_hw *aml_hw)
{
    uint32_t en_irq = 0;

    aml_hw->irq_done = 1;
    en_irq |= BIT(31);
    //set bit31 to 0xa07064 enable sdio irq
    aml_hw->plat->hif_sdio_ops->hi_random_word_write(RG_WIFI_IF_FW2HST_CLR, en_irq);

    return;
}

u32 aml_disable_sdio_irq(struct aml_hw *aml_hw)
{
    unsigned int reg_data[2] = {0};

    //read 0xa0706c for sdio irq disable
    aml_hw->plat->hif_sdio_ops->hi_desc_read((unsigned char *)(unsigned long)reg_data,
            (unsigned char *)(unsigned long)RG_WIFI_IF_FW2HST_IRQ_CFG , sizeof(reg_data));

    AML_RLMT_INFO("irq status 0x%08x, new pos 0x%08x\n", reg_data[1], reg_data[0]);
    return reg_data[1];
}

irqreturn_t aml_irq_sdio_hdlr(int irq, void *dev_id)
{
    struct aml_hw *aml_hw = (struct aml_hw *)dev_id;
    if (atomic_read(&g_wifi_pm.bus_suspend_cnt) || atomic_read(&g_wifi_pm.is_shut_down))
    {
        return IRQ_HANDLED;
    }

    if (aml_hw->irq_done)
    {
        aml_hw->irq_done = 0;
        up(&aml_hw->aml_irq_sem);
    }

    return IRQ_HANDLED;
}

void aml_irq_sdio_hdlr_for_pt(struct sdio_func *func)
{
    struct aml_hw *aml_hw = dev_get_drvdata(&func->dev);
    irq_handler_done = 0;

    if (atomic_read(&g_wifi_pm.bus_suspend_cnt) || atomic_read(&g_wifi_pm.is_shut_down))
    {
        irq_handler_done = 1;
        return;
    }

    sdio_release_host(func);
    if (aml_hw->irq_done)
    {
        aml_hw->irq_done = 0;
        //up(&aml_hw->aml_irq_sem);
        aml_dat1_irq_handler(aml_hw);
    }
    sdio_claim_host(func);

    irq_handler_done = 1;
    return;
}
#endif
void aml_irq_usb_hdlr(struct urb *urb)
{
    struct aml_hw *aml_hw = (struct aml_hw *)(urb->context);

    if (atomic_read(&g_wifi_pm.bus_suspend_cnt) || atomic_read(&g_wifi_pm.is_shut_down))
    {
        return;
    }
    urb->status = 0;
    up(&aml_hw->aml_irq_sem);
    return;
}

int aml_dat1_irq_handler(struct aml_hw *aml_hw)
{
    u32 status;
    int ret = 0;
    int try_cnt = 0;

    REG_SW_SET_PROFILING(aml_hw, SW_PROF_AML_IPC_IRQ_HDLR);

    while (status = aml_hw->plat->ack_irq(aml_hw)) {

        if (aml_hw->aml_irq_task_quit) {
            break;
        }

        ipc_host_irq(aml_hw->ipc_env, status);
        #ifdef CONFIG_SDIO_TX_ENH
        /* if irqless is enabled, read irq status once */
        if (aml_hw->irqless_flag)
            break;
        #endif
    }

    spin_lock_bh(&aml_hw->tx_lock);
    aml_hwq_process_all(aml_hw);
    spin_unlock_bh(&aml_hw->tx_lock);
#ifdef SDIO_MODE_ON
    if (aml_bus_type == SDIO_MODE) {
        aml_enable_sdio_irq(aml_hw);
    }
#endif
    REG_SW_CLEAR_PROFILING(aml_hw, SW_PROF_AML_IPC_IRQ_HDLR);

    return 0;
}

int aml_irq_task(void *data)
{
    struct aml_hw *aml_hw = (struct aml_hw *)data;
    u32 status;
    u32 urb_consume;
    int ret = 0;
    struct sched_param sch_param;
    int try_cnt = 0;

    sch_param.sched_priority = 93;
#ifndef CONFIG_PT_MODE
    sched_setscheduler(current, SCHED_FIFO, &sch_param);
#endif
    while (!aml_hw->aml_irq_task_quit) {
        /* wait for work */
        if (down_interruptible(&aml_hw->aml_irq_sem) != 0) {
            /* interrupted, exit */
            AML_RLMT_ERR("wait aml_task_sem fail!\n");
            break;
        }

        REG_SW_SET_PROFILING(aml_hw, SW_PROF_AML_IPC_IRQ_HDLR);
        if (aml_hw->aml_irq_task_quit) {
            break;
        }

        urb_consume = jiffies;
        while (status = aml_hw->plat->ack_irq(aml_hw)) {
            if (aml_hw->aml_irq_task_quit) {
                break;
            }

            ipc_host_irq(aml_hw->ipc_env, status);
#ifdef CONFIG_SDIO_TX_ENH
                    /* if irqless is enabled, read irq status once */
                    if (aml_hw->irqless_flag)
                        break;
#endif
        }

        spin_lock_bh(&aml_hw->tx_lock);
        aml_hwq_process_all(aml_hw);
        spin_unlock_bh(&aml_hw->tx_lock);

        if ((aml_bus_type == USB_MODE)
#ifdef CONFIG_AML_RECOVERY
        && !bus_state_detect.bus_err
#endif
        ) {
            //usleep_range(20, 30);
            USB_BEGIN_LOCK();
            if ((atomic_read(&g_wifi_pm.bus_suspend_cnt) == 0) && (atomic_read(&g_wifi_pm.is_shut_down) == 0) &&
                (atomic_read(&g_wifi_pm.drv_suspend_cnt) == 0)) {
                if (aml_hw->g_urb->status != -EINPROGRESS)
                {
                    if (!aml_hw->usb_rst_test) {
                        if (jiffies_to_msecs(jiffies - urb_consume) > 500)
                            AML_RLMT_ERR("urb consume %ld\n", jiffies_to_msecs(jiffies - urb_consume));

                        ret = usb_submit_urb(aml_hw->g_urb, GFP_ATOMIC);
                    }
                }
            } else {
                ret = 0;
            }
            USB_END_LOCK();
            if (ret < 0) {
                try_cnt++;
                ERROR_DEBUG_OUT("usb_submit_urb failed %d, bus_supend: %d, drv_suspend: %d\n",
                    ret, atomic_read(&g_wifi_pm.bus_suspend_cnt), atomic_read(&g_wifi_pm.drv_suspend_cnt));
                if (try_cnt < 5) {
                    if ((atomic_read(&g_wifi_pm.bus_suspend_cnt) == 0) && (atomic_read(&g_wifi_pm.is_shut_down) == 0) &&
                        (atomic_read(&g_wifi_pm.drv_suspend_cnt) == 0))
                        up(&aml_hw->aml_irq_sem);
                } else {
#ifdef CONFIG_AML_RECOVERY
                    //if ((atomic_read(&g_wifi_pm.bus_suspend_cnt) == 0) && (atomic_read(&g_wifi_pm.is_shut_down) == 0) &&
                    //    (atomic_read(&g_wifi_pm.drv_suspend_cnt) == 0))
                    //    bus_state_detect.bus_err = 1;

#endif
                    ERROR_DEBUG_OUT("usb_submit_urb failed(%d), try cnt %d\n", ret, try_cnt);
                }
            } else {
                try_cnt = 0;
            }
        }
#ifdef SDIO_MODE_ON
        else if(aml_bus_type == SDIO_MODE) {
            aml_enable_sdio_irq(aml_hw);
        }
#endif
        REG_SW_CLEAR_PROFILING(aml_hw, SW_PROF_AML_IPC_IRQ_HDLR);
    }
    if (aml_hw->aml_irq_completion_init) {
        aml_hw->aml_irq_completion_init = 0;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(5, 16, 20)
        complete_and_exit(&aml_hw->aml_irq_completion, 0);
#else
        complete(&aml_hw->aml_irq_completion);
#endif
    }

    return 0;
}


/**
 * aml_irq_hdlr - IRQ handler
 *
 * Handler registered by the platform driver
 */
irqreturn_t aml_irq_pcie_hdlr(int irq, void *dev_id)
{
    struct aml_hw *aml_hw = (struct aml_hw *)dev_id;

    if (atomic_read(&g_wifi_pm.bus_suspend_cnt) || atomic_read(&g_wifi_pm.is_shut_down))
    {
        return IRQ_HANDLED;
    }
    disable_irq_nosync(irq);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0) // template solution for S905L3A

#ifdef CONFIG_AML_USE_TASK
    up(&aml_hw->irqhdlr->task_sem);
#else
    tasklet_schedule(&aml_hw->task);
#endif

#else
    tasklet_schedule(&aml_hw->task);
#endif
    return IRQ_HANDLED;
}

/**
 * aml_task - Bottom half for IRQ handler
 *
 * Read irq status and process accordingly
 */
void aml_pcie_task(unsigned long data)
{
    struct aml_hw *aml_hw = (struct aml_hw *)data;
    struct aml_plat *aml_plat = aml_hw->plat;
    u32 status;

    REG_SW_SET_PROFILING(aml_hw, SW_PROF_AML_IPC_IRQ_HDLR);

    /* Ack unconditionally in case ipc_host_get_status does not see the irq */
    aml_plat->ack_irq(aml_hw);

    while ((status = ipc_host_get_status(aml_hw->ipc_env))) {
        /* All kinds of IRQs will be handled in one shot (RX, MSG, DBG, ...)
         * this will ack IPC irqs not the cfpga irqs */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0) // template solution for S905L3A
        ipc_host_irq(aml_hw->ipc_env, status);
#else
        ipc_host_irq_ext(aml_hw->ipc_env, status);
#endif

        aml_plat->ack_irq(aml_hw);
    }

    aml_spin_lock(&aml_hw->tx_lock);
    aml_hwq_process_all(aml_hw);
    aml_spin_unlock(&aml_hw->tx_lock);

    enable_irq(aml_platform_get_irq(aml_plat));
    REG_SW_CLEAR_PROFILING(aml_hw, SW_PROF_AML_IPC_IRQ_HDLR);
}
