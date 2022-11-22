/*************************************************************************/
/*  ik_kusudama.h                                                        */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2022 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2022 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#ifndef IK_KUSUDAMA_H
#define IK_KUSUDAMA_H

#include "core/io/resource.h"
#include "core/math/quaternion.h"
#include "core/object/ref_counted.h"
#include "core/variant/typed_array.h"
#include "scene/3d/node_3d.h"

#include "ik_bone_3d.h"
#include "ik_bone_segment.h"
#include "ik_kusudama.h"
#include "ik_limit_cone.h"
#include "ik_ray_3d.h"
#include "math/ik_node_3d.h"

class IKBone3D;
class IKLimitCone;
class IKKusudama : public Resource {
	GDCLASS(IKKusudama, Resource);

	static real_t _mod(real_t x, real_t y);

	/**
	 * An array containing all of the Kusudama's limit_cones. The kusudama is built up
	 * with the expectation that any limitCone in the array is connected to the cone at the previous element in the array,
	 * and the cone at the next element in the array.
	 */
	TypedArray<IKLimitCone> limit_cones;

	/**
	 * Defined as some Angle in radians about the limiting_axes Y axis, 0 being equivalent to the
	 * limiting_axes Z axis.
	 */
	real_t min_axial_angle = 0.0;
	/**
	 * Defined as some Angle in radians about the limiting_axes Y axis, 0 being equivalent to the
	 * min_axial_angle
	 */
	real_t range_angle = Math_TAU;

	bool orientationally_constrained = false;
	bool axially_constrained = false;

	/**
	 * Get the swing rotation and twist rotation for the specified axis. The twist rotation represents the rotation around the specified axis. The swing rotation represents the rotation of the specified
	 * axis itself, which is the rotation around an axis perpendicular to the specified axis. The swing and twist rotation can be
	 * used to reconstruct the original quaternion: this = swing * twist
	 *
	 * @param p_axis the X, Y, Z component of the normalized axis for which to get the swing and twist rotation
	 * @return twist represent the rotational twist
	 * @return swing represent the rotational swing
	 * @see <a href="http://www.euclideanspace.com/maths/geometry/rotations/for/decomposition">calculation</a>
	 */
	static void get_swing_twist(
			Quaternion p_rotation,
			Vector3 p_axis,
			Quaternion &r_swing,
			Quaternion &r_twist);
	double rotational_freedom = 1;

protected:
	static void _bind_methods();

public:
	virtual ~IKKusudama() {
	}

	IKKusudama() {}

	IKKusudama(Ref<IKNode3D> to_set, Ref<IKNode3D> bone_direction, Ref<IKNode3D> limiting_axes, real_t cos_half_angle_dampen);

	virtual void _update_constraint();

	virtual void update_tangent_radii();

	Ref<IKRay3D> bone_ray = Ref<IKRay3D>(memnew(IKRay3D()));
	Ref<IKRay3D> constrained_ray = Ref<IKRay3D>(memnew(IKRay3D()));
	double unit_hyper_area = 2 * Math::pow(Math_PI, 2);
	double unit_area = 4 * Math_PI;

public:
	/**
	 * Presumes the input axes are the bone's localAxes, and rotates
	 * them to satisfy the snap limits.
	 *
	 * @param to_set
	 */
	virtual void set_axes_to_orientation_snap(Ref<IKNode3D> bone_direction, Ref<IKNode3D> to_set, Ref<IKNode3D> limiting_axes, real_t p_dampen, real_t p_cos_half_angle_dampen);

	real_t signed_angle_difference(real_t min_angle, real_t p_super);
	/**
	 * Kusudama constraints decompose the bone orientation into a swing component, and a twist component.
	 * The "Swing" component is the final direction of the bone. The "Twist" component represents how much
	 * the bone is rotated about its own final direction. Where limit cones allow you to constrain the "Swing"
	 * component, this method lets you constrain the "twist" component.
	 *
	 * @param min_angle some angle in radians about the major rotation frame's y-axis to serve as the first angle within the range_angle that the bone is allowed to twist.
	 * @param in_range some angle in radians added to the min_angle. if the bone's local Z goes maxAngle radians beyond the min_angle, it is considered past the limit.
	 * This value is always interpreted as being in the positive direction. For example, if this value is -PI/2, the entire range_angle from min_angle to min_angle + 3PI/4 is
	 * considered valid.
	 */
	virtual void set_axial_limits(real_t min_angle, real_t in_range);

	/**
	 *
	 * @param to_set
	 * @param limiting_axes
	 * @return radians of the twist required to snap bone into twist limits (0 if bone is already in twist limits)
	 */
	virtual void set_snap_to_twist_limit(Ref<IKNode3D> bone_direction, Ref<IKNode3D> to_set, Ref<IKNode3D> limiting_axes, float p_dampening, float p_cos_half_dampen);

	real_t get_current_twist_rotation(Ref<IKBone3D> bone_attached_to);
	void set_current_twist_rotation(Ref<IKBone3D> bone_attached_to, real_t p_rotation);

	/**
	 * Given a point (in local coordinates), checks to see if a ray can be extended from the Kusudama's
	 * origin to that point, such that the ray in the Kusudama's reference frame is within the range_angle allowed by the Kusudama's
	 * coneLimits.
	 * If such a ray exists, the original point is returned (the point is within the limits).
	 * If it cannot exist, the tip of the ray within the kusudama's limits that would require the least rotation
	 * to arrive at the input point is returned.
	 * @param in_point the point to test.
	 * @param in_bounds should be an array with at least 2 elements. The first element will be set to  a number from -1 to 1 representing the point's distance from the boundary, 0 means the point is right on
	 * the boundary, 1 means the point is within the boundary and on the path furthest from the boundary. any negative number means
	 * the point is outside of the boundary, but does not signify anything about how far from the boundary the point is.
	 * The second element will be given a value corresponding to the limit cone whose bounds were exceeded. If the bounds were exceeded on a segment between two limit cones,
	 * this value will be set to a non-integer value between the two indices of the limitcone comprising the segment whose bounds were exceeded.
	 * @return the original point, if it's in limits, or the closest point which is in limits.
	 */
	Vector3 get_local_point_in_limits(Vector3 in_point, Vector<double> *in_bounds);

	Vector3 local_point_on_path_sequence(Vector3 in_point, Ref<IKNode3D> limiting_axes);

	/**
	 * Add a IKLimitCone to the Kusudama.
	 * @param new_point where on the Kusudama to add the LimitCone (in Kusudama's local coordinate frame defined by its bone's majorRotationAxes))
	 * @param radius the radius of the limitCone
	 */
	virtual void add_limit_cone(Vector3 new_point, double radius);
	virtual void remove_limit_cone(Ref<IKLimitCone> limitCone);

	/**
	 *
	 * @return the lower bound on the axial constraint
	 */
	virtual real_t get_min_axial_angle();
	virtual real_t get_range_angle();

	virtual bool is_axially_constrained();
	virtual bool is_orientationally_constrained();
	virtual void disable_orientational_limits();
	virtual void enable_orientational_limits();
	virtual void toggle_orientational_limits();
	virtual void disable_axial_limits();
	virtual void enable_axial_limits();
	virtual void toggle_axial_limits();
	bool is_enabled();
	void disable();
	void enable();

	/**
	 * TODO: This functionality is not yet fully implemented It always returns an overly simplistic representation
	 * not in line with what is described below.
	 *
	 * @return an (approximate) measure of the amount of rotational
	 * freedom afforded by this kusudama, with 0 meaning no rotational
	 * freedom, and 1 meaning total unconstrained freedom.
	 *
	 * This is approximately computed as a ratio between the orientations the bone can be in
	 * vs the orientations it cannot be in. Note that unfortunately this function double counts
	 * the orientations a bone can be in between two limit cones in a sequence if those limit
	 * cones intersect with a previous sequence.
	 */
	double get_rotational_freedom();
	virtual void update_rotational_freedom();
	virtual TypedArray<IKLimitCone> get_limit_cones() const;
	virtual void set_limit_cones(TypedArray<IKLimitCone> p_cones);
	static real_t _to_tau(real_t angle);
};

#endif // IK_KUSUDAMA_H
