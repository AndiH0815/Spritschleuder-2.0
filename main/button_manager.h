#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

class ButtonManager {
public:
    static ButtonManager &getInstance();
    void init();
private:
    ButtonManager() = default;
    static void task(void *);
    TaskHandle_t task_handle = nullptr;
};
