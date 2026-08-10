#ifndef PHYSICS_HH
#define PHYSICS_HH

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

            unsigned int throttle_level = 3; // [0, 5]
            float current_velocity = (throttle_level / 5.0f) * MAX_SPEED;
            float angular_velocity = glm::radians(30.0f); // rot_speed
            float acceleration_rate = 0.0f;

            /** old Camera movement **/
            float yaw_dir = 0.0;
            float pitch_dir = 0.0;
            float roll_dir = 0.0;
        public:
            void set_yaw_dir (float dir) {yaw_dir = dir;}
            void set_pitch_dir (float dir) {pitch_dir = dir;}
            void set_roll_dir (float dir) {roll_dir = dir;}

            void throttle_up () {if (throttle_level < 5) {throttle_level++;}}
            void throttle_down () {if (throttle_level > 0) {throttle_level--;}}

            void update (float dt) {

                float target_velocity = (throttle_level / 5.0f) * MAX_SPEED; // fixed max speed for each specific throttle_level
                float speed_difference = target_velocity - current_velocity; // dynamic engine/brake
                

                acceleration_rate = speed_difference * 0.25f; // start to accelerate (decelerate) to reach target speed
                current_velocity += acceleration_rate * dt; // v = at 

                glm::vec3 euler_turn( // turning using euler angles sent by WASD + EQ
                    pitch_dir * angular_velocity * dt,
                    yaw_dir * angular_velocity * dt,
                    roll_dir * angular_velocity * dt
                );

                glm::quat local_rotation = glm::quat(euler_turn);
                orientation = orientation * local_rotation; // applies rotation
                orientation = glm::normalize(orientation);

                float lift_power = GRAVITY_FORCE * (current_velocity / MAX_SPEED);

                // three base vectors
                glm::vec3 fw_dir = orientation * glm::vec3(0.0f, 0.0f, -1.0f);
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