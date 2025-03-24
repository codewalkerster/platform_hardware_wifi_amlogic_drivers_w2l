#ifndef _AML_INTERFACE_H_
#define _AML_INTERFACE_H_

#define AML_SDIO_STATE_MON_INTERVAL   (5 *HZ)
#define ICCM_RAM_LEN (192 * 1024)
#define ICCM_ROM_LEN (256 * 1024)
#define ICCM_ALL_LEN (ICCM_RAM_LEN)
#define DCCM_ALL_LEN (192 * 1024)
#define ICCM_ROM_ADDR (0x00100000)
#define ICCM_RAM_ADDR (0x00100000 + ICCM_ROM_LEN)
#define DCCM_RAM_ADDR (0x00d00000)
#define DCCM_RAM_OFFSET (0x00700000) //0x00800000 - 0x00100000, in fw_flash
#define BYTE_IN_LINE (9)
#define ICCM_BUFFER_RD_LEN  (ICCM_RAM_LEN)

#define ICCM_CHECK_LEN      (ICCM_RAM_LEN)
#define DCCM_CHECK_LEN      (DCCM_ALL_LEN)
#define RAM_BIN_LEN (1024 * 512 * 2)

#define RF_FW (0)
#define SUSPEND_FW (1)

enum interface_type {
    SDIO_MODE,
    USB_MODE,
    PCIE_MODE
};

enum custom_version {
    DEFAULT_VER,
    ROKU_VER
};

struct aml_bus_state_detect {
  unsigned char bus_err;
  unsigned char is_drv_load_finished;
  unsigned char bus_reset_ongoing;
  unsigned char is_load_by_timer;
  unsigned char is_recy_ongoing;
  struct timer_list timer;
  struct work_struct detect_work;
  int (*insmod_drv)(void);
};

struct aml_pm_type {
    atomic_t bus_suspend_cnt;
    atomic_t drv_suspend_cnt;
    atomic_t is_shut_down;
    atomic_t wifi_enable;
};

typedef void (*bt_shutdown_func)(void);
typedef void (*lp_shutdown_func)(void);
#endif
