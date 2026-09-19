#ifndef _BLE_H_
#define _BLE_H_

void BLE_Init(void);
void BLE_StartAdvertise(void);
void BLE_StopAdverting();
void BLE_AddServiceData(uint8_t *data, uint8_t length);
void BLE_RemoveServiceData(void);
uint8_t BLE_AdvStatus(void);

#endif /* _BLE_H_ */