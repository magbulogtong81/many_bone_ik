@tool
extends Object

class_name IKKusudama

public class IKKusudama {

	public static final double TAU = Math.PI * 2;
	public static final double PI = Math.PI;
	protected IKNode3D limitingAxes;
	protected IKNode3D twistAxes;
	protected double painfullness = 0.9d;
	protected double cushionRatio = 0.1d;

	/**
	 * An array containing all of the Kusudama's limitCones. The kusudama is built
	 * up
	 * with the expectation that any limitCone in the array is connected to the cone
	 * at the previous element in the array,
	 * and the cone at the next element in the array.
	 */
	protected ArrayList<IKLimitCone> limitCones = new ArrayList<IKLimitCone>();

	/**
	 * Defined as some Angle in radians about the limitingAxes Y axis, 0 being
	 * equivalent to the
	 * limitingAxes Z axis.
	 */
	protected double minAxialAngle = Math.PI;
	/**
	 * Defined as some Angle in radians about the limitingAxes Y axis, 0 being
	 * equivalent to the
	 * minAxialAngle
	 */
	protected double range = Math.PI * 3;

	protected boolean orientationallyConstrained = false;
	protected boolean axiallyConstrained = false;

	// for IK solvers. Defines the weight ratio between the unconstrained IK solved
	// orientation and the constrained orientation for this bone
	// per iteration. This should help stabilize solutions somewhat by allowing for
	// soft constraint violations.
	protected Double strength = 1d;

	protected IKBone attachedTo;
	protected Quaternion twistMinRot = new Quaternion();
	protected Quaternion twistMaxRot = new Quaternion();
	protected Quaternion twistCenterRot = new Quaternion();

	public IKKusudama() {
	}

	public IKKusudama(IKBone forBone) {
		this.attachedTo = forBone;
		this.limitingAxes = forBone.getMajorRotationAxes();
		this.twistAxes = this.limitingAxes.attachedCopy(false);
		this.attachedTo.addConstraint(this);
		this.enable();
	}

	public void constraintUpdateNotification() {
		this.updateTangentRadii();
		this.updateRotationalFreedom();
	}

	/**
	 * This function should be called after you've set all of the Limiting Cones
	 * for this Kusudama. It will orient the twist axes relative to which the
	 * constrained rotations are computed
	 * so as to minimize the potential for undesirable twist rotations due to
	 * antipodal singularities.
	 * 
	 * In general, auto-optimization attempts to point the y-component of the twist
	 * constraint
	 * axes in the direction that places it within an oreintation allowed by the
	 * constraint,
	 * and roughly as far as possible from any orientations not allowed by the
	 * constraint.
	 */
	public void optimizeLimitingAxes() {
		IKNode3D originalLimitingAxes = twistAxes.getGlobalCopy();

		ArrayList<Vector3> directions = new ArrayList<>();
		if (getLimitCones().size() == 1) {
			directions.add((limitCones.get(0).getControlPoint()).copy());
		} else {
			for (int i = 0; i < getLimitCones().size() - 1; i++) {
				Vector3 thisC = getLimitCones().get(i).getControlPoint().copy();
				Vector3 nextC = getLimitCones().get(i + 1).getControlPoint().copy();
				Quaternion thisToNext = new Quaternion(thisC, nextC);
				Quaternion halfThisToNext = new Quaternion(thisToNext.getAxis(), thisToNext.getAngle() / 2d);

				Vector3 halfAngle = halfThisToNext.applyToCopy(thisC);
				halfAngle.normalize();
				halfAngle.mult(thisToNext.getAngle());
				directions.add(halfAngle);
			}
		}

		Vector3 newY = new Vector3();
		for (Vector3 dv : directions) {
			newY.add(dv);
		}

		newY.div(directions.size());
		if (newY.mag() != 0 && !Double.isNaN(newY.y)) {
			newY.normalize();
		} else {
			newY = new Vector3(0, 1d, 0);
		}

		IKRay3D newYRay = new IKRay3D(new Vector3(0, 0, 0), newY);

		Quaternion oldYtoNewY = new Quaternion(twistAxes.y_().heading(),
				originalLimitingAxes.getGlobalOf(newYRay).heading());
		twistAxes.rotateBy(oldYtoNewY);

		for (IKLimitCone lc : getLimitCones()) {
			originalLimitingAxes.setToGlobalOf(lc.controlPoint, lc.controlPoint);
			twistAxes.setToLocalOf(lc.controlPoint, lc.controlPoint);
			lc.controlPoint.normalize();
		}

		this.updateTangentRadii();
	}

	public void updateTangentRadii() {
		for (int i = 0; i < limitCones.size(); i++) {
			IKLimitCone next = i < limitCones.size() - 1 ? limitCones.get(i + 1) : null;
			limitCones.get(i).updateTangentHandles(next);
		}
	}

	IKRay3D boneRay = new IKRay3D(new Vector3(), new Vector3());
	IKRay3D constrainedRay = new IKRay3D(new Vector3(), new Vector3());

	/**
	 * Snaps the bone this Kusudama is constraining to be within the Kusudama's
	 * orientational and axial limits.
	 */
	public void snapToLimits() {
		if (orientationallyConstrained) {
			setAxesToOrientationSnap(attachedTo().localAxes(), swingOrientationAxes());
		}
		if (axiallyConstrained) {
			snapToTwistLimits(attachedTo().localAxes(), twistOrientationAxes());
		}
	}

	/**
	 * Presumes the input axes are the bone's localAxes, and rotates
	 * them to satisfy the snap limits.
	 * 
	 * @param toSet
	 */
	public void setAxesToSnapped(IKNode3D toSet, IKNode3D limitingAxes, IKNode3D twistAxes) {
		if (limitingAxes != null) {
			if (orientationallyConstrained) {
				setAxesToOrientationSnap(toSet, limitingAxes);
			}
			if (axiallyConstrained) {
				snapToTwistLimits(toSet, twistAxes);
			}
		}
	}

	public double clamp(double value, double min, double max) {
		if (value < min)
			return min;
		if (value > max)
			return max;
		return value;
	}

	public void setAxesToReturnfulled(IKNode3D toSet, IKNode3D swingAxes, IKNode3D twistAxes,
			double cosHalfReturnfullness, double angleReturnfullness) {
		if (swingAxes != null && painfullness > 0d) {
			if (orientationallyConstrained) {
				Vector3 origin = toSet.origin_();
				Vector3 inPoint = toSet.y_().p2().copy();
				Vector3 pathPoint = pointOnPathSequence(inPoint, swingAxes);
				inPoint.sub(origin);
				pathPoint.sub(origin);
				Quaternion toClamp = new Quaternion(inPoint, pathPoint);
				toClamp.rotation.clampToQuadranceAngle(cosHalfReturnfullness);
				toSet.rotateBy(toClamp);
			}
			if (axiallyConstrained) {
				double angleToTwistMid = angleToTwistCenter(toSet, twistAxes);
				double clampedAngle = clamp(angleToTwistMid, -angleReturnfullness, angleReturnfullness);
				toSet.rotateAboutY(clampedAngle, false);
			}
		}
	}

	/**
	 * A value between between 0 and 1 dictating
	 * how much the bone to which this kusudama belongs
	 * prefers to be away from the edges of the kusudama
	 * if it can. This is useful for avoiding unnatural poses,
	 * as the kusudama will push bones back into their more
	 * "comfortable" regions. Leave this value at its default of
	 * -1 unless your empirical observations show you need it, as its computation
	 * isn't free.
	 * Setting this value to anything higher than 0.4 is probably overkill
	 * in most situations.
	 * 
	 * @param amt set to a value outside of 0-1 to disable
	 */
	public void setPainfullness(double amt) {
		painfullness = amt;
		if (attachedTo() != null && attachedTo().parentArmature != null) {
			attachedTo().parentArmature.updateShadowSkelRateInfo();
		}
	}

	/**
	 * @return A value between (ideally between 0 and 1) dictating
	 *         how much the bone to which this kusudama belongs
	 *         prefers to be away from the edges of the kusudama
	 *         if it can.
	 */
	public double getPainfulness() {
		return painfullness;
	}

	@Override
	public boolean isInLimits_(Vector3 globalPoint) {
		double[] inBounds = { 1d };
		Vector3 inLimits = this.pointInLimits(globalPoint, inBounds, IKLimitCone.BOUNDARY);
		return inBounds[0] > 0d;
	}

	/**
	 * Presumes the input axes are the bone's localAxes, and rotates
	 * them to satisfy the snap limits.
	 * 
	 * @param toSet
	 */
	public void setAxesToSoftOrientationSnap(IKNode3D toSet, IKNode3D limitingAxes, double cosHalfAngleDampen) {
		double[] inBounds = { 1d };
		/**
		 * Basic idea:
		 * We treat our hard and soft boundaries as if they were two seperate kusudamas.
		 * First we check if we have exceeded our soft boundaries, if so,
		 * we find the closest point on the soft boundary and the closest point on the
		 * same segment
		 * of the hard boundary.
		 * let d be our orientation between these two points, represented as a ratio
		 * with 0 being right on the soft boundary
		 * and 1 being right on the hard boundary.
		 * 
		 * On every kusudama call, we store the previous value of d.
		 * If the new d is greater than the old d, our result is the weighted average of
		 * these
		 * (with the weight determining the resistance of the boundary). This result is
		 * stored for reference by future calls.
		 * If the new d is less than the old d, we return the input orientation, and set
		 * the new d to this lower value for reference by future calls.
		 */
		limitingAxes.updateGlobal();
		boneRay.p1().set(limitingAxes.origin_());
		boneRay.p2().set(toSet.y_().p2());
		Vector3 bonetip = limitingAxes.getLocalOf(toSet.y_().p2());
		Vector3 inCushionLimits = this.pointInLimits(bonetip, inBounds, IKLimitCone.CUSHION);

		if (inBounds[0] == -1 && inCushionLimits != null) {
			constrainedRay.p1().set(boneRay.p1());
			constrainedRay.p2().set(limitingAxes.getGlobalOf(inCushionLimits));
			Quaternion rectifiedRot = new Quaternion(boneRay.heading(), constrainedRay.heading());
			toSet.rotateBy(rectifiedRot);
			toSet.updateGlobal();
		}
	}

	/**
	 * Presumes the input axes are the bone's localAxes, and rotates
	 * them to satisfy the snap limits.
	 * 
	 * @param toSet
	 */
	public void setAxesToOrientationSnap(IKNode3D toSet, IKNode3D limitingAxes) {
		double[] inBounds = { 1d };
		limitingAxes.updateGlobal();
		boneRay.p1().set(limitingAxes.origin_());
		boneRay.p2().set(toSet.y_().p2());
		Vector3 bonetip = limitingAxes.getLocalOf(toSet.y_().p2());
		Vector3 inLimits = this.pointInLimits(bonetip, inBounds, IKLimitCone.BOUNDARY);

		if (inBounds[0] == -1 && inLimits != null) {
			constrainedRay.p1().set(boneRay.p1());
			constrainedRay.p2().set(limitingAxes.getGlobalOf(inLimits));
			Quaternion rectifiedRot = new Quaternion(boneRay.heading(), constrainedRay.heading());
			toSet.rotateBy(rectifiedRot);
			toSet.updateGlobal();
		}
	}

	public boolean isInOrientationLimits(IKNode3D globalAxes, IKNode3D limitingAxes) {
		double[] inBounds = { 1d };
		Vector3 localizedPoint = limitingAxes.getLocalOf(globalAxes.y_().p2()).copy().normalize();
		if (limitCones.size() == 1) {
			return limitCones.get(0).determineIfInBounds(null, localizedPoint);
		} else {
			for (int i = 0; i < limitCones.size() - 1; i++) {
				if (limitCones.get(i).determineIfInBounds(limitCones.get(i + 1), localizedPoint))
					return true;
			}
			return false;
		}
	}

	protected Vector3 twistMinVec;
	protected Vector3 twistMaxVec;
	protected Vector3 twistTan;
	protected Vector3 twistCenterVec;
	protected double twistHalfRangeHalfCos;
	protected boolean flippedBounds = false;

	/**
	 * Kusudama constraints decompose the bone orientation into a swing component
	 * and a twist component.
	 * One way to visualize the swing and twist is by imagining that the bone has a
	 * canonical default transorm such that when it is in this default pose, it is
	 * pointing straight up along the y-axis.
	 * The swing twist decomposition corresponds to
	 * 1. first "twisting" the bone along that y-axis, then
	 * 2. "swinging" it away from the y-axis until it is in its desired
	 * non-canonical pose.
	 * Where LimitCones let you constrain how far you can "swing" away from that
	 * y-axis on step 2,
	 * The axialLimits define how much you can "twist" clockwis or counterclockwise
	 * along that y-axis on step 1.
	 * 
	 * 
	 * @param minAnlge some angle in radians about the major rotation frame's y-axis
	 *                 to serve as the first angle within the range that the bone is
	 *                 allowed to twist.
	 * @param inRange  some angle in radians added to the minAngle. if the bone's
	 *                 local Z goes maxAngle radians beyond the minAngle, it is
	 *                 considered past the limit.
	 *                 This value is always interpreted as being in the positive
	 *                 direction. For example, if this value is -PI/2, the entire
	 *                 range from minAngle to minAngle + 3PI/4 is
	 *                 considered valid.
	 */
	public void setAxialLimits(double minAngle, double inRange) {
		minAxialAngle = minAngle;
		range = inRange;
		Vector3 y_axis = new Vector3(0, 1, 0);
		twistMinRot = new Quaternion(y_axis, minAxialAngle);
		twistMinVec = twistMinRot.applyToCopy(new Vector3(0, 0, 1));
		twistHalfRangeHalfCos = Math.cos(inRange / 4); // for quadrance angle. We need half the range angle since
														// starting from the center, and half of that since quadrance
														// takes cos(angle/2)
		twistMaxVec = new Quaternion(y_axis, range).applyToCopy(twistMinVec);
		twistMaxRot = new Quaternion(y_axis, range);
		Quaternion halfRot = new Quaternion(y_axis, range / 2);
		twistCenterVec = halfRot.applyToCopy(twistMinVec);
		twistCenterRot = new Quaternion(new Vector3(0, 0, 1), twistCenterVec);
		Vector3 maxcross = twistMaxVec.crossCopy(y_axis);
		constraintUpdateNotification();
	}

	/**
	 * gets the current twist ratio of the bone this kusudama constrains
	 */
	public double getTwistRatio() {
		return getTwistRatio(this.attachedTo().localAxes());
	}

	/**
	 * gets the current twist ratio of the bone this kusudama constrains
	 */
	public double getTwistRatio(IKNode3D toGet) {
		return getTwistRatio(toGet, this.twistAxes);
	}

	/**
	 * sets the twist angle of the bone this kusudama constrains
	 * 
	 * @param ratio, value from 0-1, with 0 being equivalent to the minimum
	 *               boundary, and 1 the maximum boundary
	 */

	public void setTwist(double ratio) {
		this.setTwist(ratio, this.attachedTo.localAxes());

	}

	public void setTwist(double ratio, IKNode3D toSet) {
		Quaternion alignRot = twistAxes.getGlobalMBasis().inverseRotation.applyTo(toSet.getGlobalMBasis().rotation);
		Quaternion[] decomposition = alignRot.getSwingTwist(new Vector3(0, 1, 0));
		decomposition[1] = new Quaternion(Quaternion.slerp(ratio, twistMinRot.rotation, twistMaxRot.rotation));
		Quaternion recomposition = decomposition[0].applyTo(decomposition[1]);
		toSet.getParentAxes().getGlobalMBasis().inverseRotation
				.applyTo(twistAxes.getGlobalMBasis().rotation.applyTo(recomposition), toSet.localMBasis.rotation);
		toSet.markDirty();
	}

	public double getTwistRatio(IKNode3D toGet, IKNode3D twistAxes) {
		Quaternion alignRot = twistAxes.getGlobalMBasis().inverseRotation.applyTo(toGet.getGlobalMBasis().rotation);
		Quaternion[] decomposition = alignRot.getSwingTwist(new Vector3(0, 1, 0));
		Vector3 twistZ = decomposition[1].applyToCopy(new Vector3(0, 0, 1));
		Quaternion minToZ = new Quaternion(twistMinVec, twistZ);
		Quaternion minToCenter = new Quaternion(twistMinVec, twistCenterVec);
		double minToZAngle = minToZ.getAngle();
		double minToMaxAngle = range;
		double flipper = minToCenter.getAxis().dot(minToZ.getAxis()) < 0 ? -1 : 1;
		return (minToZAngle * flipper) / minToMaxAngle;
	}

	/**
	 * 
	 * @param toSet
	 * @param twistAxes
	 * @return radians of twist required to snap bone into twist limits (0 if bone
	 *         is already in twist limits)
	 */
	public double snapToTwistLimits(IKNode3D toSet, IKNode3D twistAxes) {
		if (!axiallyConstrained)
			return 0d;
		Quaternion globTwistCent = twistAxes.getGlobalMBasis().rotation.applyTo(twistCenterRot);// create a temporary
		// orientation representing
		// globalOf((0,0,1))
		// represents the middle of
		// the allowable twist range
		// in global space
		Quaternion alignRot = globTwistCent.applyInverseTo(toSet.getGlobalMBasis().rotation);
		Quaternion[] decomposition = alignRot.getSwingTwist(new Vector3(0, 1, 0)); // decompose the orientation to a
																						// swing and
		// twist away from globTwistCent's
		// global basis
		decomposition[1].rotation.clampToQuadranceAngle(twistHalfRangeHalfCos);
		Quaternion recomposition = decomposition[0].applyTo(decomposition[1]);
		toSet.getParentAxes().getGlobalMBasis().inverseRotation.applyTo(globTwistCent.applyTo(recomposition),
				toSet.localMBasis.rotation);
		toSet.localMBasis.refreshPrecomputed();
		toSet.markDirty();
		return 0;
	}

	public double angleToTwistCenter(IKNode3D toSet, IKNode3D twistAxes) {
		if (!axiallyConstrained)
			return 0d;
		Quaternion invRot = twistAxes.getGlobalMBasis().getInverseRotation();
		Quaternion alignRot = invRot.applyTo(toSet.getGlobalMBasis().rotation);
		Quaternion[] decomposition = alignRot.getSwingTwist(new Vector3(0, 1, 0));
		Vector3 twistedDir = decomposition[1].applyToCopy(new Vector3(0, 0, 1));
		Quaternion toMid = new Quaternion(twistedDir, twistCenterVec);
		return toMid.getAngle() * toMid.getAxis().y;
	}

	public boolean inTwistLimits(IKNode3D boneAxes, IKNode3D limitingAxes) {
		Quaternion invRot = limitingAxes.getGlobalMBasis().getInverseRotation();
		Quaternion alignRot = invRot.applyTo(boneAxes.getGlobalMBasis().rotation);
		Quaternion[] decomposition = alignRot.getSwingTwist(new Vector3(0, 1, 0));
		double angleDelta2 = decomposition[1].getAngle() * decomposition[1].getAxis().y * -1d;
		angleDelta2 = toTau(angleDelta2);
		double fromMinToAngleDelta = toTau(signedAngleDifference(angleDelta2, TAU - this.minAxialAngle()));
		if (fromMinToAngleDelta < TAU - range) {
			return false;
		} else {
			return true;
		}
	}

	public double signedAngleDifference(double minAngle, double base) {
		double d = Math.abs(minAngle - base) % TAU;
		double r = d > PI ? TAU - d : d;
		double sign = (minAngle - base >= 0 && minAngle - base <= PI)
				|| (minAngle - base <= -PI && minAngle - base >= -TAU) ? 1f : -1f;
		r *= sign;
		return r;
	}

	/**
	 * Given a point (in global coordinates), checks to see if a ray can be extended
	 * from the Kusudama's origin to that point, such that the ray in the Kusudama's
	 * reference frame is within the range allowed by the Kusudama's coneLimits.
	 * If such a ray exists, the original point is returned (the point is within the
	 * limits). If it cannot exist, the tip of the ray within the kusudama's limits
	 * that
	 * would require the least rotation to arrive at the input point is returned.
	 *
	 * @param inPoint      the point to test.
	 * @param inBounds     a number from -1 to 1 representing the point's distance
	 *                     from
	 *                     the boundary, 0 means the point is right on the boundary,
	 *                     1 means the point
	 *                     is within the boundary and on the path furthest from the
	 *                     boundary. any negative number means
	 *                     the point is outside of the boundary, but does not
	 *                     signify anything about how far from the boundary
	 *                     the point is.
	 * @param boundaryMode mode for handling boundaries
	 * @return the original point, if it's in limits, or the closest point which is
	 *         in limits.
	 */
	public Vector3 pointInLimits(Vector3 inPoint, double[] inBounds, int boundaryMode) {
		Vector3 point = inPoint.copy();
		point.normalize();

		inBounds[0] = -1;

		Vector3 closestCollisionPoint = null;
		double closestCos = -2d;

		if (limitCones.size() > 1 && this.orientationallyConstrained) {
			for (int i = 0; i < limitCones.size() - 1; i++) {
				Vector3 collisionPoint = inPoint.copy();
				collisionPoint.set(0, 0, 0);
				IKLimitCone nextCone = limitCones.get(i + 1);
				boolean inSegBounds = limitCones.get(i).inBoundsFromThisToNext(nextCone, point, collisionPoint);

				if (inSegBounds) {
					inBounds[0] = 1;
				} else {
					double thisCos = collisionPoint.dot(point);
					if (closestCollisionPoint == null || thisCos > closestCos) {
						closestCollisionPoint = collisionPoint.copy();
						closestCos = thisCos;
					}
				}
			}

			return inBounds[0] == -1 ? closestCollisionPoint : inPoint;
		} else if (orientationallyConstrained) {
			if (point.dot(limitCones.get(0).getControlPoint()) > limitCones.get(0).getRadiusCosine()) {
				inBounds[0] = 1;
				return inPoint;
			} else {
				Vector3 axis = limitCones.get(0).getControlPoint().crossCopy(point);
				Quaternion toLimit = new Quaternion(axis, limitCones.get(0).getRadius());
				return toLimit.applyToCopy(limitCones.get(0).getControlPoint());
			}
		} else {
			inBounds[0] = 1;
			return inPoint;
		}
	}

	public Vector3 pointOnPathSequence(Vector3 inPoint, IKNode3D limitingAxes) {
		double closestPointDot = 0d;
		Vec3d point = limitingAxes.getLocalOf(inPoint);
		point.normalize();
		Vec3d result = (Vec3d) point.copy();

		if (limitCones.size() == 1) {
			result.set(limitCones.get(0).controlPoint);
		} else {
			for (int i = 0; i < limitCones.size() - 1; i++) {
				IKLimitCone nextCone = limitCones.get(i + 1);
				Vector3 closestPathPoint = limitCones.get(i).getClosestPathPoint(nextCone, point);
				double closeDot = closestPathPoint.dot(point);
				if (closeDot > closestPointDot) {
					result.set(closestPathPoint);
					closestPointDot = closeDot;
				}
			}
		}

		return limitingAxes.getGlobalOf(result);
	}

	public IKBone attachedTo() {
		return this.attachedTo;
	}

	/**
	 * Add a LimitCone to the Kusudama.
	 * 
	 * @param newPoint where on the Kusudama to add the LimitCone (in Kusudama's
	 *                 local coordinate frame defined by its bone's
	 *                 majorRotationAxes))
	 * @param radius   the radius of the limitCone
	 * @param previous the LimitCone adjacent to this one (may be null if LimitCone
	 *                 is not supposed to be between two existing LimitCones)
	 * @param next     the other LimitCone adjacent to this one (may be null if
	 *                 LimitCone is not supposed to be between two existing
	 *                 LimitCones)
	 */
	public void addLimitCone(Vector3 newPoint, double radius, IKLimitCone previous, IKLimitCone next) {
		int insertAt = 0;

		if (next == null || limitCones.size() == 0) {
			addLimitConeAtIndex(-1, newPoint, radius);
		} else if (previous != null) {
			insertAt = limitCones.indexOf(previous) + 1;
		} else {
			insertAt = (int) MathUtils.max(0, limitCones.indexOf(next));
		}
		addLimitConeAtIndex(insertAt, newPoint, radius);
	}

	public void removeLimitCone(IKLimitCone limitCone) {
		this.limitCones.remove(limitCone);
		this.updateTangentRadii();
		this.updateRotationalFreedom();
	}

	/**
	 * Adds a LimitCone to the Kusudama. LimitCones are reach cones which can be
	 * arranged sequentially. The Kusudama will infer
	 * a smooth path leading from one LimitCone to the next.
	 * 
	 * Using a single LimitCone is functionally equivalent to a classic reachCone
	 * constraint.
	 * 
	 * @param insertAt the intended index for this LimitCone in the sequence of
	 *                 LimitCones from which the Kusudama will infer a path. @see
	 *                 IK.IKKusudama.limitCones limitCones array.
	 * @param newPoint where on the Kusudama to add the LimitCone (in Kusudama's
	 *                 local coordinate frame defined by its bone's
	 *                 majorRotationAxes))
	 * @param radius   the radius of the limitCone
	 */
	public void addLimitConeAtIndex(int insertAt, Vector3 newPoint, double radius) {
		IKLimitCone newCone = createLimitConeForIndex(insertAt, newPoint, radius);
		if (insertAt == -1) {
			limitCones.add(newCone);
		} else {
			limitCones.add(insertAt, newCone);
		}
		this.updateTangentRadii();
		this.updateRotationalFreedom();

	}

	public double toTau(double angle) {
		double result = angle;
		if (angle < 0) {
			result = (2 * Math.PI) + angle;
		}
		result = result % (Math.PI * 2);
		return result;
	}

	public double mod(double x, double y) {
		if (y != 0 && x != 0) {
			double result = x % y;
			if (result < 0) {
				result += y;
			}
			return result;
		} else {
			return 0;
		}
	}

	/**
	 * @return the limitingAxes of this Kusudama (these are just its parentBone's
	 *         majorRotationAxes)
	 */
	@Override
	public IKNode3D swingOrientationAxes() {
		return IKNode3D limitingAxes;
	}

	@Override
	public IKNode3D twistOrientationAxes() {
		return IKNode3D twistAxes;
	}

	/**
	 * 
	 * @return the lower bound on the axial constraint
	 */
	public double minAxialAngle() {
		return minAxialAngle;
	}

	public double maxAxialAngle() {
		return range;
	}

	/**
	 * the upper bound on the axial constraint in absolute terms
	 * 
	 * @return
	 */
	public double absoluteMaxAxialAngle() {
		// return mod((minAxialAngle + range),(Math.PI*2d));
		return signedAngleDifference(range + minAxialAngle, Math.PI * 2d);
	}

	public boolean isAxiallyConstrained() {
		return axiallyConstrained;
	}

	public boolean isOrientationallyConstrained() {
		return orientationallyConstrained;
	}

	public void disableOrientationalLimits() {
		this.orientationallyConstrained = false;
	}

	public void enableOrientationalLimits() {
		this.orientationallyConstrained = true;
	}

	public void toggleOrientationalLimits() {
		this.orientationallyConstrained = !this.orientationallyConstrained;
	}

	public void disableAxialLimits() {
		this.axiallyConstrained = false;
	}

	public void enableAxialLimits() {
		this.axiallyConstrained = true;
	}

	public void toggleAxialLimits() {
		axiallyConstrained = !axiallyConstrained;
	}

	public boolean isEnabled() {
		return axiallyConstrained || orientationallyConstrained;
	}

	public void disable() {
		this.axiallyConstrained = false;
		this.orientationallyConstrained = false;
	}

	public void enable() {
		this.axiallyConstrained = true;
		this.orientationallyConstrained = true;
	}

	double unitHyperArea = 2 * Math.pow(Math.PI, 2);
	double unitArea = 4 * Math.PI;

	/**
	 * This functionality is not yet fully implemented. It always returns an
	 * overly simplistic representation
	 * not in line with what is described below.
	 * 
	 * @return an (approximate) measure of the amount of rotational
	 *         freedom afforded by this kusudama, with 0 meaning no rotational
	 *         freedom, and 1 meaning total unconstrained freedom.
	 * 
	 *         This is approximately computed as a ratio between the orientations
	 *         the bone can be in
	 *         vs the orientations it cannot be in. Note that unfortunately this
	 *         function double counts
	 *         the orientations a bone can be in between two limit cones in a
	 *         sequence if those limit
	 *         cones intersect with a previous sequence.
	 */
	public double getRotationalFreedom() {

		// computation cached from updateRotationalFreedom
		// feel free to override that method if you want your own more correct result.
		// please contribute back a better solution if you write one.
		return rotationalFreedom;
	}

	double rotationalFreedom = 1d;

	protected void updateRotationalFreedom() {
		double axialConstrainedHyperArea = isAxiallyConstrained() ? (range / TAU) : 1d;
		// quick and dirty solution (should revisit);
		double totalLimitConeSurfaceAreaRatio = 0d;
		for (IKLimitCone l : limitCones) {
			totalLimitConeSurfaceAreaRatio += (l.getRadius() * 2d) / TAU;
		}
		rotationalFreedom = axialConstrainedHyperArea
				* (isOrientationallyConstrained() ? Math.min(totalLimitConeSurfaceAreaRatio, 1d) : 1d);
	}

	/**
	 * attaches the Kusudama to the BoneExample. If the
	 * kusudama has its own limitingAxes specified,
	 * replaces the bone's major rotation
	 * axes with the Kusudamas limitingAxes.
	 * 
	 * otherwise, this function will set the kusudama's
	 * limiting axes to the major rotation axes specified by the bone.
	 * 
	 * @param forBone the bone to which to attach this Kusudama.
	 */
	public void attachTo(IKBone forBone) {
		this.attachedTo = forBone;
		if (this.limitingAxes == null)
			this.limitingAxes = forBone.getMajorRotationAxes();
		else {
			forBone.setFrameofRotation(this.limitingAxes);
			this.limitingAxes = forBone.getMajorRotationAxes();
		}
		this.twistAxes = this.limitingAxes.attachedCopy(false);
	}

	/**
	 * for IK solvers. Defines the weight ratio between the unconstrained IK solved
	 * orientation and the constrained orientation for this bone
	 * per iteration. This should help stabilize solutions somewhat by allowing for
	 * soft constraint violations.
	 * 
	 * @param strength a value between 0 and 1. Any other value will be clamped to
	 *                 this range.
	 **/
	public void setStrength(double newStrength) {
		this.strength = Math.max(0d, Math.min(1d, newStrength));
	}

	/**
	 * for IK solvers. Defines the weight ratio between the unconstrained IK solved
	 * orientation and the constrained orientation for this bone
	 * per iteration. This should help stabilize solutions somewhat by allowing for
	 * soft constraint violations.
	 **/
	public double getStrength() {
		return this.strength;
	}

	public ArrayList<? extends IKLimitCone> getLimitCones() {
		return this.limitCones;
	}

	@Override
	public void notifyOfLoadCompletion() {
		if (twistAxes.getParentAxes() == null) {
			twistAxes.setParent(limitingAxes.getParentAxes());
		}
		this.constraintUpdateNotification();
		this.optimizeLimitingAxes();
		if (isAxiallyConstrained()) {
			this.setAxialLimits(this.minAxialAngle, this.range);
		}
	}
}
