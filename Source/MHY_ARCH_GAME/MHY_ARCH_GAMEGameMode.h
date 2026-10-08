// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MHY_ARCH_GAMEGameMode.generated.h"

/**
 *  Simple GameMode for a third person game
 */
UCLASS(abstract)
class AMHY_ARCH_GAMEGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	
	/** Constructor */
	AMHY_ARCH_GAMEGameMode();
};



