#pragma once

#include <cmath>
#include <cstdio>
#include <vector>

struct Vec3 {
	double x = 0.0;
	double y = 0.0;
	double z = 0.0;

	Vec3() = default;
	Vec3(double x, double y, double z) : x(x), y(y), z(z) {}

	Vec3 operator+(const Vec3 &other) const { return {x + other.x, y + other.y, z + other.z}; }
	Vec3 operator-(const Vec3 &other) const { return {x - other.x, y - other.y, z - other.z}; }
	Vec3 operator*(double scale) const { return {x * scale, y * scale, z * scale}; }

	Vec3 &operator+=(const Vec3 &other) {
		x += other.x;
		y += other.y;
		z += other.z;
		return *this;
	}

	double dot(const Vec3 &other) const { return x * other.x + y * other.y + z * other.z; }
	double length_squared() const { return dot(*this); }
	double length() const { return std::sqrt(length_squared()); }
};

struct Particle {
	double mass = 0.0;
	Vec3 position;
	Vec3 velocity;
	Vec3 acceleration;
};

enum class Solver { DirectSum, BarnesHut };
enum class InitialCondition { Cube, Plummer, Clusters };

struct Params {
	int particle_count = 5000;
	int step_count = 100;
	double time_step = 1.0;

	Solver solver = Solver::DirectSum;
	// Cells that subtend less than this angle are approximated; 100 disables
	// the criterion, so every cell is used as a multipole.
	double opening_angle = 100.0;
	int max_particles_per_leaf = 16;

	// Plummer softening replaces r^2 by r^2 + softening_length^2. Zero
	// switches it off.
	double softening_length = 0.2;

	InitialCondition initial_condition = InitialCondition::Cube;
	unsigned int random_seed = 42;

	// Plummer sphere: total mass and scale radius. The defaults span roughly
	// +-40 for 90 % of the mass, with typical speeds of 3.
	double plummer_total_mass = 100.0;
	double plummer_scale_radius = 10.0;

	bool stream_frames = false;
	const char *output_filename = nullptr;
	int energy_report_interval = 0;
	bool quiet = false;
};

std::vector<Particle> make_particles(const Params &params);

// Overwrite Particle::acceleration with the gravitational acceleration (G = 1).
void compute_exact_accelerations(std::vector<Particle> &particles, double softening_length);
void compute_tree_accelerations(std::vector<Particle> &particles, const Params &params);

// One drift-kick-drift leapfrog step: half drift, one force evaluation, kick,
// half drift. Positions and velocities are synchronised after every step.
void leapfrog_step(std::vector<Particle> &particles, const Params &params);

// One frame for gnuplot and the viewers: "x y z" per particle, then two blank
// lines.
void write_particle_frame(std::FILE *out, const std::vector<Particle> &particles);
