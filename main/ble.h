#ifndef _BLE_H_
#define _BLE_H_

void BLE_Init(void);
void BLE_SendAdvertise(void);
void BLE_AddServiceData(uint8_t *data, uint8_t length);
void BLE_RemoveServiceData(void);

#endif /* _BLE_H_ */