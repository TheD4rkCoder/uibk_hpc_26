#pragma once

#include "nbody.h"

#include <vector>

struct OctreeNode {
	Vec3 center;
	double half_width = 0.0;
	Vec3 center_of_mass;
	double total_mass = 0.0;
#ifdef QUADRUPOLE
	// Traceless quadrupole about the center of mass:
	// Q_ij = sum_p m_p (3 r_i r_j - delta_ij r^2), r = x_p - center_of_mass.
	double quadrupole[3][3] = {};
#endif
	int first_child_index = -1;
	int first_particle_index = 0;
	int particle_end_index = 0;

	bool is_leaf() const { return first_child_index < 0; }
};

class Octree {
  public:
	Octree(double softening_length, double opening_angle, int max_particles_per_leaf);

	void build(const std::vector<Particle> &particles);
	// Returns the number of particle-cell and particle-particle interactions.
	long long compute_accelerations(std::vector<Particle> &particles) const;
	const OctreeNode &root() const { return nodes_[0]; }

  private:
	void build_subtree(int node_index, int first_particle_index, int particle_end_index,
					   const Vec3 &center, double half_width, int depth);
	void compute_multipoles(int node_index);
	void add_forces_from_subtree(int node_index, int target_index, const Vec3 &target_position,
								 Vec3 &acceleration, bool contains_target) const;
	void add_forces_from_leaf_particles(int first_particle_index, int particle_end_index,
										int target_particle_index, const Vec3 &target_position,
										Vec3 &acceleration) const;

	void add_approximate_force(Vec3 &acceleration, const OctreeNode &node,
							   const Vec3 &target_position) const;

	static int child_index_containing(const Vec3 &position, const Vec3 &center);

	std::vector<OctreeNode> nodes_;
	// Particle indices grouped by tree node; build() reorders this array.
	std::vector<int> particle_order_;
	std::vector<int> partition_scratch_;

	const std::vector<Particle> *particles_ = nullptr;
	double softening_squared_;
	double opening_angle_;
	double inverse_opening_angle_;
	int max_particles_per_leaf_;

	mutable long long interaction_count_ = 0;
};
