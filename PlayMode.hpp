#include "Mode.hpp"

#include "Scene.hpp"
#include "Mesh.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>
#include <random>

struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	// helper functions
	void spawn_ball();
	void update_camera();
	bool ball_caught(Scene::Transform *ball);
	bool at_delivery();
	void reset_game();

	//----- game state -----

	//input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right, down, up, space;

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;
	MeshBuffer meshes;

	Scene::Transform *player = nullptr;

	Scene::Transform* balls[3] = { nullptr, nullptr, nullptr };

	glm::vec3 box_position;
	glm::vec3 box_offset = glm::vec3(0.55f, 0.0f, 0.7f);
	float camera_dist = 10.0f;
	bool looking = false;

	glm::vec3 camera_target = glm::vec3(0.0f, 0.0f, 1.0f);
	glm::vec3 camera_up = glm::vec3(0.0f, 0.0f, 1.0f);
	glm::vec3 camera_dir;
	glm::vec3 camera_offset = glm::vec3(0.0f, 0.0f, 1.0f);

	float floor = 0.1f;
	bool limit_pitch = false;

	float pitch = 0.0f, yaw = 0.0f;
	float player_yaw = 0.0f;

	float box_radius = 0.25f;
	float box_height = 0.15f;

	struct Bounds {
		float x_min = -12.0f;
		float x_max = 12.0f;
		float y_min = -12.0f;
		float y_max = 12.0f;
		float x_size = 24.0f;
		float y_size = 24.0f;
	} game_area;

	glm::vec2 delivery_point = glm::vec2(-12.0f, 0.0f);

	float ball_spawn_height = 7.5f;
	float ball_speed = 0.5f;

	float max_player_speed = 7.5f;
	float min_player_speed = 2.5f;
	int8_t battery_threshold = 30;

	uint32_t score = 0;
	int8_t battery = 50;
	int8_t battery_per_ball = 25;
	int8_t battery_per_delivery = 2;

	float battery_drain_timer = 0.0f;
	float battery_drain_rate = 1.0f;
	float scrap_timer = 0.0f;
	float scrap_rate = 25.0f;

	float ball_spawn_timer = 0.0f;
	float ball_spawn_rate = 6.0f;
	int ball_spawn = 0;

	float dead_timer = 0.0f;
	float dead_rate = 2.5f;
	bool dead = false;

	std::mt19937 mt;

	
	//camera:
	Scene::Camera *camera = nullptr;
};
