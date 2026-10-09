// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ClimbTypes.generated.h"

/**
 *  How the climb displacement is produced.
 */
UENUM(BlueprintType)
enum class EClimbMoveMode : uint8
{
	/**
	 * Root Motion + Motion Warping (primary). The montage must carry root motion and a
	 * MotionWarping notify state using the same warp target name as the component.
	 * The engine bends the root motion so the character lands exactly on the ledge top.
	 */
	MotionWarping	UMETA(DisplayName = "Motion Warping (Root Motion)"),

	/**
	 * Code-driven vertical interpolation. Used automatically when no
	 * UMotionWarpingComponent is present, so the feature still works unconfigured.
	 */
	CurveDriven		UMETA(DisplayName = "Curve Driven (No Root Motion)")
};
