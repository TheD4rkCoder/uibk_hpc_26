#include "nbody.h"

#include <algorithm>
#include <random>

namespace {

constexpr double CUBE_HALF_WIDTH = 30.0;
constexpr double CUBE_MAX_SPEED = 3.0;
constexpr double CUBE_MIN_MASS = 0.1;
constexpr double CUBE_MAX_MASS = 0.2;
constexpr double TWO_PI = 6.283185307179586476925286766559;

Vec3 random_unit_vector(std::mt19937_64 &random) {
	std::uniform_real_distribution<double> uniform(0.0, 1.0);
	const double z = 2.0 * uniform(random) - 1.0;
	const double azimuth = TWO_PI * uniform(random);
	const double radius = std::sqrt(std::max(0.0, 1.0 - z * z));
	return {radius * std::cos(azimuth), radius * std::sin(azimuth), z};
}

std::vector<Particle> make_cube(const Params &params) {
	std::mt19937_64 random(params.random_seed);
	std::uniform_real_distribution<double> position_distribution(-CUBE_HALF_WIDTH, CUBE_HALF_WIDTH);
	std::uniform_real_distribution<double> velocity_distribution(-CUBE_MAX_SPEED, CUBE_MAX_SPEED);
	std::uniform_real_distribution<double> mass_distribution(CUBE_MIN_MASS, CUBE_MAX_MASS);

	std::vector<Particle> particles(params.particle_count);
	for (Particle &particle : particles) {
		particle.mass = mass_distribution(random);
		particle.position = {position_distribution(random), position_distribution(random),
							 position_distribution(random)};
		particle.velocity = {velocity_distribution(random), velocity_distribution(random),
							 velocity_distribution(random)};
	}
	return particles;
}

std::vector<Particle> make_plummer(const Params &params) {
	std::mt19937_64 random(params.random_seed);
	std::uniform_real_distribution<double> uniform(0.0, 1.0);

	const double total_mass = params.plummer_total_mass;
	const double scale_radius = params.plummer_scale_radius;
	const double particle_mass = total_mass / params.particle_count;

	std::vector<Particle> particles(params.particle_count);
	for (Particle &particle : particles) {
		// Inverse cumulative distribution of the Plummer density profile.
		const double radius_sample = uniform(random);
		const double distance = scale_radius / std::sqrt(std::pow(radius_sample, -2.0 / 3.0) - 1.0);

		particle.mass = particle_mass;
		particle.position = random_unit_vector(random) * distance;

		// Aarseth, Henon & Wielen (1974): the speed fraction q = v / v_escape
		// follows g(q) = q^2 (1 - q^2)^(7/2), whose maximum is about 0.092.
		double speed_fraction;
		do {
			speed_fraction = uniform(random);
		} while (0.1 * uniform(random) > speed_fraction * speed_fraction *
											 std::pow(1.0 - speed_fraction * speed_fraction, 3.5));

		const double escape_speed = std::sqrt(
			2.0 * total_mass / std::sqrt(distance * distance + scale_radius * scale_radius));
		particle.velocity = random_unit_vector(random) * (speed_fraction * escape_speed);
	}
	return particles;
}

// A four-cluster scene for the movie: two clusters collide head-on along x,
// while two orbit the collision point in the vertical y-z plane.
std::vector<Particle> make_clusters(const Params &params) {
	struct Cluster {
		Vec3 offset;
		Vec3 bulk_velocity;
	};

	constexpr double COLLISION_RADIUS = 90.0;
	constexpr double COLLISION_SPEED = 0.3;
	constexpr double ORBIT_RADIUS = 90.0;
	constexpr double ORBIT_SPEED = 0.72;
	constexpr double ORBIT_START_ANGLE = 0.125 * TWO_PI;
	const double orbit_y = ORBIT_RADIUS * std::cos(ORBIT_START_ANGLE);
	const double orbit_z = ORBIT_RADIUS * std::sin(ORBIT_START_ANGLE);
	const double orbit_speed_y = -ORBIT_SPEED * std::sin(ORBIT_START_ANGLE);
	const double orbit_speed_z = ORBIT_SPEED * std::cos(ORBIT_START_ANGLE);
	const Cluster clusters[] = {
		{{-COLLISION_RADIUS, 0.0, 0.0}, {COLLISION_SPEED, 0.0, 0.0}},
		{{COLLISION_RADIUS, 0.0, 0.0}, {-COLLISION_SPEED, 0.0, 0.0}},
		{{0.0, orbit_y, orbit_z}, {0.0, orbit_speed_y, orbit_speed_z}},
		{{0.0, -orbit_y, -orbit_z}, {0.0, -orbit_speed_y, -orbit_speed_z}},
	};
	constexpr int CLUSTER_COUNT = static_cast<int>(sizeof(clusters) / sizeof(clusters[0]));

	std::vector<Particle> particles;
	particles.reserve(params.particle_count);
	for (int cluster_index = 0; cluster_index < CLUSTER_COUNT; ++cluster_index) {
		Params cluster_params = params;
		cluster_params.particle_count =
			params.particle_count / CLUSTER_COUNT +
			(cluster_index < params.particle_count % CLUSTER_COUNT ? 1 : 0);
		if (cluster_params.particle_count == 0) {
			continue;
		}
		cluster_params.random_seed =
			params.random_seed + static_cast<unsigned int>(1000 * cluster_index);
		cluster_params.plummer_total_mass = params.plummer_total_mass / CLUSTER_COUNT;
		cluster_params.plummer_scale_radius = 0.5 * params.plummer_scale_radius;

		std::vector<Particle> cluster_particles = make_plummer(cluster_params);
		for (Particle &particle : cluster_particles) {
			particle.position += clusters[cluster_index].offset;
			particle.velocity += clusters[cluster_index].bulk_velocity;
		}
		particles.insert(particles.end(), cluster_particles.begin(), cluster_particles.end());
	}
	return particles;
}

} // namespace

void leapfrog_step(std::vector<Particle> &particles, const Params &params) {
	const double half_time_step = 0.5 * params.time_step;
	for (Particle &particle : particles) {
		particle.position += particle.velocity * half_time_step;
	}

	if (params.solver == Solver::DirectSum) {
		compute_exact_accelerations(particles, params.softening_length);
	} else {
		compute_tree_accelerations(particles, params);
	}

	for (Particle &particle : particles) {
		particle.velocity += particle.acceleration * params.time_step;
		particle.position += particle.velocity * half_time_step;
	}
}

std::vector<Particle> make_particles(const Params &params) {
	if (params.initial_condition == InitialCondition::Plummer) {
		return make_plummer(params);
	}
	if (params.initial_condition == InitialCondition::Clusters) {
		return make_clusters(params);
	}
	return make_cube(params);
}

// Exact O(n^2) summation. Newton's third law halves the work: every pair is
// evaluated once and applied with opposite signs.
void compute_exact_accelerations(std::vector<Particle> &particles, double softening_length) {
	const double softening_squared = softening_length * softening_length;
	const std::size_t count = particles.size();

	for (Particle &particle : particles) {
		particle.acceleration = {};
	}

	for (std::size_t i = 0; i < count; ++i) {
		const Vec3 position_i = particles[i].position;
		for (std::size_t j = i + 1; j < count; ++j) {
			const Vec3 displacement = particles[j].position - position_i;
			const double distance_squared = displacement.length_squared() + softening_squared;
			const double inverse_distance = 1.0 / std::sqrt(distance_squared);
			const Vec3 displacement_over_distance_cubed =
				displacement * (inverse_distance * inverse_distance * inverse_distance);

			particles[i].acceleration += displacement_over_distance_cubed * particles[j].mass;
			particles[j].acceleration += displacement_over_distance_cubed * (-particles[i].mass);
		}
	}
}

void write_particle_frame(std::FILE *out, const std::vector<Particle> &particles) {
	for (const Particle &particle : particles) {
		std::fprintf(out, "%.6f %.6f %.6f\n", particle.position.x, particle.position.y,
					 particle.position.z);
	}
	std::fprintf(out, "\n\n");
}
