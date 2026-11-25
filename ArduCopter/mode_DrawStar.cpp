#include "Copter.h"

bool Copter::ModeDrawStar::init(bool ignore_checks)
{
    if (!copter.position_ok() && !ignore_checks) {
        return false;
    }

    wp_nav->wp_and_spline_init();
    path_num = 0;
    generate_path();
    auto_yaw.set_mode_to_default(false);
    return wp_nav->set_wp_destination(path[0], false);
}

void Copter::ModeDrawStar::generate_path()
{
    const float radius_cm = g2.star_radius_cm.get();
    constexpr float step_deg = -720.0f / vertex_count;

    wp_nav->get_wp_stopping_point(path[0]);

    // Skip one vertex at each step to draw a five-point star.
    for (uint8_t i = 1; i <= vertex_count; i++) {
        const float angle = radians(step_deg * (i - 1));
        path[i] = path[0] + Vector3f(cosf(angle), sinf(angle), 0.0f) * radius_cm;
    }
    path[vertex_count + 1] = path[1];
}

void Copter::ModeDrawStar::run()
{
    // if not auto armed or motors not enabled set throttle to zero and exit immediately
    if (!motors->armed() || !ap.auto_armed || !motors->get_interlock() || ap.land_complete) {
        zero_throttle_and_relax_ac();
        return;
    }

    if (path_num < vertex_count + 1 && wp_nav->reached_wp_destination()) {
        path_num++;
        wp_nav->set_wp_destination(path[path_num], false);
    }

    // process pilot's yaw input
    float target_yaw_rate = 0;
    if (!copter.failsafe.radio) {
        // get pilot's desired yaw rate
        target_yaw_rate = get_pilot_desired_yaw_rate(channel_yaw->get_control_in());
        if (!is_zero(target_yaw_rate)) {
            auto_yaw.set_mode(AUTO_YAW_HOLD);
        }
    }

    // set motors to full range
    motors->set_desired_spool_state(AP_Motors::DESIRED_THROTTLE_UNLIMITED);

    // run waypoint controller
    copter.failsafe_terrain_set_status(wp_nav->update_wpnav());

    // call z-axis position controller (wpnav should have already updated it's alt target)
    pos_control->update_z_controller();

    // call attitude controller
    if (auto_yaw.mode() == AUTO_YAW_HOLD) {
        // roll & pitch from waypoint controller, yaw rate from pilot
        attitude_control->input_euler_angle_roll_pitch_euler_rate_yaw(wp_nav->get_roll(), wp_nav->get_pitch(), target_yaw_rate);
    } else if (auto_yaw.mode() == AUTO_YAW_RATE) {
        // roll & pitch from waypoint controller, yaw rate from mavlink command or mission item
        attitude_control->input_euler_angle_roll_pitch_euler_rate_yaw(wp_nav->get_roll(), wp_nav->get_pitch(), auto_yaw.rate_cds());
    } else {
        // roll, pitch from waypoint controller, yaw heading from GCS or auto_heading()
        attitude_control->input_euler_angle_roll_pitch_yaw(wp_nav->get_roll(), wp_nav->get_pitch(), auto_yaw.yaw(), true);
    }
}
