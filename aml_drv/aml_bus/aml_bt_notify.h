#ifndef _AML_BT_NOTIFY_H_
#define _AML_BT_NOTIFY_H_

int notify_bt_event(int event);
int register_bt_event_notifier(struct notifier_block *nb);
int unregister_bt_event_notifier(struct notifier_block *nb);

#endif//_AML_BT_NOTIFY_H_
