#ifndef C_SENSOR_MODULE_H
#define C_SENSOR_MODULE_H

#include "c_sensing_tenant.h"
#include "c_broadcast_tenant.h"

// F-Core Includes
#include <f_core/c_project_configuration.h>
#include <f_core/os/c_task.h>
#include <f_core/os/c_tenant.h>

class CSensorModule : public CProjectConfiguration {
public:
    /**
     * Constructor
     */
    CSensorModule();

private:
    // Tenants
    CSensingTenant sensingTenant{"Sensing Tenant"};
    CBroadcastTenant broadcastTenant{"Broadcast Tenant"};

    // Tasks
    CTask networkTask{"Networking Task", 15, 128, 0};
    CTask sensingTask{"Sensing Task", 15, 128, 0};

    // TODO: Messaging handlers to pass into tenants

    /**
     * See parent docs
     */
    void addTenantsToTasks() override;

    /**
     * See parent docs
     */
    void addTasksToRtos() override;

    /**
     * See parent docs
     */
    void setupCallbacks() override;
};



#endif //C_SENSOR_MODULE_H
