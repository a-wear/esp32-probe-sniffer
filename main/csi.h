#ifndef CSI_H
#define CSI_H

#include <stdio.h>
#include "esp_err.h"

void initialize_csi(void);
esp_err_t csi_start(void);
esp_err_t csi_stop(void);
void open_csi_file(uint32_t idx);
void close_csi_file(void);


#endif