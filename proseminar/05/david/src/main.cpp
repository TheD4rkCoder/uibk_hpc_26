#include "nbody.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

double seconds_since(Clock::time_point start) {
	return std::chrono::duration<double>(Clock::now() - start).count();
}

const char *solver_name(Solver solver) {
	return solver == Solver::DirectSum ? "direct" : "bh";
}

const char *initial_condition_name(InitialCondition initial_condition) {
	switch (initial_condition) {
	case InitialCondition::Cube:
		return "cube";
	case InitialCondition::Plummer:
		return "plummer";
	case InitialCondition::Clusters:
		return "clusters";
	}
	return "cube";
}

void print_usage(const char *program) {
	std::fprintf(stderr,
				 "usage: %s [options]\n"
				 "  --n N              particles (default 5000)\n"
				 "  --steps T          timesteps (default 100)\n"
				 "  --dt DT            timestep (default 1)\n"
				 "  --solver direct|bh (default direct)\n"
				 "  --theta THETA      Barnes-Hut opening angle (default 100 = no criterion)\n"
				 "  --leaf K           max particles per leaf (default 16)\n"
				 "  --ic cube|plummer|clusters  initial conditions (default cube)\n"
				 "  --seed S           random seed (default 42)\n"
				 "  --softening EPS    Plummer softening (default 0.2)\n"
				 "  --output FILE      write frames to FILE\n"
				 "  --emit             stream frames to stdout (live viewer)\n"
				 "  --energy-every K   print exact total energy every K steps\n"
				 "  --quiet            suppress the summary\n",
				 program);
}

// O(n^2); only a diagnostic, never part of the timing.
double total_energy(const std::vector<Particle> &particles, double softening_length) {
	const double softening_squared = softening_length * softening_length;
	double kinetic_energy = 0.0;
	for (const Particle &particle : particles) {
		kinetic_energy += 0.5 * particle.mass * particle.velocity.length_squared();
	}
	double potential_energy = 0.0;
	for (std::size_t i = 0; i < particles.size(); ++i) {
		for (std::size_t j = i + 1; j < particles.size(); ++j) {
			const double distance_squared =
				(particles[j].position - particles[i].position).length_squared() +
				softening_squared;
			potential_energy -= particles[i].mass * particles[j].mass / std::sqrt(distance_squared);
		}
	}
	return kinetic_energy + potential_energy;
}

void write_frames(std::FILE *output, bool stream_frames, const std::vector<Particle> &particles,
				  double &io_seconds) {
	const Clock::time_point start = Clock::now();
	if (output != nullptr) {
		write_particle_frame(output, particles);
	}
	if (stream_frames) {
		write_particle_frame(stdout, particles);
		std::fflush(stdout);
	}
	io_seconds += seconds_since(start);
}

bool take_option_value(int &index, int argc, char **argv, const char *&value) {
	if (index + 1 >= argc) {
		return false;
	}
	value = argv[++index];
	return true;
}

bool parse_solver(const char *text, Solver &solver) {
	if (std::strcmp(text, "direct") == 0) {
		solver = Solver::DirectSum;
		return true;
	}
	if (std::strcmp(text, "bh") == 0) {
		solver = Solver::BarnesHut;
		return true;
	}
	return false;
}

bool parse_initial_condition(const char *text, InitialCondition &initial_condition) {
	if (std::strcmp(text, "cube") == 0) {
		initial_condition = InitialCondition::Cube;
		return true;
	}
	if (std::strcmp(text, "plummer") == 0) {
		initial_condition = InitialCondition::Plummer;
		return true;
	}
	if (std::strcmp(text, "clusters") == 0) {
		initial_condition = InitialCondition::Clusters;
		return true;
	}
	return false;
}

bool parse_args(int argc, char **argv, Params &params) {
	for (int i = 1; i < argc; ++i) {
		const char *argument = argv[i];
		const char *value = nullptr;

		if (std::strcmp(argument, "--help") == 0 || std::strcmp(argument, "-h") == 0) {
			print_usage(argv[0]);
			std::exit(0);
		} else if (std::strcmp(argument, "--n") == 0 && take_option_value(i, argc, argv, value)) {
			params.particle_count = std::atoi(value);
		} else if (std::strcmp(argument, "--steps") == 0 &&
				   take_option_value(i, argc, argv, value)) {
			params.step_count = std::atoi(value);
		} else if (std::strcmp(argument, "--dt") == 0 && take_option_value(i, argc, argv, value)) {
			params.time_step = std::atof(value);
		} else if (std::strcmp(argument, "--solver") == 0 &&
				   take_option_value(i, argc, argv, value)) {
			if (!parse_solver(value, params.solver)) {
				std::fprintf(stderr, "unknown solver: %s\n", value);
				return false;
			}
		} else if (std::strcmp(argument, "--theta") == 0 &&
				   take_option_value(i, argc, argv, value)) {
			params.opening_angle = std::atof(value);
		} else if (std::strcmp(argument, "--leaf") == 0 &&
				   take_option_value(i, argc, argv, value)) {
			params.max_particles_per_leaf = std::atoi(value);
		} else if (std::strcmp(argument, "--ic") == 0 && take_option_value(i, argc, argv, value)) {
			if (!parse_initial_condition(value, params.initial_condition)) {
				std::fprintf(stderr, "unknown initial condition: %s\n", value);
				return false;
			}
		} else if (std::strcmp(argument, "--seed") == 0 &&
				   take_option_value(i, argc, argv, value)) {
			params.random_seed = static_cast<unsigned int>(std::strtoul(value, nullptr, 10));
		} else if (std::strcmp(argument, "--softening") == 0 &&
				   take_option_value(i, argc, argv, value)) {
			params.softening_length = std::atof(value);
		} else if (std::strcmp(argument, "--output") == 0 &&
				   take_option_value(i, argc, argv, value)) {
			params.output_filename = value;
		} else if (std::strcmp(argument, "--emit") == 0) {
			params.stream_frames = true;
		} else if (std::strcmp(argument, "--energy-every") == 0 &&
				   take_option_value(i, argc, argv, value)) {
			params.energy_report_interval = std::atoi(value);
		} else if (std::strcmp(argument, "--quiet") == 0) {
			params.quiet = true;
		} else {
			std::fprintf(stderr, "unknown or incomplete option: %s\n", argument);
			return false;
		}
	}

	if (params.particle_count < 1 || params.step_count < 0 || params.time_step <= 0.0 ||
		params.opening_angle <= 0.0 || params.softening_length < 0.0 ||
		params.max_particles_per_leaf < 1) {
		std::fprintf(stderr, "invalid parameter value\n");
		return false;
	}
	return true;
}

} // namespace

int main(int argc, char **argv) {
	Params params;
	if (!parse_args(argc, argv, params)) {
		print_usage(argv[0]);
		return 1;
	}

	const Clock::time_point start = Clock::now();
	std::vector<Particle> particles = make_particles(params);

	std::FILE *output = nullptr;
	if (params.output_filename != nullptr) {
		output = std::fopen(params.output_filename, "w");
		if (output == nullptr) {
			std::fprintf(stderr, "cannot open %s\n", params.output_filename);
			return 1;
		}
	}

	double io_seconds = 0.0;
	write_frames(output, params.stream_frames, particles, io_seconds);
	if (params.energy_report_interval > 0) {
		std::fprintf(stderr, "step 0 energy %.9e\n",
					 total_energy(particles, params.softening_length));
	}

	const Clock::time_point simulation_start = Clock::now();

	for (int step = 0; step < params.step_count; ++step) {
		leapfrog_step(particles, params);
		if (params.energy_report_interval > 0 && (step + 1) % params.energy_report_interval == 0) {
			std::fprintf(stderr, "step %d energy %.9e\n", step + 1,
						 total_energy(particles, params.softening_length));
		}
		write_frames(output, params.stream_frames, particles, io_seconds);
	}
	const double simulation_seconds = seconds_since(simulation_start);
	const double total_seconds = seconds_since(start);

	if (output != nullptr) {
		std::fclose(output);
	}

	// --emit puts frames on stdout, so keep the summary on stderr there.
	std::FILE *report = params.stream_frames ? stderr : stdout;
	if (!params.quiet) {
		std::fprintf(report, "n=%d steps=%d solver=%s ic=%s theta=%.3f softening=%.3f\n",
					 params.particle_count, params.step_count, solver_name(params.solver),
					 initial_condition_name(params.initial_condition), params.opening_angle,
					 params.softening_length);
		std::fprintf(report, "sim=%.6fs io=%.6fs total=%.6fs\n", simulation_seconds, io_seconds,
					 total_seconds);
	}
#ifdef QUADRUPOLE
	const int quadrupole_enabled = 1;
#else
	const int quadrupole_enabled = 0;
#endif
	std::fprintf(report,
				 "BENCH solver=%s ic=%s n=%d steps=%d theta=%.4f quad=%d sim_s=%.6f io_s=%.6f\n",
				 solver_name(params.solver), initial_condition_name(params.initial_condition),
				 params.particle_count, params.step_count, params.opening_angle, quadrupole_enabled,
				 simulation_seconds, io_seconds);
	return 0;
}
