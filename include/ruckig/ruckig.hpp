#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>
#include <math.h>
#include <numeric>
#include <optional>
#include <vector>

#include <ruckig/parameter.hpp>


namespace ruckig {

struct Profile {
    enum class Limits { ACC0_ACC1_VEL, VEL, ACC0, ACC1, ACC0_ACC1, ACC0_VEL, ACC1_VEL, NONE } limits;
    enum class Direction { UP, DOWN } direction;
    enum class Teeth { UDDU, UDUD } teeth;

    std::array<double, 7> t, t_sum, j;
    std::array<double, 8> a, v, p;

    //! Total time of the braking segments
    std::optional<double> t_brake;

    //! Allow up to two segments of braking before the "correct" profile starts
    std::array<double, 2> t_brakes, j_brakes, a_brakes, v_brakes, p_brakes;

    template<Teeth teeth>
    bool check(double pf, double vf, double af, double jf, double vMax, double aMax) {
        if constexpr (teeth == Teeth::UDDU) {
            j = {jf, 0, -jf, 0, -jf, 0, jf};
        } else {
            j = {jf, 0, -jf, 0, jf, 0, -jf};
        }

        t_sum[0] = t[0];
        if (t[0] < 0) {
            return false;
        }

        for (size_t i = 0; i < 6; i += 1) {
            if (t[i+1] < 0) {
                return false;
            }

            t_sum[i+1] = t_sum[i] + t[i+1];
        }
        for (size_t i = 0; i < 7; i += 1) {
            a[i+1] = a[i] + t[i] * j[i];
            v[i+1] = v[i] + t[i] * (a[i] + t[i] * j[i] / 2);
            p[i+1] = p[i] + t[i] * (v[i] + t[i] * (a[i] / 2 + t[i] * j[i] / 6));
        }

        // Velocity and acceleration limits can be broken in the beginning if the initial velocity and acceleration are too high
        // std::cout << std::setprecision(15) << "target: " << std::abs(p[7]-pf) << " " << std::abs(v[7] - vf) << " " << std::abs(a[7] - af) << std::endl;
        return std::all_of(v.begin() + 3, v.end(), [vMax](double vm){ return std::abs(vm) < std::abs(vMax) + 1e-9; })
            && std::all_of(a.begin() + 2, a.end(), [aMax](double am){ return std::abs(am) < std::abs(aMax) + 1e-9; })
            && std::abs(p[7] - pf) < 1e-8 && std::abs(v[7] - vf) < 1e-8 && std::abs(a[7] - af) < 1e-8;
    }
    
    template<Teeth teeth>
    inline bool check(double tf, double pf, double vf, double af, double jf, double vMax, double aMax) {
        // std::cout << std::setprecision(15) << "target: " << std::abs(t_sum[6]-tf) << " " << std::abs(p[7]-pf) << " " << std::abs(v[7] - vf) << " " << std::abs(a[7] - af) << std::endl;
        return check<teeth>(pf, vf, af, jf, vMax, aMax) && (std::abs(t_sum[6] - tf) < 1e-8);
    }
    
    template<Teeth teeth>
    inline bool check(double tf, double pf, double vf, double af, double jf, double vMax, double aMax, double jMax) {
        return (std::abs(jf) < std::abs(jMax) + 1e-12) && check<teeth>(tf, pf, vf, af, jf, vMax, aMax);
    }

    //! Integrate with constant jerk for duration t. Returns new position, new velocity, and new acceleration.
    static std::tuple<double, double, double> integrate(double t, double p0, double v0, double a0, double j);

    std::string to_string() const;
};


//! Which times are possible for synchronization?
struct Block {
    struct Interval {
        double left, right; // [s]
    };

    double t_min; // [s]
    Profile p_min;

    std::optional<Interval> a, b; // Max. 2 intervals can be blocked
    std::optional<Profile> p_a, p_b;

    bool is_blocked(double t) const {
        return (t < t_min) || (a && a->left < t && t < a->right) || (b && b->left < t && t < b->right);
    }
};


//! Calculates (pre-) trajectory to get current state below the limits
class Brake {
    static constexpr double eps {2e-15};

    static void acceleration_brake(double v0, double a0, double vMax, double aMax, double jMax, std::array<double, 2>& t_brake, std::array<double, 2>& j_brake);
    static void velocity_brake(double v0, double a0, double vMax, double aMax, double jMax, std::array<double, 2>& t_brake, std::array<double, 2>& j_brake);

public:
    static void get_brake_trajectory(double v0, double a0, double vMax, double aMax, double jMax, std::array<double, 2>& t_brake, std::array<double, 2>& j_brake);
};


class Step1 {
    using Limits = Profile::Limits;
    using Teeth = Profile::Teeth;

    double p0, v0, a0;
    double pf, vf, af;

    // Pre-calculated expressions
    double pd;
    double v0_v0, vf_vf, vMax_vMax;
    double a0_a0, af_af, aMax_aMax;
    double jMax_jMax;

    std::vector<Profile> valid_profiles;

    void add_profile(Profile profile, Limits limits, double jMax);

    void time_up_acc0_acc1_vel(Profile& profile, double vMax, double aMax, double jMax);
    void time_up_acc1_vel(Profile& profile, double vMax, double aMax, double jMax);
    void time_up_acc0_vel(Profile& profile, double vMax, double aMax, double jMax);
    void time_up_vel(Profile& profile, double vMax, double aMax, double jMax);
    void time_up_acc0_acc1(Profile& profile, double vMax, double aMax, double jMax);
    void time_up_acc1(Profile& profile, double vMax, double aMax, double jMax);
    void time_up_acc0(Profile& profile, double vMax, double aMax, double jMax);
    void time_up_none(Profile& profile, double vMax, double aMax, double jMax);

    void time_down_acc0_acc1_vel(Profile& profile, double vMax, double aMax, double jMax);
    void time_down_acc1_vel(Profile& profile, double vMax, double aMax, double jMax);
    void time_down_acc0_vel(Profile& profile, double vMax, double aMax, double jMax);
    void time_down_vel(Profile& profile, double vMax, double aMax, double jMax);
    void time_down_acc0_acc1(Profile& profile, double vMax, double aMax, double jMax);
    void time_down_acc1(Profile& profile, double vMax, double aMax, double jMax);
    void time_down_acc0(Profile& profile, double vMax, double aMax, double jMax);
    void time_down_none(Profile& profile, double vMax, double aMax, double jMax);

public:
    Block block;

    explicit Step1(double p0, double v0, double a0, double pf, double vf, double af, double vMax, double aMax, double jMax);

    bool get_profile(const Profile& input, double vMax, double aMax, double jMax);
};


class Step2 {
    using Teeth = Profile::Teeth;

    double tf;
    double p0, v0, a0;
    double pf, vf, af;

    bool time_up_acc0_acc1_vel(Profile& profile, double vMax, double aMax, double jMax);
    bool time_up_acc1_vel(Profile& profile, double vMax, double aMax, double jMax);
    bool time_up_acc0_vel(Profile& profile, double vMax, double aMax, double jMax);
    bool time_up_vel(Profile& profile, double vMax, double aMax, double jMax);
    bool time_up_acc0_acc1(Profile& profile, double vMax, double aMax, double jMax);
    bool time_up_acc1(Profile& profile, double vMax, double aMax, double jMax);
    bool time_up_acc0(Profile& profile, double vMax, double aMax, double jMax);
    bool time_up_none(Profile& profile, double vMax, double aMax, double jMax);

    bool time_down_acc0_acc1_vel(Profile& profile, double vMax, double aMax, double jMax);
    bool time_down_acc1_vel(Profile& profile, double vMax, double aMax, double jMax);
    bool time_down_acc0_vel(Profile& profile, double vMax, double aMax, double jMax);
    bool time_down_vel(Profile& profile, double vMax, double aMax, double jMax);
    bool time_down_acc0_acc1(Profile& profile, double vMax, double aMax, double jMax);
    bool time_down_acc1(Profile& profile, double vMax, double aMax, double jMax);
    bool time_down_acc0(Profile& profile, double vMax, double aMax, double jMax);
    bool time_down_none(Profile& profile, double vMax, double aMax, double jMax);

public:
    explicit Step2(double tf, double p0, double v0, double a0, double pf, double vf, double af, double vMax, double aMax, double jMax);

    bool get_profile(Profile& profile, double vMax, double aMax, double jMax);
};


template<size_t DOFs>
class Ruckig {
    InputParameter<DOFs> current_input;

    double t, tf;
    std::array<Profile, DOFs> profiles;

    bool synchronize(const std::array<Block, DOFs>& blocks, std::optional<double> t_min, double& t_sync, int& limiting_dof, std::array<Profile, DOFs>& profiles) {
        if (DOFs == 1 && !t_min) {
            limiting_dof = 0;
            t_sync = blocks[0].t_min;
            profiles[limiting_dof] = blocks[0].p_min;
            return true;
        }

        // Possible t_syncs are the start times of the intervals
        std::array<double, 3*DOFs> possible_t_syncs;
        for (size_t dof = 0; dof < DOFs; ++dof) {
            auto& block = blocks[dof];
            possible_t_syncs[3 * dof] = block.t_min;
            possible_t_syncs[3 * dof + 1] = block.a ? block.a->right : std::numeric_limits<double>::infinity();
            possible_t_syncs[3 * dof + 2] = block.b ? block.b->right : std::numeric_limits<double>::infinity();
        }

        // Test them in sorted order
        std::array<size_t, 3*DOFs> idx;
        std::iota(idx.begin(), idx.end(), 0);
        std::stable_sort(idx.begin(), idx.end(), [&possible_t_syncs](size_t i, size_t j) { return possible_t_syncs[i] < possible_t_syncs[j]; });

        for (size_t i: idx) {
            double possible_t_sync = possible_t_syncs[i];
            if (std::any_of(blocks.begin(), blocks.end(), [possible_t_sync](auto block){ return block.is_blocked(possible_t_sync); }) || possible_t_sync < t_min.value_or(0.0)) {
                continue;
            }

            t_sync = possible_t_sync;
            limiting_dof = std::ceil((i + 1.0) / 3) - 1;
            // std::cout << "sync: " << limiting_dof << " " << i % 3 << " " << t_sync << std::endl;
            switch (i % 3) {
                case 0: {
                    profiles[limiting_dof] = blocks[limiting_dof].p_min;
                } break;
                case 1: {
                    profiles[limiting_dof] = blocks[limiting_dof].p_a.value();
                } break;
                case 2: {
                    profiles[limiting_dof] = blocks[limiting_dof].p_b.value();
                } break;
            }
            return true;
        }

        return false;
    }

    Result calculate(const InputParameter<DOFs>& input, OutputParameter<DOFs>& output) {
        // auto start = std::chrono::high_resolution_clock::now();
        current_input = input;

        // std::cout << "reference: " << std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - start).count() / 1000.0 << std::endl;

        if (!validate_input(input)) {
            return Result::ErrorInvalidInput;
        }

        // std::cout << "validate: " << std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - start).count() / 1000.0 << std::endl;

        std::array<Block, DOFs> blocks;
        std::array<double, DOFs> p0s, v0s, a0s; // Starting point of profiles without brake trajectory
        for (size_t dof = 0; dof < DOFs; ++dof) {
            if (!input.enabled[dof]) {
                continue;
            }

            // Calculate brakes (if input exceeds or will exceed limits)
            Brake::get_brake_trajectory(input.current_velocity[dof], input.current_acceleration[dof], input.max_velocity[dof], input.max_acceleration[dof], input.max_jerk[dof], profiles[dof].t_brakes, profiles[dof].j_brakes);
            profiles[dof].t_brake = profiles[dof].t_brakes[0] + profiles[dof].t_brakes[1];

            p0s[dof] = input.current_position[dof];
            v0s[dof] = input.current_velocity[dof];
            a0s[dof] = input.current_acceleration[dof];

            if (profiles[dof].t_brakes[0] > 0.0) {
                profiles[dof].p_brakes[0] = p0s[dof];
                profiles[dof].v_brakes[0] = v0s[dof];
                profiles[dof].a_brakes[0] = a0s[dof];
                std::tie(p0s[dof], v0s[dof], a0s[dof]) = Profile::integrate(profiles[dof].t_brakes[0], p0s[dof], v0s[dof], a0s[dof], profiles[dof].j_brakes[0]);

                if (profiles[dof].t_brakes[1] > 0.0) {
                    profiles[dof].p_brakes[1] = p0s[dof];
                    profiles[dof].v_brakes[1] = v0s[dof];
                    profiles[dof].a_brakes[1] = a0s[dof];
                    std::tie(p0s[dof], v0s[dof], a0s[dof]) = Profile::integrate(profiles[dof].t_brakes[1], p0s[dof], v0s[dof], a0s[dof], profiles[dof].j_brakes[1]);
                }
            }

            Step1 step1 {p0s[dof], v0s[dof], a0s[dof], input.target_position[dof], input.target_velocity[dof], input.target_acceleration[dof], input.max_velocity[dof], input.max_acceleration[dof], input.max_jerk[dof]};
            bool found_profile = step1.get_profile(profiles[dof], input.max_velocity[dof], input.max_acceleration[dof], input.max_jerk[dof]);
            if (!found_profile) {
                throw std::runtime_error("[ruckig] error in step 1: " + input.to_string(dof) + " all: " + input.to_string());
                return Result::ErrorExecutionTimeCalculation;
            }

            blocks[dof] = step1.block;
            output.independent_min_durations[dof] = step1.block.t_min;
        }

        // std::cout << "step1: " << std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - start).count() / 1000.0 << std::endl;

        int limiting_dof; // The DoF that doesn't need step 2
        bool found_synchronization = synchronize(blocks, input.minimum_duration, tf, limiting_dof, profiles);
        if (!found_synchronization) {
            throw std::runtime_error("[ruckig] error in time synchronization: " + std::to_string(tf));
            return Result::ErrorSynchronizationCalculation;
        }
        
        if (tf > 0.0) {
            for (size_t dof = 0; dof < DOFs; ++dof) {
                if (!input.enabled[dof] || dof == limiting_dof) {
                    continue;
                }

                double t_profile = tf - profiles[dof].t_brake.value_or(0.0);

                Step2 step2 {t_profile, p0s[dof], v0s[dof], a0s[dof], input.target_position[dof], input.target_velocity[dof], input.target_acceleration[dof], input.max_velocity[dof], input.max_acceleration[dof], input.max_jerk[dof]};
                bool found_time_synchronization = step2.get_profile(profiles[dof], input.max_velocity[dof], input.max_acceleration[dof], input.max_jerk[dof]);
                if (!found_time_synchronization) {
                    throw std::runtime_error("[ruckig] error in step 2 in dof: " + std::to_string(dof) + " for t sync: " + std::to_string(tf) + " | " + input.to_string(dof) + " all: " + input.to_string());
                    return Result::ErrorSynchronizationCalculation;
                }
            }
        }

        // std::cout << "step2: " << std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - start).count() / 1000.0 << std::endl;

        t = 0.0;
        output.duration = tf;
        output.new_calculation = true;
        return Result::Working;
    }

public:
    //! Just a shorter notation
    using Input = InputParameter<DOFs>;
    using Output = OutputParameter<DOFs>;

    //! Time step between updates (cycle time) in [s]
    const double delta_time;

    explicit Ruckig(double delta_time): delta_time(delta_time) { }

    bool validate_input(const InputParameter<DOFs>& input) {
        for (size_t dof = 0; dof < DOFs; ++dof) {
            if (input.max_velocity[dof] <= 0.0) {
                std::cerr << "Velocity limit needs to be positive." << std::endl;
                return false;
            }

            if (input.max_acceleration[dof] <= 0.0) {
                std::cerr << "Acceleration limit needs to be positive." << std::endl;
                return false;
            }

            if (input.max_jerk[dof] <= 0.0) {
                std::cerr << "Jerk limit needs to be positive." << std::endl;
                return false;
            }

            if (input.target_velocity[dof] > input.max_velocity[dof]) {
                std::cerr << "Target velocity exceeds velocity limit." << std::endl;
                return false;
            }

            if (input.target_acceleration[dof] > input.max_acceleration[dof]) {
                std::cerr << "Target acceleration exceeds acceleration limit." << std::endl;
                return false;
            }

            double max_target_acceleration = std::sqrt(2 * input.max_jerk[dof] * (input.max_velocity[dof] - std::abs(input.target_velocity[dof])));
            if (std::abs(input.target_acceleration[dof]) > max_target_acceleration) {
                std::cerr << "Target acceleration exceeds maximal possible acceleration." << std::endl;
                return false;
            }
        }

        return true;
    }

    Result update(const InputParameter<DOFs>& input, OutputParameter<DOFs>& output) {
        auto start = std::chrono::high_resolution_clock::now();

        t += delta_time;
        output.new_calculation = false;

        if (input != current_input && Result::Working != calculate(input, output)) {
            return Result::Error;
        }

        at_time(t, output);

        auto stop = std::chrono::high_resolution_clock::now();
        output.calculation_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start).count() / 1000.0;

        if (t + delta_time > tf) {
            return Result::Finished;
        }

        current_input.current_position = output.new_position;
        current_input.current_velocity = output.new_velocity;
        current_input.current_acceleration = output.new_acceleration;
        return Result::Working;
    }

    void at_time(double time, OutputParameter<DOFs>& output) {
        if (time + delta_time > tf) {
            // Keep constant acceleration
            for (size_t dof = 0; dof < DOFs; ++dof) {
                std::tie(output.new_position[dof], output.new_velocity[dof], output.new_acceleration[dof]) = Profile::integrate(time - tf, current_input.target_position[dof], current_input.target_velocity[dof], current_input.target_acceleration[dof], 0);
            }
            return;
        }

        for (size_t dof = 0; dof < DOFs; ++dof) {
            if (!current_input.enabled[dof]) {
                std::tie(output.new_position[dof], output.new_velocity[dof], output.new_acceleration[dof]) = Profile::integrate(time, current_input.current_position[dof], current_input.current_velocity[dof], current_input.current_acceleration[dof], 0);
            }

            auto& p = profiles[dof];

            double t_diff = time;
            if (p.t_brake.has_value()) {
                if (t_diff < p.t_brake.value()) {
                    size_t index = (t_diff < p.t_brakes[0]) ? 0 : 1;
                    if (index > 0) {
                        t_diff -= p.t_brakes[index - 1];
                    }

                    std::tie(output.new_position[dof], output.new_velocity[dof], output.new_acceleration[dof]) = Profile::integrate(t_diff, p.p_brakes[index], p.v_brakes[index], p.a_brakes[index], p.j_brakes[index]);
                    continue;
                } else {
                    t_diff -= p.t_brake.value();
                }
            }

            // Non-time synchronization
            if (t_diff >= p.t_sum[6]) {
                output.new_position[dof] = p.p[7];
                output.new_velocity[dof] = p.v[7];
                output.new_acceleration[dof] = p.a[7];
                continue;
            }

            auto index_ptr = std::upper_bound(p.t_sum.begin(), p.t_sum.end(), t_diff);
            size_t index = std::distance(p.t_sum.begin(), index_ptr);

            if (index > 0) {
                t_diff -= p.t_sum[index - 1];
            }

            std::tie(output.new_position[dof], output.new_velocity[dof], output.new_acceleration[dof]) = Profile::integrate(t_diff, p.p[index], p.v[index], p.a[index], p.j[index]);
        }
    }
};

} // namespace ruckig
