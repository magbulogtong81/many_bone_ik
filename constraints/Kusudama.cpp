/*************************************************************************/
/*  test_ewbik.h                                                         */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2020 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2020 Godot Engine contributors (cf. AUTHORS.md).   */
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

#include "Kusudama.h"

IKKusudama::IKKusudama() {
}

IKKusudama::IKKusudama(Ref<IKBone3D> forBone) {
	this->attachedTo_Conflict = forBone;
	this->limitingAxes_Conflict = forBone->get_global_pose();
	this->attachedTo_Conflict->addConstraint(this);
	this->enable();
}

void IKKusudama::constraintUpdateNotification() {
	this->updateTangentRadii();
	this->updateRotationalFreedom();
}

void IKKusudama::optimizeLimitingAxes() {
	IKTransform3D originalLimitingAxes = limitingAxes_Conflict;
	Vector<Vector3> directions;
	if (getLimitCones().size() == 1) {
		directions.push_back(limitCones[0]->getControlPoint());
	} else {
		for (int i = 0; i < getLimitCones().size() - 1; i++) {
			Vector3 thisC = getLimitCones()[i]->getControlPoint();
			Vector3 nextC = getLimitCones()[i + 1]->getControlPoint();
			Basis thisToNext = Basis(thisC, nextC);
			Vector3 axis;
			real_t angle;
			thisToNext.get_axis_angle(axis, angle);
			Basis halfThisToNext(axis, angle / 2.0f);

			Vector3 halfAngle = halfThisToNext.xform(thisC);
			halfAngle.normalize();
			halfAngle *= thisToNext.get_euler();
			directions.push_back(halfAngle);
		}
	}

	Vector3 newY;
	for (Vector3 dv : directions) {
		newY += dv;
	}

	newY /= directions.size();
	if (newY.length() != 0.f && !Math::is_nan(newY.y)) {
		newY.normalize();
	} else {
		newY = Vector3(0.f, 1.f, 0.f);
	}

	Vector3 tempVar(0.f, 0.f, 0.f);
	Ref<Ray3D> newYRay = memnew(Ray3D(tempVar, newY));

	// TODO: fire 2022-05-13 FIX ME!!
	// Quaternion oldYtoNewY = Quaternion(limitingAxes_Conflict.get_transform().basis;, originalLimitingAxes->get_global_transform().xform(newYRay->heading()));
	// limitingAxes_Conflict.rotateBy(oldYtoNewY);

	for (auto lc : getLimitCones()) {
		originalLimitingAxes->setToGlobalOf(lc->controlPoint, lc->controlPoint);
		limitingAxes_Conflict.setToLocalOf(lc->controlPoint, lc->controlPoint);
		lc->controlPoint.normalize();
	}

	this->updateTangentRadii();
}

void IKKusudama::updateTangentRadii() {
	for (int i = 0; i < limitCones.size(); i++) {
		Ref<LimitCone> next = i < limitCones.size() - 1 ? limitCones[i + 1] : nullptr;
		limitCones.write[i]->updateTangentHandles(next);
	}
}

void IKKusudama::snapToLimits() {
	// System.out.println("snapping to limits");
	if (orientationallyConstrained) {
		setAxesToOrientationSnap(attachedTo()->get_ik_transform(), limitingAxes_Conflict, 0);
	}
	if (axiallyConstrained) {
		snapToTwistLimits(attachedTo()->get_ik_transform(), limitingAxes_Conflict);
	}
}

void IKKusudama::setAxesToSnapped(IKTransform3D *toSet, IKTransform3D *limitingAxes, double cosHalfAngleDampen) {
	if (limitingAxes != nullptr) {
		if (orientationallyConstrained) {
			setAxesToOrientationSnap(toSet, limitingAxes, cosHalfAngleDampen);
		}
		if (axiallyConstrained) {
			snapToTwistLimits(toSet, limitingAxes);
		}
	}
}

void IKKusudama::setAxesToReturnfulled(IKTransform3D *toSet, IKTransform3D *limitingAxes, double cosHalfReturnfullness, double angleReturnfullness) {
	if (limitingAxes != nullptr && painfullness > 0) {
		if (orientationallyConstrained) {
			Vector3 origin = toSet->get_transform().origin;
			// TODO: fire 2022-05-13 Questions!
			Vector3 inPoint = toSet->get_transform().basis[Vector3::AXIS_Y];
			Vector3 pathPoint = pointOnPathSequence(inPoint, limitingAxes);
			inPoint -= origin;
			pathPoint -= origin;
			Quaternion toClamp = Quaternion(inPoint, pathPoint);
			toClamp.rotation.clampToQuadranceAngle(cosHalfReturnfullness);
			toSet->rotateBy(toClamp);
		}
		if (axiallyConstrained) {
			double angleToTwistMid = angleToTwistCenter(toSet, limitingAxes);
			double clampedAngle = CLAMP(angleToTwistMid, -angleReturnfullness, angleReturnfullness);
			toSet->rotateAboutY(clampedAngle, false);
		}
	}
}

void IKKusudama::setPainfullness(double amt) {
	painfullness = amt;
	if (!(attachedTo().is_valid() && attachedTo()->parentArmature.is_valid())) {
		return;
	}
	Ref<IKBone3D> wb = this->attachedTo();
	if (wb.is_null()) {
		return;
	}
	wb->updateCosDampening();
}

double IKKusudama::getPainfullness() {
	return painfullness;
}

void IKKusudama::setAxesToSoftOrientationSnap(IKTransform3D *toSet, IKTransform3D *limitingAxes, double cosHalfAngleDampen) {
	Vector<double> inBounds = { 1 };
	/**
	 * Basic idea:
	 * We treat our hard and soft boundaries as if they were two seperate kusudamas.
	 * First we check if we have exceeded our soft boundaries, if so,
	 * we find the closest point on the soft boundary and the closest point on the same segment
	 * of the hard boundary.
	 * let d be our orientation between these two points, represented as a ratio with 0 being right on the soft boundary
	 * and 1 being right on the hard boundary.
	 *
	 * On every kusudama call, we store the previous value of d.
	 * If the new d is  greater than the old d, our result is the weighted average of these
	 * (with the weight determining the resistance of the boundary). This result is stored for reference by future calls.
	 * If the new d is less than the old d, we return the input orientation, and set the new d to this lower value for reference by future calls.
	 *
	 *
	 * Because we can expect rotations to be fairly small, we use nlerp instead of slerp for efficiency when averaging.
	 */
	boneRay->p1(limitingAxes->get_transform().origin);
	boneRay->p2(toSet->get_transform().basis[Vector3::AXIS_Y]);
	Vector3 bonetip = limitingAxes->get_transform().xform(toSet->get_transform().basis[Vector3::AXIS_Y]);
	Vector3 inCushionLimits = this->pointInLimits(bonetip, inBounds, CUSHION);

	if (inBounds[0] == -1 && inCushionLimits != Vector3(NAN, NAN, NAN)) {
		constrainedRay->p1(boneRay->p1());
		constrainedRay->p2(limitingAxes->getGlobalOf(inCushionLimits));
		Quaternion rectifiedRot = Quaternion(boneRay->heading(), constrainedRay->heading());
		toSet->rotateBy(rectifiedRot);
		toSet->updateGlobal();
	}
}

bool IKKusudama::isInOrientationLimits(IKTransform3D *globalAxes, IKTransform3D *limitingAxes) {
	Vector<double> inBounds = { 1 };
	Vector3 *localizedPoint = limitingAxes->get_transform().xform(globalAxes->y_().p2()).copy().normalize();
	if (limitCones.size() == 1) {
		return limitCones[0]->determineIfInBounds(nullptr, localizedPoint);
	}
	for (int i = 0; i < limitCones.size() - 1; i++) {
		if (limitCones[i]->determineIfInBounds(limitCones[i + 1], localizedPoint)) {
			return true;
		}
	}
	return false;
}

void IKKusudama::setAxialLimits(double minAngle, double inRange) {
	minAxialAngle_Conflict = minAngle;
	range = toTau(inRange);
	constraintUpdateNotification();
}

double IKKusudama::snapToTwistLimits(IKTransform3D *toSet, IKTransform3D *limitingAxes) {
	if (!axiallyConstrained) {
		return 0;
	}
	Basis invRot = limitingAxes->get_global_transform().basis.inverse();
	Basis alignRot = invRot * toSet->get_global_transform().basis;
	Vector3 tempVar(0, 1, 0);
	Vector<Rot *> decomposition = alignRot->getSwingTwist(&tempVar);
	double angleDelta2 = decomposition[1]->getAngle() * decomposition[1]->getAxis().y * -1;
	angleDelta2 = toTau(angleDelta2);
	double fromMinToAngleDelta = toTau(signedAngleDifference(angleDelta2, TAU - this->minAxialAngle()));

	if (fromMinToAngleDelta < TAU - range) {
		double distToMin = std::abs(signedAngleDifference(angleDelta2, TAU - this->minAxialAngle()));
		double distToMax = std::abs(signedAngleDifference(angleDelta2, TAU - (this->minAxialAngle() + range)));
		double turnDiff = 1;
		turnDiff *= limitingAxes->getGlobalChirality();
		if (distToMin < distToMax) {
			turnDiff = turnDiff * (fromMinToAngleDelta);
			toSet->rotateAboutY(turnDiff, true);
		} else {
			turnDiff = turnDiff * (range - (Math_TAU - fromMinToAngleDelta));
			toSet->rotateAboutY(turnDiff, true);
		}
		return turnDiff < 0 ? turnDiff * -1 : turnDiff;
	}
	return 0;
}

double IKKusudama::angleToTwistCenter(IKTransform3D *toSet, IKTransform3D *limitingAxes) {
	if (!axiallyConstrained) {
		return 0;
	}

	Quaternion alignRot = limitingAxes->get_global_transform().basis.inverse() * toSet->get_global_transform().basis;
	Vector3 tempVar(0, 1, 0);
	Vector<Rot *> decomposition = alignRot->getSwingTwist(&tempVar);
	double angleDelta2 = decomposition[1]->getAngle() * Basis(decomposition[1])[Vector3::AXIS_Y] * -1;
	angleDelta2 = toTau(angleDelta2);

	double distToMid = signedAngleDifference(angleDelta2, Math_TAU - (this->minAxialAngle() + (range / 2)));
	return distToMid;
}

bool IKKusudama::inTwistLimits(IKTransform3D *boneAxes, IKTransform3D *limitingAxes) {
	Rot *invRot = limitingAxes->getGlobalMBasis().getInverseRotation();
	Rot *alignRot = invRot->applyTo(boneAxes->getGlobalMBasis().rotation);
	Vector3 tempVar(0, 1, 0);
	Vector<Rot *> decomposition = alignRot->getSwingTwist(&tempVar);
	double angleDelta2 = decomposition[1]->getAngle() * decomposition[1]->getAxis().y * -1;
	angleDelta2 = toTau(angleDelta2);
	double fromMinToAngleDelta = toTau(signedAngleDifference(angleDelta2, Math_TAU - this->minAxialAngle()));

	if (fromMinToAngleDelta < TAU - range) {
		double distToMin = std::abs(signedAngleDifference(angleDelta2, Math_TAU - this->minAxialAngle()));
		double distToMax = std::abs(signedAngleDifference(angleDelta2, Math_TAU - (this->minAxialAngle() + range)));
		double turnDiff = 1;
		turnDiff *= limitingAxes->getGlobalChirality();
		if (distToMin < distToMax) {
			return false;
		} else {
			return false;
		}
	}
	return true;
}

double IKKusudama::signedAngleDifference(double minAngle, double p_super) {
	double d = Math::fmod(Math::abs(minAngle - p_super), Math_TAU);
	double r = d > Math_PI ? Math_TAU - d : d;

	double sign = (minAngle - p_super >= 0 && minAngle - p_super <= Math_PI) || (minAngle - p_super <= -Math_PI && minAngle - p_super >= -Math_TAU) ? 1.0f : -1.0f;
	r *= sign;
	return r;
}

Ref<IKBone3D> IKKusudama::attachedTo() {
	return this->attachedTo_Conflict;
}

void IKKusudama::addLimitCone(Vector3 newPoint, double radius, Ref<LimitCone> previous, Ref<LimitCone> next) {
	int insertAt = 0;

	if (next == nullptr || limitCones.empty()) {
		addLimitConeAtIndex(-1, newPoint, radius);
	} else if (previous != nullptr) {
		insertAt = VectorHelper::indexOf(limitCones, previous) + 1;
	} else {
		insertAt = static_cast<int>(MathUtils::max(0, VectorHelper::indexOf(limitCones, next)));
	}
	addLimitConeAtIndex(insertAt, newPoint, radius);
}

void IKKusudama::removeLimitCone(Ref<LimitCone> limitCone) {
	this->limitCones.remove(limitCone);
	this->updateTangentRadii();
	this->updateRotationalFreedom();
}

void IKKusudama::addLimitConeAtIndex(int insertAt, Vector3 newPoint, double radius) {
	Ref<LimitCone> newCone = createLimitConeForIndex(insertAt, newPoint, radius);
	if (insertAt == -1) {
		limitCones.push_back(newCone);
	} else {
		limitCones.push_back(insertAt, newCone);
	}
	this->updateTangentRadii();
	this->updateRotationalFreedom();
}

double IKKusudama::toTau(double angle) {
	double result = angle;
	if (angle < 0) {
		result = (2 * M_PI) + angle;
	}
	result = result % (M_PI * 2);
	return result;
}

double IKKusudama::mod(double x, double y) {
	if (y != 0 && x != 0) {
		double result = x % y;
		if (result < 0) {
			result += y;
		}
		return result;
	}
	return 0;
}

double IKKusudama::minAxialAngle() {
	return minAxialAngle_Conflict;
}

double IKKusudama::maxAxialAngle() {
	return range;
}

double IKKusudama::absoluteMaxAxialAngle() {
	return signedAngleDifference(range + minAxialAngle_Conflict, M_PI * 2);
}

bool IKKusudama::isAxiallyConstrained() {
	return axiallyConstrained;
}

bool IKKusudama::isOrientationallyConstrained() {
	return orientationallyConstrained;
}

void IKKusudama::disableOrientationalLimits() {
	this->orientationallyConstrained = false;
}

void IKTransform3D::enableOrientationalLimits() {
	this->orientationallyConstrained = true;
}

void IKKusudama::toggleOrientationalLimits() {
	this->orientationallyConstrained = !this->orientationallyConstrained;
}

void IKKusudama::disableAxialLimits() {
	this->axiallyConstrained = false;
}

void IKKusudama::enableAxialLimits() {
	this->axiallyConstrained = true;
}

void IKKusudama::toggleAxialLimits() {
	axiallyConstrained = !axiallyConstrained;
}

bool IKKusudama::isEnabled() {
	return axiallyConstrained || orientationallyConstrained;
}

void IKKusudama::disable() {
	this->axiallyConstrained = false;
	this->orientationallyConstrained = false;
}

void IKKusudama::enable() {
	this->axiallyConstrained = true;
	this->orientationallyConstrained = true;
}

double IKKusudama::getRotationalFreedom() {
	// computation cached from updateRotationalFreedom
	// feel free to override that method if you want your own more correct result.
	// please contribute back a better solution if you write one.
	return rotationalFreedom;
}

void IKKusudama::updateRotationalFreedom() {
	double axialConstrainedHyperArea = isAxiallyConstrained() ? (range / TAU) : 1;
	// quick and dirty solution (should revisit);
	double totalLimitConeSurfaceAreaRatio = 0;
	for (auto l : limitCones) {
		totalLimitConeSurfaceAreaRatio += (l->getRadius() * 2) / TAU;
	}
	rotationalFreedom = axialConstrainedHyperArea * (isOrientationallyConstrained() ? std::min(totalLimitConeSurfaceAreaRatio, 1) : 1);
}

void IKKusudama::attachTo(Ref<IKBone3D> forBone) {
	this->attachedTo_Conflict = forBone;
	if (this->limitingAxes_Conflict.is_null()) {
		this->limitingAxes_Conflict = forBone->getMajorRotationAxes();
	} else {
		forBone->setFrameofRotation(this->limitingAxes_Conflict);
		this->limitingAxes_Conflict = forBone->getMajorRotationAxes();
	}
}

void IKKusudama::setStrength(double newStrength) {
	this->strength = MAX(0, MIN(1, newStrength));
}

double IKKusudama::getStrength() const {
	return this->strength;
}

Vector<LimitCone> IKKusudama::getLimitCones() const {
	return this->limitCones;
}

bool IKKusudama::isInLimits_(Vector3 globalPoint) {
	Vector<double> inBounds = { 1 };
	this->pointInLimits(globalPoint, inBounds, LimitCone::BOUNDARY);
	return inBounds[0] > 0;
}
