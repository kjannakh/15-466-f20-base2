#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <random>

GLuint level_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > level_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("level1.pnct"));
	level_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > level_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("level1.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = level_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = level_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;

	});
});

void PlayMode::spawn_ball() {
	Scene::Transform* ball;
	if (ball_spawn == 0) {
		ball = balls[0];
		ball_spawn = 1;
	}
	else if (ball_spawn == 1) {
		ball = balls[1];
		ball_spawn = 2;
	}
	else {
		ball = balls[2];
		ball_spawn = 0;
	}
		
	ball->position.x = game_area.x_min + (mt() / float(mt.max())) * game_area.x_size;
	ball->position.y = game_area.y_min + (mt() / float(mt.max())) * game_area.y_size;
	ball->position.z = ball_spawn_height;
}

bool PlayMode::ball_caught(Scene::Transform *ball) {
	glm::mat4x3 frame = player->make_local_to_world();
	glm::vec3 forward = -glm::normalize(frame[0]);
	glm::vec3 right = glm::normalize(frame[1]);
	glm::vec3 up = glm::normalize(frame[2]);
	glm::vec3 offset = box_offset.x * forward + box_offset.y * right + box_offset.z * up;
	box_position = player->position + offset;
	glm::vec3 diff = ball->position - box_position;
	return (abs(diff.x) < box_radius && abs(diff.y) < box_radius && abs(diff.z) < box_height);
}

void PlayMode::update_camera() {
	camera->transform->position.x = cos(yaw) * camera_dist + player->position.x + camera_offset.x;
	camera->transform->position.y = sin(yaw) * camera_dist + player->position.y + camera_offset.y;
	camera->transform->position.z = sin(pitch) * camera_dist + player->position.z + camera_offset.z;

	if (camera->transform->position.z < floor) {
		camera->transform->position.z = floor;
		limit_pitch = true;
	}

	if (camera->transform->position.x < game_area.x_min)
		camera->transform->position.x = game_area.x_min;
	if (camera->transform->position.x > game_area.x_max)
		camera->transform->position.x = game_area.x_max;
	if (camera->transform->position.y < game_area.y_min)
		camera->transform->position.y = game_area.y_min;
	if (camera->transform->position.y > game_area.y_max)
		camera->transform->position.y = game_area.y_max;

	// camera looking code sourced from https://learnopengl.com/Getting-started/Camera and https://gamedev.stackexchange.com/questions/149006/direction-vector-to-quaternion
	glm::mat4 view = glm::lookAt(camera->transform->position, player->position + camera_offset, camera_up);
	camera->transform->rotation = glm::conjugate(glm::quat_cast(view));
}

bool PlayMode::at_delivery() {
	return (abs(player->position.x - delivery_point.x) < 0.5 && abs(player->position.y - delivery_point.y) < 0.5);
}

void PlayMode::reset_game() {
	//reset player position
	player->position = glm::vec3(0.0f, 0.0f, 0.0f);
	player->rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
	pitch = 0.0f;
	yaw = 0.0f;
	player_yaw = 0.0f;

	//reset timers
	battery_drain_timer = 0.0f;
	scrap_timer = 0.0f;
	ball_spawn_timer = 0.0f;
	ball_spawn = 0;
	battery = 50;
	score = 0;

	dead = true;
	update_camera();
	spawn_ball();
}

PlayMode::PlayMode() : scene(*level_scene), meshes(*level_meshes) {
	//get pointers to leg for convenience:
	for (auto &transform : scene.transforms) {
		if (transform.name == "Player") player = &transform;
		if (transform.name == "Sphere1") balls[0] = &transform;
		if (transform.name == "Sphere2") balls[1] = &transform;
		if (transform.name == "Sphere3") balls[2] = &transform;
	}
	if (player == nullptr) throw std::runtime_error("GameObject not found.");
	if (balls[0] == nullptr) throw std::runtime_error("GameObject not found.");
	if (balls[1] == nullptr) throw std::runtime_error("GameObject not found.");
	if (balls[2] == nullptr) throw std::runtime_error("GameObject not found.");

	//get pointer to camera for convenience:
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();
	
	update_camera();
	spawn_ball();
}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_KEYDOWN) {
		if (evt.key.keysym.sym == SDLK_ESCAPE) {
			SDL_SetRelativeMouseMode(SDL_FALSE);
			return true;
		} else if (evt.key.keysym.sym == SDLK_a) {
			left.downs += 1;
			left.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			right.downs += 1;
			right.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.downs += 1;
			up.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.downs += 1;
			down.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_SPACE && !space.pressed) {
			space.downs += 1;
			space.pressed = true;
		}
	} else if (evt.type == SDL_KEYUP) {
		if (evt.key.keysym.sym == SDLK_a) {
			left.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			right.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_SPACE && space.pressed) {
			space.pressed = false;
		}
	} else if (evt.type == SDL_MOUSEBUTTONDOWN) {
		if (evt.button.button == SDL_BUTTON_RIGHT && SDL_GetRelativeMouseMode() == SDL_FALSE) {
			SDL_SetRelativeMouseMode(SDL_TRUE);
			looking = true;
			return true;
		}
	}
	else if (evt.type == SDL_MOUSEBUTTONUP) {
		if (evt.button.button == SDL_BUTTON_RIGHT && SDL_GetRelativeMouseMode() == SDL_TRUE) {
			SDL_SetRelativeMouseMode(SDL_FALSE);
			looking = false;
			return true;
		}
	}
	else if (evt.type == SDL_MOUSEMOTION) {
		if (SDL_GetRelativeMouseMode() == SDL_TRUE) {
			glm::vec2 motion = glm::vec2(
				evt.motion.xrel / float(window_size.y),
				-evt.motion.yrel / float(window_size.y)
			);
			
			yaw += 2.5f * -motion.x * camera->fovy;
			if (yaw < -M_PI) yaw += 2.0f * float(M_PI);
			if (yaw >= M_PI) yaw -= 2.0f * float(M_PI);
		
			float new_pitch = pitch + 1.5f * -motion.y * camera->fovy;
			if (new_pitch > pitch || !limit_pitch) {
				pitch = new_pitch;
				limit_pitch = false;
			}
			if (pitch < -M_PI/2) pitch = float(-M_PI)/2;
			if (pitch > M_PI/2) pitch = float(M_PI)/2;

			update_camera();

			return true;
		}
	}
	else if (evt.type == SDL_MOUSEWHEEL) {
		if (evt.wheel.y > 0 && camera_dist > 2.0f) {
			camera_dist -= 1.0f;
		}
		else if (evt.wheel.y < 0 && camera_dist < 10.0f) {
			camera_dist += 1.0f;
		}
		camera->transform->position.x = cos(yaw) * camera_dist + player->position.x + camera_offset.x;
		camera->transform->position.y = sin(yaw) * camera_dist + player->position.y + camera_offset.y;
		camera->transform->position.z = sin(pitch) * camera_dist + player->position.z + camera_offset.z;
		return true;
	}

	return false;
}

void PlayMode::update(float elapsed) {
	//move player:

	//combine inputs into a move:
	float PlayerSpeed;
	if (battery >= battery_threshold)
		PlayerSpeed = 5.0f;
	else
		PlayerSpeed = min_player_speed + (float(battery) / float(battery_threshold)) * (max_player_speed - min_player_speed);
	glm::vec2 move = glm::vec2(0.0f);
	if (left.pressed && !right.pressed) move.x =-1.0f;
	if (!left.pressed && right.pressed) move.x = 1.0f;
	if (down.pressed && !up.pressed) move.y =-1.0f;
	if (!down.pressed && up.pressed) move.y = 1.0f;

	//make it so that moving diagonally doesn't go faster:
	if (move != glm::vec2(0.0f)) move = glm::normalize(move) * PlayerSpeed * elapsed;

	if (!looking) {
		glm::mat4x3 frame = player->make_local_to_world();
		glm::vec3 forward = -glm::normalize(frame[0]);
		glm::vec3 right = glm::normalize(frame[1]);
		player->position += move.x * right + move.y * forward;
	}
	else {
		glm::mat4x3 frame = camera->transform->make_local_to_parent();
		glm::vec3 right = frame[0];
		right.z = 0.0f;
		right = glm::normalize(right);
		glm::vec3 forward = frame[2];
		forward.z = 0.0f;
		forward = glm::normalize(forward);
		player->position += move.x * right - move.y * forward;
	}
	if (player->position.x < game_area.x_min)
		player->position.x = game_area.x_min;
	if (player->position.x > game_area.x_max)
		player->position.x = game_area.x_max;
	if (player->position.y < game_area.y_min)
		player->position.y = game_area.y_min;
	if (player->position.y > game_area.y_max)
		player->position.y = game_area.y_max;

	update_camera();

	if (move != glm::vec2(0.0f)) {
		if (looking) player_yaw = yaw;
		player->rotation.w = cos(player_yaw / 2.0f);
		player->rotation.z = sin(player_yaw / 2.0f);
	}
	

	//move balls:
	{
		for (auto ball : balls) {
			ball->position.z -= ball_speed * elapsed;
			if (ball_caught(ball)) {
				ball->position.z = -1.0f;
				battery += battery_per_ball;
				if (battery > 100) battery = 100;
			}
		}

		ball_spawn_timer += elapsed;
		if (ball_spawn_timer >= ball_spawn_rate) {
			ball_spawn_timer -= ball_spawn_rate;
			spawn_ball();
		}
	}

	battery_drain_timer += elapsed;
	if (battery_drain_timer > battery_drain_rate) {
		battery_drain_timer -= battery_drain_rate;
		battery--;
		if (move != glm::vec2(0.0f)) {
			battery--;
		}
	}
	if (battery < 0) {
		reset_game();
		return;
	}

	if (space.downs > 0) {
		battery -= battery_per_delivery * space.downs;
		score += battery_per_delivery * space.downs;
		scrap_timer = 0.0f;
	}

	scrap_timer += elapsed;
	if (scrap_timer > scrap_rate) {
		reset_game();
		return;
	}

	if (dead) {
		dead_timer += elapsed;
		if (dead_timer > dead_rate) {
			dead_timer = 0.0f;
			dead = false;
		}
	}

	//reset button press counters:
	left.downs = 0;
	right.downs = 0;
	up.downs = 0;
	down.downs = 0;
	space.downs = 0;
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	//update camera aspect ratio for drawable:
	camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	//set up light type and position for lit_color_texture_program:
	// TODO: consider using the Light(s) in the scene to do this
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	GL_ERRORS();
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	GL_ERRORS();
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
	GL_ERRORS();
	glUseProgram(0);

	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	scene.draw(*camera);

	{ //use DrawLines to overlay some text:
		glDisable(GL_DEPTH_TEST);
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		));

		constexpr float H = 0.15f;
		if (at_delivery()) {
			lines.draw_text("Press spacebar repeatedly to deliver",
				glm::vec3(-0.5f * aspect, -1.0 + 0.1f * H, 0.0),
				glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
				glm::u8vec4(0xff, 0xff, 0xff, 0x00));
		}
		if (scrap_timer / scrap_rate < 0.8f) {
			lines.draw_text("Time until scrapped: " + std::to_string(int(scrap_rate - scrap_timer)),
				glm::vec3(0.25 * aspect, 1.0 - 1.1f * H, 0.0),
				glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
				glm::u8vec4(0xff, 0xff, 0xff, 0x00));
		}
		else {
			lines.draw_text("Time until scrapped: " + std::to_string(int(scrap_rate - scrap_timer)),
				glm::vec3(0.25 * aspect, 1.0 - 1.1f * H, 0.0),
				glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
				glm::u8vec4(0xff, 0x00, 0x00, 0x00));
		}
		lines.draw_text("Score: " + std::to_string(score),
			glm::vec3(-0.9 * aspect, 1.0 - 1.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));

		float bat_width = 0.2f;
		float bat_height = 0.7f;
		glm::mat4x3 bat = glm::mat4x3(
			bat_width, 0.0f, 0.0f,
			0.0f, bat_height, 0.0f,
			0.0f, 0.0f, 1.0f,
			-0.8f * aspect, 0.0f, 0.0f
		);
		glm::mat4x3 lvl = glm::mat4x3(
			bat_width, 0.0f, 0.0f,
			0.0f, 0.01f, 0.0f,
			0.0f, 0.0f, 0.0f,
			-0.8 * aspect, float(battery - 50) * bat_height / 50, 0.0f
		);
		if (battery > battery_threshold) {
			lines.draw_box(bat, glm::u8vec4(0xff, 0xff, 0x00, 0x00));
			lines.draw_box(lvl, glm::u8vec4(0xff, 0xaa, 0x00, 0x00));
		}
		else {
			lines.draw_box(bat, glm::u8vec4(0xff, 0x00, 0x00, 0x00));
			lines.draw_box(lvl, glm::u8vec4(0xff, 0x00, 0x00, 0x00));
		}

		if (dead) {
			lines.draw_text("You've been scrapped!",
				glm::vec3(-0.5 * aspect, 0.0, 0.0),
				glm::vec3(0.25, 0.0f, 0.0f), glm::vec3(0.0f, 0.25, 0.0f),
				glm::u8vec4(0xff, 0x00, 0x00, 0x00));
		}
		
	}
}
