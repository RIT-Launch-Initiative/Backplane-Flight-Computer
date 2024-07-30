/*
* Copyright (c) 2024 RIT Launch Initiative
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "c_sensing_tenant.h"
#include "c_broadcast_tenant.h"

#include <f_core/os/c_task.h>
#include <f_core/os/n_rtos.h>

int main() {
    // Networking Subsystem
    static CBroadcastTenant broadcastTenant = CBroadcastTenant("Broadcast Tenant");
    static CTask networkTask("Networking Task", 15, 512, 0);
    networkTask.AddTenant(broadcastTenant);

    // Sensing Subsystem
    static CSensingTenant sensingTenant = CSensingTenant("Sensing Tenant");
    static CTask sensingTask("Sensing Task", 15, 512, 0);
    sensingTask.AddTenant(sensingTenant);

    // Add tasks and start RTOS
    NRtos::AddTask(networkTask);
    NRtos::StartRtos();

#ifdef CONFIG_ARCH_POSIX
    k_sleep(K_SECONDS(10));
    NRtos::StopRtos();
#endif

    return 0;
}

