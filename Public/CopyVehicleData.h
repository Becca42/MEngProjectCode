// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "CopyVehicleData.generated.h"

/**
 * 
 */
UCLASS()
class VEHICLEADV3_API UCopyVehicleData : public UObject
{
	GENERATED_BODY()

	FVector* linearVelocity;
	FVector* angularVelocity;
	FTransform* startPosition;

public:

	UCopyVehicleData();
	UCopyVehicleData(FVector* linear, FVector* angular);
	UCopyVehicleData(FVector* linear, FVector* angular, FTransform* start);



	/** @returns linearVelocity */
	FVector* GetLinearVelocity();

	/** @returns angularVelocity */
	FVector* GetAngularVelocity();

	/** @returns startPosition */
	FTransform* GetStartPosition();
	
};
