#include "octree.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr int MAX_DEPTH = 64;
constexpr double MIN_HALF_WIDTH = 1e-9;

} // namespace

Octree::Octree(double softening_length, double opening_angle, int max_particles_per_leaf)
	: softening_squared_(softening_length * softening_length), opening_angle_(opening_angle),
	  inverse_opening_angle_(1.0 / opening_angle),
	  max_particles_per_leaf_(std::max(1, max_particles_per_leaf)) {}

int Octree::child_index_containing(const Vec3 &position, const Vec3 &center) {
	int index = 0;
	if (position.x > center.x) {
		index |= 1;
	}
	if (position.y > center.y) {
		index |= 2;
	}
	if (position.z > center.z) {
		index |= 4;
	}
	return index;
}

void Octree::build(const std::vector<Particle> &particles) {
	particles_ = &particles;
	const int count = static_cast<int>(particles.size());

	nodes_.clear();
	particle_order_.resize(count);
	partition_scratch_.resize(count);
	for (int i = 0; i < count; ++i) {
		particle_order_[i] = i;
	}

	Vec3 minimum = particles[0].position;
	Vec3 maximum = particles[0].position;
	for (const Particle &particle : particles) {
		minimum.x = std::min(minimum.x, particle.position.x);
		minimum.y = std::min(minimum.y, particle.position.y);
		minimum.z = std::min(minimum.z, particle.position.z);
		maximum.x = std::max(maximum.x, particle.position.x);
		maximum.y = std::max(maximum.y, particle.position.y);
		maximum.z = std::max(maximum.z, particle.position.z);
	}
	const Vec3 center = (minimum + maximum) * 0.5;
	double half_width =
		0.5 * std::max({maximum.x - minimum.x, maximum.y - minimum.y, maximum.z - minimum.z});
	if (half_width <= 0.0) {
		half_width = 1.0;
	}
	half_width *= 1.0 + 1e-9; // keep particles on the boundary inside the root cell

	nodes_.resize(1);
	build_subtree(0, 0, count, center, half_width, 0);
	compute_multipoles(0);
	interaction_count_ = 0;
}

void Octree::build_subtree(int node_index, int first_particle_index, int particle_end_index,
						   const Vec3 &center, double half_width, int depth) {
	OctreeNode &node = nodes_[node_index];
	node.center = center;
	node.half_width = half_width;
	node.first_particle_index = first_particle_index;
	node.particle_end_index = particle_end_index;
	node.first_child_index = -1;

	const int particle_count = particle_end_index - first_particle_index;
	if (particle_count <= max_particles_per_leaf_ || depth >= MAX_DEPTH ||
		half_width < MIN_HALF_WIDTH) {
		return;
	}

	int particles_per_child[8] = {};
	for (int i = first_particle_index; i < particle_end_index; ++i) {
		const Particle &particle = (*particles_)[particle_order_[i]];
		++particles_per_child[child_index_containing(particle.position, center)];
	}

	int child_begin[8];
	int cursor = first_particle_index;
	for (int child = 0; child < 8; ++child) {
		child_begin[child] = cursor;
		cursor += particles_per_child[child];
	}
	int child_end[8];
	std::copy(child_begin, child_begin + 8, child_end);
	for (int i = first_particle_index; i < particle_end_index; ++i) {
		const int particle_index = particle_order_[i];
		const Particle &particle = (*particles_)[particle_index];
		const int child = child_index_containing(particle.position, center);
		partition_scratch_[child_end[child]++] = particle_index;
	}
	std::copy(partition_scratch_.begin() + first_particle_index,
			  partition_scratch_.begin() + particle_end_index,
			  particle_order_.begin() + first_particle_index);

	const int first_child_index = static_cast<int>(nodes_.size());
	nodes_.resize(first_child_index + 8);
	nodes_[node_index].first_child_index = first_child_index;

	const double child_half_width = half_width * 0.5;
	for (int child = 0; child < 8; ++child) {
		const Vec3 child_center = {center.x + ((child & 1) ? child_half_width : -child_half_width),
								   center.y + ((child & 2) ? child_half_width : -child_half_width),
								   center.z + ((child & 4) ? child_half_width : -child_half_width)};
		build_subtree(first_child_index + child, child_begin[child], child_end[child], child_center,
					  child_half_width, depth + 1);
	}
}

void Octree::compute_multipoles(int node_index) {
	OctreeNode &node = nodes_[node_index];
	double mass = 0.0;
	Vec3 mass_weighted_position;

	if (node.is_leaf()) {
		for (int i = node.first_particle_index; i < node.particle_end_index; ++i) {
			const Particle &particle = (*particles_)[particle_order_[i]];
			mass += particle.mass;
			mass_weighted_position += particle.position * particle.mass;
		}
	} else {
		for (int child = 0; child < 8; ++child) {
			const int child_index = node.first_child_index + child;
			compute_multipoles(child_index);
			mass += nodes_[child_index].total_mass;
			mass_weighted_position +=
				nodes_[child_index].center_of_mass * nodes_[child_index].total_mass;
		}
	}

	node.total_mass = mass;
	node.center_of_mass = mass > 0.0 ? mass_weighted_position * (1.0 / mass) : node.center;

#ifdef QUADRUPOLE
	for (int row = 0; row < 3; ++row) {
		for (int column = 0; column < 3; ++column) {
			node.quadrupole[row][column] = 0.0;
		}
	}
	if (node.is_leaf()) {
		for (int i = node.first_particle_index; i < node.particle_end_index; ++i) {
			const Particle &particle = (*particles_)[particle_order_[i]];
			const Vec3 offset = particle.position - node.center_of_mass;
			const double distance_squared = offset.length_squared();
			const double coordinates[3] = {offset.x, offset.y, offset.z};
			for (int row = 0; row < 3; ++row) {
				for (int column = 0; column < 3; ++column) {
					node.quadrupole[row][column] +=
						particle.mass * (3.0 * coordinates[row] * coordinates[column] -
										 (row == column ? distance_squared : 0.0));
				}
			}
		}
	} else {
		// Parallel-axis theorem: move every child quadrupole to this centre of mass.
		for (int child = 0; child < 8; ++child) {
			const OctreeNode &child_node = nodes_[node.first_child_index + child];
			const Vec3 displacement = child_node.center_of_mass - node.center_of_mass;
			const double distance_squared = displacement.length_squared();
			const double coordinates[3] = {displacement.x, displacement.y, displacement.z};
			for (int row = 0; row < 3; ++row) {
				for (int column = 0; column < 3; ++column) {
					node.quadrupole[row][column] +=
						child_node.quadrupole[row][column] +
						child_node.total_mass * (3.0 * coordinates[row] * coordinates[column] -
												 (row == column ? distance_squared : 0.0));
				}
			}
		}
	}
#endif
}

void Octree::add_approximate_force(Vec3 &acceleration, const OctreeNode &node,
								   const Vec3 &target_position) const {
	const Vec3 offset = target_position - node.center_of_mass;
	// The softening also enters the multipole denominator, so the force does
	// not jump when a cell crosses the acceptance boundary.
	const double distance_squared = offset.length_squared() + softening_squared_;
	const double inverse_distance = 1.0 / std::sqrt(distance_squared);
	const double inverse_distance_squared = inverse_distance * inverse_distance;
	const double inverse_distance_cubed = inverse_distance_squared * inverse_distance;

	acceleration += offset * (-node.total_mass * inverse_distance_cubed);

#ifdef QUADRUPOLE
	const double inverse_distance_fifth = inverse_distance_cubed * inverse_distance_squared;
	const double inverse_distance_seventh = inverse_distance_fifth * inverse_distance_squared;
	const Vec3 quadrupole_times_offset = {
		node.quadrupole[0][0] * offset.x + node.quadrupole[0][1] * offset.y +
			node.quadrupole[0][2] * offset.z,
		node.quadrupole[1][0] * offset.x + node.quadrupole[1][1] * offset.y +
			node.quadrupole[1][2] * offset.z,
		node.quadrupole[2][0] * offset.x + node.quadrupole[2][1] * offset.y +
			node.quadrupole[2][2] * offset.z};
	const double offset_quadrupole_offset = offset.dot(quadrupole_times_offset);
	acceleration += quadrupole_times_offset * inverse_distance_fifth;
	acceleration += offset * (-2.5 * offset_quadrupole_offset * inverse_distance_seventh);
#endif
	++interaction_count_;
}

void Octree::add_forces_from_leaf_particles(int first_particle_index, int particle_end_index,
											int target_particle_index, const Vec3 &target_position,
											Vec3 &acceleration) const {
	double acceleration_x = 0.0;
	double acceleration_y = 0.0;
	double acceleration_z = 0.0;
	for (int i = first_particle_index; i < particle_end_index; ++i) {
		const int other_index = particle_order_[i];
		if (other_index == target_particle_index) {
			continue;
		}
		++interaction_count_;
		const Particle &other = (*particles_)[other_index];
		const double dx = other.position.x - target_position.x;
		const double dy = other.position.y - target_position.y;
		const double dz = other.position.z - target_position.z;
		const double distance_squared = dx * dx + dy * dy + dz * dz + softening_squared_;
		const double inverse_distance = 1.0 / std::sqrt(distance_squared);
		const double factor = other.mass * inverse_distance * inverse_distance * inverse_distance;
		acceleration_x += dx * factor;
		acceleration_y += dy * factor;
		acceleration_z += dz * factor;
	}

	acceleration.x += acceleration_x;
	acceleration.y += acceleration_y;
	acceleration.z += acceleration_z;
}

void Octree::add_forces_from_subtree(int node_index, int target_index, const Vec3 &target_position,
									 Vec3 &acceleration, bool contains_target) const {
	const OctreeNode &node = nodes_[node_index];
	if (node.total_mass == 0.0) {
		return;
	}

	// Barnes-Hut criterion: cell_width / distance_to_centre_of_mass < angle,
	// compared squared to avoid a square root on every visit.
	const double reach = 2.0 * node.half_width * inverse_opening_angle_;
	if (!contains_target &&
		(node.center_of_mass - target_position).length_squared() > reach * reach) {
		add_approximate_force(acceleration, node, target_position);
		return;
	}

	if (node.is_leaf()) {
		add_forces_from_leaf_particles(node.first_particle_index, node.particle_end_index,
									   target_index, target_position, acceleration);
		return;
	}

	// A cell that contains the target must always be opened, otherwise the
	// target would attract itself through its own cell.
	const int target_child = child_index_containing(target_position, node.center);
	for (int child = 0; child < 8; ++child) {
		add_forces_from_subtree(node.first_child_index + child, target_index, target_position,
								acceleration, contains_target && child == target_child);
	}
}

long long Octree::compute_accelerations(std::vector<Particle> &particles) const {
	interaction_count_ = 0;
	for (int particle_index = 0; particle_index < static_cast<int>(particles.size());
		 ++particle_index) {
		Vec3 acceleration;
		add_forces_from_subtree(0, particle_index, particles[particle_index].position, acceleration,
								true);
		particles[particle_index].acceleration = acceleration;
	}
	return interaction_count_;
}

void compute_tree_accelerations(std::vector<Particle> &particles, const Params &params) {
	Octree tree(params.softening_length, params.opening_angle, params.max_particles_per_leaf);
	tree.build(particles);
	tree.compute_accelerations(particles);
}
