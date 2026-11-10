// Self-contained checks; no test framework on purpose.
//
//   1. Barnes-Hut accelerations converge to direct summation as theta -> 0.
//   2. Leapfrog conserves energy on a circular two-body orbit.
//
// Build both variants to see the effect of the quadrupole:
//   make validate validate_quad

#include "nbody.h"
#include "octree.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace {

int failure_count = 0;

void check(bool condition, const char *description) {
	if (!condition) {
		std::fprintf(stderr, "FAIL: %s\n", description);
		++failure_count;
	}
}

struct Accuracy {
	double relative_l2_error;
	double maximum_relative_error;
	double interactions_per_particle;
};

Accuracy measure_accuracy(const std::vector<Particle> &particles,
						  const std::vector<Particle> &exact, double theta, double softening_length,
						  int leaf_capacity) {
	Octree tree(softening_length, theta, leaf_capacity);
	tree.build(particles);

	std::vector<Particle> approximate = particles;
	const long long interactions = tree.compute_accelerations(approximate);

	double error_squared_sum = 0.0;
	double reference_squared_sum = 0.0;
	double maximum_relative_error = 0.0;
	for (std::size_t i = 0; i < particles.size(); ++i) {
		const Vec3 difference = approximate[i].acceleration - exact[i].acceleration;
		error_squared_sum += difference.length_squared();
		reference_squared_sum += exact[i].acceleration.length_squared();
		maximum_relative_error =
			std::max(maximum_relative_error,
					 difference.length() / std::max(exact[i].acceleration.length(), 1e-12));
	}

	return {std::sqrt(error_squared_sum / reference_squared_sum), maximum_relative_error,
			static_cast<double>(interactions) / particles.size()};
}

void check_force_accuracy() {
	Params params;
	params.particle_count = 2000;
	params.initial_condition = InitialCondition::Cube;
	params.random_seed = 123;
	params.softening_length = 0.0;

	const std::vector<Particle> particles = make_particles(params);
	std::vector<Particle> exact = particles;
	compute_exact_accelerations(exact, params.softening_length);

	std::printf("Barnes-Hut force error, N=%d, softening=0\n", params.particle_count);
	std::printf("  theta   L2 rel. error   max rel. error   interactions/particle\n");
	for (double theta : {0.2, 0.5, 0.8, 1.0, 2.0, 100.0}) {
		const Accuracy accuracy = measure_accuracy(particles, exact, theta, 0.0, 16);
		std::printf("  %.1f     %.3e        %.3e         %.1f\n", theta, accuracy.relative_l2_error,
					accuracy.maximum_relative_error, accuracy.interactions_per_particle);

		if (theta == 0.5) {
#ifdef QUADRUPOLE
			check(accuracy.relative_l2_error < 1e-2, "quadrupole theta=0.5 L2 error < 1%");
#else
			check(accuracy.relative_l2_error < 5e-2, "monopole theta=0.5 L2 error < 5%");
#endif
		}
	}

	// The root mass must equal the total mass; this catches a broken tree build.
	Octree tree(0.0, 0.5, 16);
	tree.build(particles);
	double total_mass = 0.0;
	for (const Particle &particle : particles) {
		total_mass += particle.mass;
	}
	check(std::fabs(tree.root().total_mass - total_mass) < 1e-9 * total_mass,
		  "tree mass equals total mass");

	// The same comparison with the production softening, which also exercises
	// the softened multipole denominators.
	Params softened_params = params;
	softened_params.softening_length = 0.2;
	const std::vector<Particle> softened_particles = make_particles(softened_params);
	std::vector<Particle> softened_exact = softened_particles;
	compute_exact_accelerations(softened_exact, softened_params.softening_length);

	const Accuracy softened = measure_accuracy(softened_particles, softened_exact, 0.5,
											   softened_params.softening_length, 16);
	std::printf("Plummer, eps=0.2, theta=0.5 L2 rel. error %.3e\n", softened.relative_l2_error);
	check(softened.relative_l2_error < 5e-2, "softened theta=0.5 L2 error < 5%");
}

double orbital_energy(const std::vector<Particle> &particles) {
	double kinetic_energy = 0.0;
	for (const Particle &particle : particles) {
		kinetic_energy += 0.5 * particle.mass * particle.velocity.length_squared();
	}
	const double distance = (particles[1].position - particles[0].position).length();
	return kinetic_energy - particles[0].mass * particles[1].mass / distance;
}

void check_two_body_orbit() {
	Params params;
	params.particle_count = 2;
	params.step_count = 10000;
	params.time_step = 0.01;
	params.solver = Solver::DirectSum;
	params.softening_length = 0.0;

	const double separation = 1.0;
	const double angular_speed = std::sqrt(2.0 / (separation * separation * separation));
	const double speed = 0.5 * angular_speed * separation;

	std::vector<Particle> particles(2);
	particles[0].mass = 1.0;
	particles[0].position = {-0.5, 0.0, 0.0};
	particles[0].velocity = {0.0, -speed, 0.0};
	particles[1].mass = 1.0;
	particles[1].position = {0.5, 0.0, 0.0};
	particles[1].velocity = {0.0, speed, 0.0};

	const double initial_energy = orbital_energy(particles);
	double worst_radius_error = 0.0;

	for (int step = 0; step < params.step_count; ++step) {
		leapfrog_step(particles, params);
		const double distance = (particles[1].position - particles[0].position).length();
		worst_radius_error = std::max(worst_radius_error, std::fabs(distance - separation));
	}

	const double final_energy = orbital_energy(particles);
	const double relative_drift = std::fabs((final_energy - initial_energy) / initial_energy);
	std::printf("two-body circular orbit, dt=%.2f, %d steps\n", params.time_step,
				params.step_count);
	std::printf("  energy drift %.3e, worst radius error %.3e\n", relative_drift,
				worst_radius_error);
	check(relative_drift < 1e-6, "leapfrog energy drift < 1e-6");
	// Leapfrog's radius oscillates by O((omega*dt)^2) (~1e-4 here) instead of
	// drifting, so a loose bound still catches a broken integrator.
	check(worst_radius_error < 1e-3, "circular orbit radius stays within 1e-3");
}

} // namespace

int main() {
	check_force_accuracy();
	check_two_body_orbit();
	if (failure_count == 0) {
		std::printf("all checks passed\n");
		return 0;
	}
	std::fprintf(stderr, "%d check(s) failed\n", failure_count);
	return 1;
}
