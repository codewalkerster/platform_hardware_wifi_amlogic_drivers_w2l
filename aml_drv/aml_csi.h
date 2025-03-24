#ifndef _AML_CSI_H_
#define _AML_CSI_H_

#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/workqueue.h>

enum {
    AML_CSI_FUNC_START = 0xFF01,
    AML_CSI_FUNC_STOP,
    AML_CSI_DATA_UPLOAD,
};

int aml_csi_nl_init(void);
void aml_csi_nl_destroy(void);
int aml_send_csi_data_to_user(char *pbuf, uint16_t len, int msg_type);

#endif /* _AML_CSI_H_ */
