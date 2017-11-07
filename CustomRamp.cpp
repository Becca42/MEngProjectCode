// Fill out your copyright notice in the Description page of Project Settings.

#include "CustomRamp.h"


// Sets default values
ACustomRamp::ACustomRamp()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	RampMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RampMesh"));

	FAttachmentTransformRules FattRules(EAttachmentRule::KeepRelative, false);
	RampMesh->AttachToComponent(RootComponent, FattRules);

}

// Called when the game starts or when spawned
void ACustomRamp::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ACustomRamp::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

