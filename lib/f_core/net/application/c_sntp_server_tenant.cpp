#include "f_core/net/application/c_sntp_server_tenant.h"

#include <zephyr/drivers/rtc.h>

void CSntpServerTenant::Startup() {}

void CSntpServerTenant::Cleanup() {}

void CSntpServerTenant::Run() {
    uint32_t packet[32] = {0};
    socklen_t srcAddrLen = sizeof(srcAddr);

    int rxLen = sock.ReceiveAsynchronous(packet, 32, &srcAddr, &srcAddrLen);

    if (rxLen == 0) {
        return;
    } else if (rxLen < 0) {
        LOG_ERR("Failed to receive packet (%d)", rxLen);
        return;
    }


    sockaddr srcAddr{0};
    rtc_time time{0};
    uint8_t li = NO_WARNING;

    int ret = rtc_get_time(&rtcDevice, &time);
    if (ret != 0) {
        if (ret == -ENODATA) {
            LOG_ERR("RTC time not set");
            li = ALARM_CONDITION;
        } else {
            LOG_ERR("RTC time get failed");
        }
    }

    uint32_t lastUpdateTimeSeconds = time.tm_sec + time.tm_min * 60 + time.tm_hour * 3600;
    uint32_t lastUpdateTimeNanoseconds = time.tm_nsec;

    uint8_t li_vn_mode = (li << LEAP_INDICATOR_BIT_OFFSET | SERVER_VERSION_NUMBER << VERSION_NUMBER_BIT_OFFSET | 4);
    SntpPacket packet = {
        .li_vn_mode = li_vn_mode,
        .stratum = stratum,
        .poll = pollInterval,
        .precision = precisionExponent,
        .root_delay = 0, // Unknown, but very small with the assumption Ethernet is used
        .root_dispersion = 0.0005, // 0.5 ms
        .reference_id = GPS_REFERENCE_CODE, // Currently only GPS is the expected reference (stratum 1)
        .reference_timestamp = lastUpdateTimeSeconds << 32 | lastUpdateTimeNanoseconds,
        .originate_timestamp = 0, // TODO

    };







    
}
