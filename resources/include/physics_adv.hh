#ifndef PHYSICS_ADV_HH
#define PHYSICS_ADV_HH

#include <glm/vec3.hpp> 
#include <glm/gtc/quaternion.hpp>
#include <glm/ext/matrix_transform.hpp>

namespace fcg 
{
    class Airplane {
        public: 
            glm::vec3 position {};
            glm::quat orientation {};
        private:
            const float MAX_SPEED = 10.0f;
            const float GRAVITY_FORCE = 1.0f; // a force that pulls down the aircraft
            const float CENTERING_SPEED = 1.2f; // auto-leveling
            const float WEIGHT_FEEL = 5.0f;

            unsigned int throttle_level = 3; // [0, 5]
            float current_velocity = (throttle_level / 5.0f) * MAX_SPEED;
            float angular_velocity = glm::radians(25.0f); // rot_speed
            float yaw_velocity = glm::radians(5.0f); // yaw_speed, slower!

            float target_yaw = 0.0f;
            float target_pitch = 0.0f;
            float target_roll = 0.0f;

            float current_yaw = 0.0f;
            float current_pitch = 0.0f;
            float current_roll = 0.0f;

            float turn_amount = 0.0f;
            float turn_time = 0.0f;

        public:
            void set_yaw_dir (float dir) {target_yaw = dir;}
            void set_pitch_dir (float dir) {target_pitch = dir;}
            void set_roll_dir (float dir) {target_roll = dir;}

            void throttle_up () {if (throttle_level < 5) {throttle_level++;}}
            void throttle_down () {if (throttle_level > 0) {throttle_level--;}}

            void update (float dt) {

                // auto-align
                float auto_pitch = 0.0f;
                float auto_roll = 0.0f;
                float auto_yaw = 0.0f;

                float target_velocity = (throttle_level / 5.0f) * MAX_SPEED; // fixed max speed for each specific throttle_level
                float speed_difference = target_velocity - current_velocity; // dynamic engine/brake
                
                float acceleration_rate = speed_difference * 0.25f; // start to accelerate (decelerate) to reach target speed
                current_velocity += acceleration_rate * dt; // v = at 

                // this gives somewhat of a feel of the aircrafts weight
                current_pitch += (target_pitch - current_pitch) * WEIGHT_FEEL * dt;
                current_yaw += (target_yaw - current_yaw) * WEIGHT_FEEL * dt;
                current_roll += (target_roll - current_roll) * WEIGHT_FEEL * dt;

                glm::vec3 fw_dir = orientation * glm::vec3(0.0f, 0.0f, -1.0f);
                glm::vec3 right_dir = orientation * glm::vec3(1.0f, 0.0f, 0.0f);

                // if i'm not holding the stick (WS + EQ) 
                if (target_pitch == 0.0f) {auto_pitch = fw_dir.y * CENTERING_SPEED;}
                if (target_roll == 0.0f) {auto_roll = right_dir.y * CENTERING_SPEED;}
                if (target_yaw != 0.0f) { 
                    turn_amount += current_yaw * yaw_velocity * dt; // how far i'm turning
                    turn_time += dt; // for how long i'm turning

                    // the more i turn, the less i feel the effect
                    if (turn_time > 0.5f) {turn_amount -= turn_amount * 15.0f * dt;} 
                } else {
                    turn_time = 0.0f; // reset
                    auto_yaw = (turn_amount * 30.0f * CENTERING_SPEED) / yaw_velocity; 
                    turn_amount -= auto_yaw * yaw_velocity * dt;
                }

                glm::vec3 euler_turn( // turning using euler angles sent by WASD + EQ
                    (current_pitch - auto_pitch) * angular_velocity * dt,
                    (current_yaw - auto_yaw) * yaw_velocity * dt,    
                    (current_roll - auto_roll) * angular_velocity * dt
                );

                glm::quat local_rotation = glm::quat(euler_turn);
                orientation = orientation * local_rotation; // applies rotation
                orientation = glm::normalize(orientation);

                float lift_power = GRAVITY_FORCE * (current_velocity / MAX_SPEED);

                // three base vectors
                fw_dir = orientation * glm::vec3(0.0f, 0.0f, -1.0f);
                glm::vec3 up_dir = orientation * glm::vec3(0.0f, 1.0f, 0.0f);
                glm::vec3 gravity_dir = glm::vec3(0.0f, -GRAVITY_FORCE, 0.0f);

                // dynamic vectors
                glm::vec3 lift_vec = up_dir * lift_power;
                glm::vec3 move_dir = fw_dir * current_velocity;
                
                glm::vec3 final_velocity = move_dir + gravity_dir + lift_vec;
                position += final_velocity * dt; // its moving!
            }
    };
}

#endif