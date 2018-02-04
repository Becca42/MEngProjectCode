// Fill out your copyright notice in the Description page of Project Settings.

#include "Landmark.h"


// Sets default values
ALandmark::ALandmark()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	LandmarkMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LandmarkMesh"));

	FAttachmentTransformRules FattRules(EAttachmentRule::KeepRelative, false);
	LandmarkMesh->AttachToComponent(RootComponent, FattRules);

}

// Called when the game starts or when spawned
void ALandmark::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ALandmark::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

bool ALandmark::IsOnLeft()
{
	return (this->RoadSide == ERoadSide::ERS_Left);
}

bool ALandmark::IsOnRight()
{
	return (this->RoadSide == ERoadSide::ERS_Right);
}
