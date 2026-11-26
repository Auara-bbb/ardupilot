#include "Copter.h"

#ifdef USERHOOK_INIT
void Copter::userhook_init()
{
    // put your initialisation code here
    // this will be called once at start-up
}
#endif

#ifdef USERHOOK_FASTLOOP
void Copter::userhook_FastLoop()
{
    // put your 100Hz code here
}
#endif

#ifdef USERHOOK_50HZLOOP
void Copter::userhook_50Hz()
{
    // put your 50Hz code here
}
#endif

#ifdef USERHOOK_MEDIUMLOOP
void Copter::userhook_MediumLoop()
{
    // put your 10Hz code here
}
#endif

#ifdef USERHOOK_SLOWLOOP
void Copter::userhook_SlowLoop()
{
    // put your 3.3Hz code here
}
#endif

#ifdef USERHOOK_SUPERSLOWLOOP
void Copter::userhook_SuperSlowLoop()
{
    // put your 1Hz code here
}
#endif

#ifdef USERHOOK_AUXSWITCH
void Copter::userhook_auxSwitch1(uint8_t ch_flag)
{
#if MODE_GUIDED_ENABLED == ENABLED
    // Require a low position before accepting high. A three-position switch may pass through middle.
    if (ch_flag == AUX_SWITCH_LOW) {
        manual_return_switch_low = true;
        return;
    }
    if (ch_flag != AUX_SWITCH_HIGH || !manual_return_switch_low) {
        return;
    }
    manual_return_switch_low = false;

    if (!motors->armed() || ap.land_complete) {
        return;
    }
    if (manual_return_count >= MANUAL_RETURN_MAX_POINTS) {
        gcs().send_text(MAV_SEVERITY_WARNING, "Return route is full");
        return;
    }

    Location_Class point;
    if (!position_ok() || !ekf_position_ok() || !ahrs.get_position(point)) {
        gcs().send_text(MAV_SEVERITY_WARNING, "Return point requires an absolute position");
        return;
    }
    point.set_alt_cm(point.alt, Location_Class::ALT_FRAME_ABSOLUTE);
    manual_return_points[manual_return_count++] = point;
    gcs().send_text(MAV_SEVERITY_INFO, "Return point %u saved", unsigned(manual_return_count));
#endif
}

#if MODE_GUIDED_ENABLED == ENABLED
void Copter::start_manual_return()
{
    if (motors->armed() && !ap.land_complete && manual_return_count != 0) {
        set_manual_return_target(manual_return_count);
    }
}

void Copter::update_manual_return()
{
    if (manual_return_remaining == 0) {
        return;
    }
    if (!motors->armed() || ap.land_complete || mode_guided.mode() != Guided_WP) {
        manual_return_remaining = 0;
        return;
    }
    if (!wp_nav->reached_wp_destination()) {
        return;
    }
    if (manual_return_remaining == 1) {
        manual_return_remaining = 0;
        gcs().send_text(MAV_SEVERITY_INFO, "Return route complete");
        return;
    }
    set_manual_return_target(manual_return_remaining - 1);
}

void Copter::set_manual_return_target(uint8_t remaining)
{
    if (!position_ok() || !mode_guided.set_destination(manual_return_points[remaining - 1])) {
        manual_return_remaining = 0;
        gcs().send_text(MAV_SEVERITY_WARNING, "Return route stopped: waypoint unavailable");
        return;
    }

    // Guided accepts external position targets too. Only a route target advances this index.
    manual_return_remaining = remaining;
}
#endif

void Copter::userhook_auxSwitch2(uint8_t ch_flag)
{
    // put your aux switch #2 handler here (CHx_OPT = 48)
}

void Copter::userhook_auxSwitch3(uint8_t ch_flag)
{
    // put your aux switch #3 handler here (CHx_OPT = 49)
}
#endif
