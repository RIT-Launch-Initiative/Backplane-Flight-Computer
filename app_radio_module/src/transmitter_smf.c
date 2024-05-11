#include "transmitter_smf.h"
#include "transmitter_gnss.h"
#include "radio_module_functionality.h"

#include <launch_core/net/udp.h>
#include <launch_core/backplane_defs.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(transmitter_smf);

// External variables
extern struct k_timer gnss_tx_timer;

// State Machine Variables
#define DEFINE_STATE_FUNCTIONS(state_name)          \
static void state_name##_state_entry(void *);       \
static void state_name##_state_run(void *);         \
static void state_name##_state_exit(void *);

DEFINE_STATE_FUNCTIONS(ground);

DEFINE_STATE_FUNCTIONS(flight);

static const struct smf_state transmitter_states[] = {
        [GROUND_STATE] = SMF_CREATE_STATE(ground_state_entry, ground_state_run, ground_state_exit),
        [FLIGHT_STATE] = SMF_CREATE_STATE(flight_state_entry, flight_state_run, flight_state_exit),
};

struct s_object {
    struct smf_ctx ctx;
    bool boost_detected;
} state_obj;

extern float gnss_altitude;

static void boost_detector(struct k_timer *timer_id) {
    static bool first_pass = true;
    static const uint32_t BOOST_THRESHOLD_FT = 500;
    static uint32_t prev_altitude = 0;

    // TODO: Check behavior without lock
    if (first_pass) {
        first_pass = false;
        prev_altitude = gnss_altitude;
    }

    if ((gnss_altitude - prev_altitude) < BOOST_THRESHOLD_FT) {
        prev_altitude = gnss_altitude;
        return;
    }

    state_obj.boost_detected = true;
}

K_TIMER_DEFINE(boost_detect_timer, boost_detector, NULL);

static uint8_t get_event_notification() {
    static bool initialized = false;

    // TODO: Not the biggest fan of this code. Refactor this. Along with the extern gnss var :P
    static int sock = -1;
    if (!initialized) {
        initialized = true;
        int sock = l_init_udp_socket(RADIO_MODULE_IP_ADDR, LAUNCH_EVENT_NOTIFICATION_PORT);
        l_set_socket_rx_timeout(sock, 1);
    }

    uint8_t notif = 0;

    if (l_receive_udp(sock, &notif, 1) == 1) {
        return notif;
    }

    return 0;
}

static void ground_state_entry(void *) {
    LOG_INF("Entered ground state");
    k_timer_start(&boost_detect_timer, K_SECONDS(5), K_SECONDS(5));
    config_gnss_tx_time(K_SECONDS(5));
    state_obj.boost_detected = false;
}

static void ground_state_run(void *) {
    while (true) {
        // If GNSS altitude changes, notify everyone and go to flight state
        if (state_obj.boost_detected) {
            smf_set_state(SMF_CTX(&state_obj), &transmitter_states[FLIGHT_STATE]);
            l_send_udp_broadcast(sock, &notif, 1, LAUNCH_EVENT_NOTIFICATION_PORT);
            return;
        }

        // Check port 9999 for notifications. If we get one, go to flight state on next iter
        state_obj.boost_detected = get_event_notification() == L_BOOST_DETECTED;
    }
}

static void ground_state_exit(void *) {
    k_timer_stop(&boost_detect_timer);
    return;
}

static void flight_state_entry(void *) {
    LOG_INF("Entered flight state");
}

static void flight_state_run(void *) {
    while (true) {
        // Listen to all ports
        udp_to_lora((int *) &udp_socket_list);

        // If notified of landing, go back to ground state.

        // If timer expires, dump data over LoRa
    }
}

static void flight_state_exit(void *) {
    // Stop timer
}

void init_state_machine() {
    smf_set_initial(SMF_CTX(&state_obj), &transmitter_states[GROUND_STATE]);
}

void run_state_machine() {
    static int ret = 0;
    if (ret == 0) {
        ret = smf_run_state(SMF_CTX(&state_obj));
        if (ret < 0) {
            LOG_ERR("Failed to run state machine: %d", ret);
        }
    }
}