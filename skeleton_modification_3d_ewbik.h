/*************************************************************************/
/*  skeleton_modification_3d_ewbik.h                                     */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2019 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2019 Godot Engine contributors (cf. AUTHORS.md)    */
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

#ifndef SKELETON_MODIFICATION_3D_EWBIK_H
#define SKELETON_MODIFICATION_3D_EWBIK_H

#include "core/object/ref_counted.h"
#include "core/os/memory.h"
#include "ik_bone_chain.h"
#include "ik_effector_template.h"
#include "scene/resources/skeleton_modification_3d.h"

class SkeletonModification3DEWBIK : public SkeletonModification3D {
	GDCLASS(SkeletonModification3DEWBIK, SkeletonModification3D);

	Skeleton3D *skeleton = nullptr;
	String root_bone;
	BoneId root_bone_index = -1;
	Ref<IKBoneChain> segmented_skeleton;
	int32_t constraint_count = 0;
	PackedStringArray constraint_names;
	int32_t pin_count = 0;
	Vector<Ref<IKEffectorTemplate>> pins;
	Vector<Ref<IKBone3D>> bone_list;
	bool is_dirty = true;
	bool debug_skeleton = false;
	PackedInt32Array kusudama_limit_cone_count;
	PackedFloat32Array kusudana_twist;
	Vector<PackedColorArray> kusudama_limit_cones;
	float MAX_KUSUDAMA_LIMIT_CONES = 30;
	float time_budget_millisecond = 0.1f;
	int32_t ik_iterations = 10;
	int32_t max_ik_iterations = 30;
	float default_damp = Math::deg2rad(5.0f);
	Ref<IKTransform3D> root_transform = memnew(IKTransform3D);

	void update_shadow_bones_transform();
	void check_shadow_bones_transform();
	void update_skeleton_bones_transform(real_t p_blending_delta);
	void update_skeleton();
	Vector<Ref<IKEffectorTemplate>> get_bone_effectors() const;

	void set_dirty() {
		is_dirty = true;
	}

protected:
	virtual void _validate_property(PropertyInfo &property) const override;
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;
	static void _bind_methods();

public:
	float get_max_ik_iterations() const {
		return max_ik_iterations;
	}
	void set_max_ik_iterations(const float &p_max_ik_iterations) {
		max_ik_iterations = p_max_ik_iterations;
	}
	float get_time_budget_millisecond() const {
		return time_budget_millisecond;
	}
	void set_time_budget_millisecond(const float &p_time_budget) {
		time_budget_millisecond = p_time_budget;
	}
	virtual void _execute(real_t p_delta) override;
	virtual void _setup_modification(SkeletonModificationStack3D *p_stack) override;
	void add_pin(const String &p_name, const NodePath &p_target_node = NodePath(), const bool &p_use_node_rotation = true);
	void remove_pin(int32_t p_index);
	// Expose properties bound by script
	bool get_debug_skeleton() const;
	void set_debug_skeleton(bool p_enabled);
	void set_ik_iterations(int32_t p_iterations);
	int32_t get_ik_iterations() const;
	void set_root_bone(const String &p_root_bone);
	String get_root_bone() const;
	void set_root_bone_index(BoneId p_index);
	BoneId get_root_bone_index() const;
	void set_pin_count(int32_t p_value);
	int32_t get_pin_count() const;
	void set_pin_bone(int32_t p_pin_index, const String &p_bone);
	String get_pin_bone_name(int32_t p_effector_index) const;
	void set_pin_target_nodepath(int32_t p_effector_index, const NodePath &p_target_node);
	NodePath get_pin_target_nodepath(int32_t p_pin_index);
	void set_pin_depth_falloff(int32_t p_effector_index, const float p_depth_falloff);
	float get_pin_depth_falloff(int32_t p_effector_index) const;
	void set_pin_use_node_rotation(int32_t p_index, bool p_use_node_rot);
	bool get_pin_use_node_rotation(int32_t p_index) const;
	real_t get_default_damp() const;
	void set_default_damp(float p_default_damp);
	void set_constraint_count(int32_t p_count);
	int32_t get_constraint_count() const;
	void set_kusudama_twist(int32_t p_index, float p_twist);
	float get_kusudama_twist(int32_t p_index) const;
	void set_kusudama_limit_cone(int32_t p_bone, int32_t p_index,
			Vector3 p_center, float p_radius);
	Vector3 get_kusudama_limit_cone_center(int32_t p_bone, int32_t p_index) const;
	float get_kusudama_limit_cone_radius(int32_t p_bone, int32_t p_index) const;
	int32_t get_kusudama_limit_cone_count(int32_t p_bone) const;
	void set_kusudama_limit_cone_count(int32_t p_bone, int32_t p_count);
	SkeletonModification3DEWBIK();
	~SkeletonModification3DEWBIK();
};

#endif // SKELETON_MODIFICATION_3D_EWBIK_H
