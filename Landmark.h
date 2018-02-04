// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Landmark.generated.h"


UENUM(Meta = (Bitflags))
enum class ERoadSide
{
	ERS_Left,
	ERS_Right
};

UCLASS()
class VEHICLEADV3_API ALandmark : public AActor
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Meta = (Bitmask))
		ERoadSide RoadSide;
	
public:	
	// Sets default values for this actor's properties
	ALandmark();

	UPROPERTY(VisibleAnywhere, Category = "mesh")
		UStaticMeshComponent* LandmarkMesh;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	/** returns true if this landmark is on the left side of the road */
	bool IsOnLeft();

	/** returns true if this landmark is on the right side of the road */
	bool IsOnRight();
	
};
