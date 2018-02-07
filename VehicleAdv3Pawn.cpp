// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "VehicleAdv3Pawn.h"
#include "VehicleAdv3WheelFront.h"
#include "VehicleAdv3WheelRear.h"
#include "VehicleAdv3Hud.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundCue.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "WheeledVehicleMovementComponent4W.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Engine.h"
#include "GameFramework/Controller.h"
#include "UObject/ConstructorHelpers.h"
#include "SimulationData.h"
#include "DrawDebugHelpers.h"
#include "CustomRamp.h"
#include "Kismet/GameplayStatics.h"
#include "Landmark.h"
#include "TestRunData.h"

// Needed for VR Headset
#if HMD_MODULE_INCLUDED
#include "IHeadMountedDisplay.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#endif // HMD_MODULE_INCLUDED

const FName AVehicleAdv3Pawn::LookUpBinding("LookUp");
const FName AVehicleAdv3Pawn::LookRightBinding("LookRight");
const FName AVehicleAdv3Pawn::EngineAudioRPM("RPM");

#define LOCTEXT_NAMESPACE "VehiclePawn"

AVehicleAdv3Pawn::AVehicleAdv3Pawn()
{
	// Car mesh
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> CarMesh(TEXT("/Game/VehicleAdv/Vehicle/Vehicle_SkelMesh.Vehicle_SkelMesh"));
	GetMesh()->SetSkeletalMesh(CarMesh.Object);
	
	static ConstructorHelpers::FClassFinder<UObject> AnimBPClass(TEXT("/Game/VehicleAdv/Vehicle/VehicleAnimationBlueprint"));
	GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	GetMesh()->SetAnimInstanceClass(AnimBPClass.Class);

	// Setup friction materials
	static ConstructorHelpers::FObjectFinder<UPhysicalMaterial> SlipperyMat(TEXT("/Game/VehicleAdv/PhysicsMaterials/Slippery.Slippery"));
	SlipperyMaterial = SlipperyMat.Object;
		
	static ConstructorHelpers::FObjectFinder<UPhysicalMaterial> NonSlipperyMat(TEXT("/Game/VehicleAdv/PhysicsMaterials/NonSlippery.NonSlippery"));
	NonSlipperyMaterial = NonSlipperyMat.Object;

	static ConstructorHelpers::FObjectFinder<UPhysicalMaterial> CustomSlipperyMat(TEXT("/Game/VehicleAdv/PhysicsMaterials/CustomSlippery.CustomSlippery"));
	CustomSlipperyMaterial = CustomSlipperyMat.Object;

	UWheeledVehicleMovementComponent4W* Vehicle4W = CastChecked<UWheeledVehicleMovementComponent4W>(GetVehicleMovement());

	check(Vehicle4W->WheelSetups.Num() == 4);

	// Wheels/Tyres
	// Setup the wheels
	Vehicle4W->WheelSetups[0].WheelClass = UVehicleAdv3WheelFront::StaticClass();
	Vehicle4W->WheelSetups[0].BoneName = FName("PhysWheel_FL");
	Vehicle4W->WheelSetups[0].AdditionalOffset = FVector(0.f, -8.f, 0.f);

	Vehicle4W->WheelSetups[1].WheelClass = UVehicleAdv3WheelFront::StaticClass();
	Vehicle4W->WheelSetups[1].BoneName = FName("PhysWheel_FR");
	Vehicle4W->WheelSetups[1].AdditionalOffset = FVector(0.f, 8.f, 0.f);

	Vehicle4W->WheelSetups[2].WheelClass = UVehicleAdv3WheelRear::StaticClass();
	Vehicle4W->WheelSetups[2].BoneName = FName("PhysWheel_BL");
	Vehicle4W->WheelSetups[2].AdditionalOffset = FVector(0.f, -8.f, 0.f);

	Vehicle4W->WheelSetups[3].WheelClass = UVehicleAdv3WheelRear::StaticClass();
	Vehicle4W->WheelSetups[3].BoneName = FName("PhysWheel_BR");
	Vehicle4W->WheelSetups[3].AdditionalOffset = FVector(0.f, 8.f, 0.f);

	// Adjust the tire loading
	Vehicle4W->MinNormalizedTireLoad = 0.0f;
	Vehicle4W->MinNormalizedTireLoadFiltered = 0.2f;
	Vehicle4W->MaxNormalizedTireLoad = 2.0f;
	Vehicle4W->MaxNormalizedTireLoadFiltered = 2.0f;

	// Engine 
	// Torque setup
	Vehicle4W->MaxEngineRPM = 5700.0f;
	Vehicle4W->EngineSetup.TorqueCurve.GetRichCurve()->Reset();
	Vehicle4W->EngineSetup.TorqueCurve.GetRichCurve()->AddKey(0.0f, 400.0f);
	Vehicle4W->EngineSetup.TorqueCurve.GetRichCurve()->AddKey(1890.0f, 500.0f);
	Vehicle4W->EngineSetup.TorqueCurve.GetRichCurve()->AddKey(5730.0f, 400.0f);
 
	// Adjust the steering 
	Vehicle4W->SteeringCurve.GetRichCurve()->Reset();
	Vehicle4W->SteeringCurve.GetRichCurve()->AddKey(0.0f, 1.0f);
	Vehicle4W->SteeringCurve.GetRichCurve()->AddKey(40.0f, 0.7f);
	Vehicle4W->SteeringCurve.GetRichCurve()->AddKey(120.0f, 0.6f);
			
 	// Transmission	
	// We want 4wd
	Vehicle4W->DifferentialSetup.DifferentialType = EVehicleDifferential4W::LimitedSlip_4W;
	
	// Drive the front wheels a little more than the rear
	Vehicle4W->DifferentialSetup.FrontRearSplit = 0.65;

	// Automatic gearbox
	Vehicle4W->TransmissionSetup.bUseGearAutoBox = true;
	Vehicle4W->TransmissionSetup.GearSwitchTime = 0.15f;
	Vehicle4W->TransmissionSetup.GearAutoBoxLatency = 1.0f;

	// Physics settings
	// Adjust the center of mass - the buggy is quite low
	UPrimitiveComponent* UpdatedPrimitive = Cast<UPrimitiveComponent>(Vehicle4W->UpdatedComponent);
	if (UpdatedPrimitive)
	{
		UpdatedPrimitive->BodyInstance.COMNudge = FVector(8.0f, 0.0f, 0.0f);
	}

	// Set the inertia scale. This controls how the mass of the vehicle is distributed.
	Vehicle4W->InertiaTensorScale = FVector(1.0f, 1.333f, 1.2f);

	// Create a spring arm component for our chase camera
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetRelativeLocation(FVector(0.0f, 0.0f, 34.0f));
	SpringArm->SetWorldRotation(FRotator(-20.0f, 0.0f, 0.0f));
	SpringArm->SetupAttachment(RootComponent);
	SpringArm->TargetArmLength = 125.0f;
	SpringArm->bEnableCameraLag = false;
	SpringArm->bEnableCameraRotationLag = false;
	SpringArm->bInheritPitch = true;
	SpringArm->bInheritYaw = true;
	SpringArm->bInheritRoll = true;

	// Create the chase camera component 
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("ChaseCamera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	Camera->SetRelativeLocation(FVector(-125.0, 0.0f, 0.0f));
	Camera->SetRelativeRotation(FRotator(10.0f, 0.0f, 0.0f));
	Camera->bUsePawnControlRotation = false;
	Camera->FieldOfView = 90.f;

	// Create In-Car camera component 
	InternalCameraOrigin = FVector(-34.0f, -10.0f, 50.0f);
	InternalCameraBase = CreateDefaultSubobject<USceneComponent>(TEXT("InternalCameraBase"));
	InternalCameraBase->SetRelativeLocation(InternalCameraOrigin);
	InternalCameraBase->SetupAttachment(GetMesh());

	InternalCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("InternalCamera"));
	InternalCamera->bUsePawnControlRotation = false;
	InternalCamera->FieldOfView = 90.f;
	InternalCamera->SetupAttachment(InternalCameraBase);

	// In car HUD
	// Create text render component for in car speed display
	InCarSpeed = CreateDefaultSubobject<UTextRenderComponent>(TEXT("IncarSpeed"));
	InCarSpeed->SetRelativeScale3D(FVector(0.1f, 0.1f, 0.1f));
	InCarSpeed->SetRelativeLocation(FVector(35.0f, -6.0f, 20.0f));
	InCarSpeed->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	InCarSpeed->SetupAttachment(GetMesh());

	// Create text render component for in car gear display
	InCarGear = CreateDefaultSubobject<UTextRenderComponent>(TEXT("IncarGear"));
	InCarGear->SetRelativeScale3D(FVector(0.1f, 0.1f, 0.1f));
	InCarGear->SetRelativeLocation(FVector(35.0f, 5.0f, 20.0f));
	InCarGear->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	InCarGear->SetupAttachment(GetMesh());
	
	// Setup the audio component and allocate it a sound cue
	static ConstructorHelpers::FObjectFinder<USoundCue> SoundCue(TEXT("/Game/VehicleAdv/Sound/Engine_Loop_Cue.Engine_Loop_Cue"));
	EngineSoundComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("EngineSound"));
	EngineSoundComponent->SetSound(SoundCue.Object);
	EngineSoundComponent->SetupAttachment(GetMesh());

	// Colors for the in-car gear display. One for normal one for reverse
	GearDisplayReverseColor = FColor(255, 0, 0, 255);
	GearDisplayColor = FColor(255, 255, 255, 255);

	bIsLowFriction = false;
	bInReverseGear = false;

	/************************************************************************/
	/*						New Code                                        */
	vehicleType = ECarType::ECT_prediction;
	/************************************************************************/
}

void AVehicleAdv3Pawn::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// set up gameplay key bindings
	check(PlayerInputComponent);

	PlayerInputComponent->BindAxis("MoveForward", this, &AVehicleAdv3Pawn::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AVehicleAdv3Pawn::MoveRight);
	PlayerInputComponent->BindAxis(LookUpBinding);
	PlayerInputComponent->BindAxis(LookRightBinding);

	PlayerInputComponent->BindAction("Handbrake", IE_Pressed, this, &AVehicleAdv3Pawn::OnHandbrakePressed);
	PlayerInputComponent->BindAction("Handbrake", IE_Released, this, &AVehicleAdv3Pawn::OnHandbrakeReleased);
	PlayerInputComponent->BindAction("SwitchCamera", IE_Pressed, this, &AVehicleAdv3Pawn::OnToggleCamera);

	PlayerInputComponent->BindAction("ResetVR", IE_Pressed, this, &AVehicleAdv3Pawn::OnResetVR); 

	/************************************************************************/
	/*                          New Code                                    */
	//PlayerInputComponent->BindAction("PauseSimulation", IE_Pressed, this, &AVehicleAdv3Pawn::PauseSimulation);

	PlayerInputComponent->BindAction("InduceError", IE_Pressed, this, &AVehicleAdv3Pawn::InduceDragError); //TODO change action on click
	/************************************************************************/
}

void AVehicleAdv3Pawn::MoveForward(float Val)
{
	GetVehicleMovementComponent()->SetThrottleInput(Val);

}

void AVehicleAdv3Pawn::MoveRight(float Val)
{
	GetVehicleMovementComponent()->SetSteeringInput(Val);
}

void AVehicleAdv3Pawn::OnHandbrakePressed()
{
	GetVehicleMovementComponent()->SetHandbrakeInput(true);
}

void AVehicleAdv3Pawn::OnHandbrakeReleased()
{
	GetVehicleMovementComponent()->SetHandbrakeInput(false);
}

void AVehicleAdv3Pawn::OnToggleCamera()
{
	EnableIncarView(!bInCarCameraActive);
}

void AVehicleAdv3Pawn::EnableIncarView(const bool bState)
{
	if (bState != bInCarCameraActive)
	{
		bInCarCameraActive = bState;
		
		if (bState == true)
		{
			OnResetVR();
			Camera->Deactivate();
			InternalCamera->Activate();
		}
		else
		{
			InternalCamera->Deactivate();
			Camera->Activate();
		}
		
		InCarSpeed->SetVisibility(bInCarCameraActive);
		InCarGear->SetVisibility(bInCarCameraActive);
	}
}

void AVehicleAdv3Pawn::Tick(float Delta)
{
	Super::Tick(Delta);

	/************************************************************************/
	/*							New Code                                    */
	// more car forward at a steady rate (for primary and simulation)
	GetVehicleMovementComponent()->SetThrottleInput(throttleInput); 

	// get current location
	FTransform currentTransform = this->GetTransform();
	FVector currentLocation = currentTransform.GetLocation();
	FQuat currentRotation = currentTransform.GetRotation();

	// periodic camera sweep
	bool bCameraErrorFound = false;
	if (AtTickLocation % 400 == 0)
	{
		// 'camera' simulation
		FHitResult output;
		FVector sweepStart = currentLocation + FVector(0, 0, 10.f);
		FVector sweepEnd = currentLocation + currentLocation.ForwardVector * 10.f;
		FCollisionObjectQueryParams params = FCollisionObjectQueryParams(ECC_GameTraceChannel1);	
		const FCollisionShape shape = FCollisionShape::MakeSphere(300.f); 
		FCollisionQueryParams CollisionParams;
		this->outputArray.Empty();
		if (GetWorld()->SweepMultiByObjectType(this->outputArray, sweepStart, sweepEnd, currentRotation, params, shape, CollisionParams))
		{
			TArray<ALandmark*> allLandmarksSeen;
			if (bModelready && expectedFuture->bIsReady)
			{
				bool bLandmarksAccessErrorFound = false;
				TArray<ALandmark*>* landmarks = new TArray<ALandmark*>();
				try 
				{ 
					landmarks = expectedFuture->GetLandmarksAtTick(int(AtTickLocation / 400)); 
				}
				catch (const std::exception&)
				{ 
					bLandmarksAccessErrorFound = true;
				}
				if (!bLandmarksAccessErrorFound)
				{
					// didn't see expected number of landmarks for this sweep
					if (outputArray.Num() != landmarks->Num())
					{
						bCameraErrorFound = true;
					}
					// loop over trace hits ('seen objects')
					for (int i = 0; i < outputArray.Num(); i++)
					{
						//TWeakObjectPtr<AActor> hit = outputArray[i].GetActor();
						ALandmark* lmhit = Cast<ALandmark>(outputArray[i].GetActor());
						if (lmhit->IsValidLowLevelFast())
						{
							//FName seenLandmarkName = hit->GetFName();
							allLandmarksSeen.Add(lmhit);

							// don't need to keep looking if camera error found; offload rest of work 
							if (vehicleType == ECarType::ECT_actual && expectedFuture->bIsReady && !bCameraErrorFound)
							{
								// check if this is the landmark we're supposed to see (if this is not a copy)
								bool hitexpected = CheckLandmarkHit(landmarks, lmhit);
								if (!hitexpected)
								{
									bCameraErrorFound = true;
								}
							}
						}
					}
				}
				}
			// store landmarks seen at index for this tick
			if (vehicleType == ECarType::ECT_prediction)
			{
				this->LandmarksAlongPath.Add(int(AtTickLocation / 400), allLandmarksSeen);
			}
		}
		// add empty array at tick location
		else if (vehicleType == ECarType::ECT_prediction)
		{
			TArray<ALandmark*>* em = new TArray<ALandmark*>();
			this->LandmarksAlongPath.Add(int(AtTickLocation / 400), *em);
		}
	}
	bool bRpmErrorFound = false;
	bool bRotationErrorFound = false;
	bool bLocationErrorFound = false;
	if (vehicleType == ECarType::ECT_actual)
	{
		if (bGenerateDrift)
		{
			GetVehicleMovementComponent()->SetSteeringInput(0.05f); // generate slight drift right
		}
		if (bModelready && expectedFuture->bIsReady)
		{
			TArray<FTransform>* expected = expectedFuture->GetPath(); 
			TArray<float>* expectedRPM = expectedFuture->GetRMPValues();
			if (AtTickLocation < expectedRPM->Num())
			{
				FTransform expectedTransform = expected->operator[](AtTickLocation);
				FVector expectedLocation = expectedTransform.GetLocation();

				// compare location & check for error (accounting for 4m margin of error on gps irl)
				float distance = expectedLocation.Dist2D(expectedLocation, currentLocation);
				if (distance > 800.f && !bLocationErrorFound) // units are in cm, so error is distance > 8m
				{
					bLocationErrorFound = true;
					GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::FColor(255, 25, 0), FString::Printf(TEXT("Location Error Detected.")));
				}
				// compare rotation and check for error
				float rotationDist = currentRotation.AngularDistance(expectedTransform.GetRotation());
				if (rotationDist > 0.2f && !bRotationErrorFound)
				{
					bRotationErrorFound = true;
					GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, FString::Printf(TEXT("Rotation Error Detected.")));
				}
				// TODO determine if 1.0 is a good threshold -- TODO figure out why begin with very different values
				UE_LOG(LogTemp, Warning, TEXT("Expected %.1f"), expectedRPM->operator[](AtTickLocation));
				UE_LOG(LogTemp, Warning, TEXT("Current %.1f"), this->GetVehicleMovement()->GetEngineRotationSpeed());
				// (NOTE: real like speedometers word by measuring each tire, so when spinning on slippery ground, speed only goes up if all tires are spinning)
				if (!bRpmErrorFound && FGenericPlatformMath::Abs(expectedRPM->operator[](AtTickLocation) - this->GetVehicleMovement()->GetEngineRotationSpeed()) > 90.f) // error threshold set empirically 
				{
					GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, FString::Printf(TEXT("RPM Error Detected.")));
					bRpmErrorFound = true;
				}
				if (bCameraErrorFound || bRotationErrorFound || bRpmErrorFound)
				{
					// TODO call asynchronously so as not to hold up tick?
					ErrorTriage(AtTickLocation, bCameraErrorFound, bRotationErrorFound, bRpmErrorFound, bLocationErrorFound);
				}
				AtTickLocation++;

				// TODO only finds error on first iteration...is that desirable?
			}
		}
	}

	// save path data for copies
	if (vehicleType == ECarType::ECT_prediction)
	{
		PathLocations.Add(this->GetTransform()); 
		VelocityAlongPath.Add(this->GetVelocity());
		RPMAlongPath.Add(GetVehicleMovement()->GetEngineRotationSpeed());
	}
	/************************************************************************/

	// Setup the flag to say we are in reverse gear
	bInReverseGear = GetVehicleMovement()->GetCurrentGear() < 0;
	
	// Update physics material
	UpdatePhysicsMaterial();

	// Update the strings used in the hud (incar and onscreen)
	UpdateHUDStrings();

	// Set the string in the incar hud
	SetupInCarHUD();

	bool bHMDActive = false;
#if HMD_MODULE_INCLUDED
	if ((GEngine->HMDDevice.IsValid() == true ) && ( (GEngine->HMDDevice->IsHeadTrackingAllowed() == true) || (GEngine->IsStereoscopic3D() == true)))
	{
		bHMDActive = true;
	}
#endif // HMD_MODULE_INCLUDED
	if( bHMDActive == false )
	{
		if ( (InputComponent) && (bInCarCameraActive == true ))
		{
			FRotator HeadRotation = InternalCamera->RelativeRotation;
			HeadRotation.Pitch += InputComponent->GetAxisValue(LookUpBinding);
			HeadRotation.Yaw += InputComponent->GetAxisValue(LookRightBinding);
			InternalCamera->RelativeRotation = HeadRotation;
		}
	}	

	// Pass the engine RPM to the sound component
	float RPMToAudioScale = 2500.0f / GetVehicleMovement()->GetEngineMaxRotationSpeed();
	EngineSoundComponent->SetFloatParameter(EngineAudioRPM, GetVehicleMovement()->GetEngineRotationSpeed()*RPMToAudioScale);
}

void AVehicleAdv3Pawn::BeginPlay()
{
	Super::BeginPlay();

	bool bWantInCar = false;
	// First disable both speed/gear displays 
	bInCarCameraActive = false;
	InCarSpeed->SetVisibility(bInCarCameraActive);
	InCarGear->SetVisibility(bInCarCameraActive);

	// Enable in car view if HMD is attached
#if HMD_MODULE_INCLUDED
	bWantInCar = UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled();
#endif // HMD_MODULE_INCLUDED

	EnableIncarView(bWantInCar);
	// Start an engine sound playing
	EngineSoundComponent->Play();

	/************************************************************************/
	/*                       New Code                                       */

	horizonCountdown = false;
	throttleInput = DEFAULT_THROTTLE;


	// timer for horizon (stops simulation after horizon reached)
	GetWorldTimerManager().SetTimer(HorizonTimerHandle, this, &AVehicleAdv3Pawn::HorizonTimer, 1.0f, true, 0.f);

	// timer for model generation, generates a new model up to HORIZON every HORIZON seconds -- TODO maybe do at different intervals -- not exactly working, does for HORIZON then immediately spawns another
	if (vehicleType == ECarType::ECT_actual)
	{
		GetWorldTimerManager().SetTimer(GenerateExpectedTimerHandle, this, &AVehicleAdv3Pawn::GenerateExpected, float(HORIZON) * 2.f, true, 0.f);
		
	}
	/************************************************************************/
}

void AVehicleAdv3Pawn::OnResetVR()
{
#if HMD_MODULE_INCLUDED
	if (GEngine->HMDDevice.IsValid())
	{
		GEngine->HMDDevice->ResetOrientationAndPosition();
		InternalCamera->SetRelativeLocation(InternalCameraOrigin);
		GetController()->SetControlRotation(FRotator());
	}
#endif // HMD_MODULE_INCLUDED
}

/************************************************************************/
/*                       New Code                                       */

bool AVehicleAdv3Pawn::CheckLandmarkHit(TArray<ALandmark*>* expectedLandmarks, ALandmark* seenLandmark)
{
	if (expectedLandmarks->Contains(seenLandmark))
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::FColor(0, 255, 75), FString::Printf(TEXT("Landmark Seen!")));
		return true;
	}
	else
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::FColor(0, 255, 75), FString::Printf(TEXT("Landmark Miss!")));
		return false;
	}
}



void AVehicleAdv3Pawn::GenerateExpected()
{
	//GetWorldTimerManager().PauseTimer(GenerateExpectedTimerHandle); // TODO does this do anything? -- yes, but maybe something bad...

	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, TEXT("Generating Expected"));

	// begin horizon countdown
	bGenExpected = true;
	horizonCountdown = true;

	// remove old path data
	PathLocations.Empty();
	VelocityAlongPath.Empty();

	this->SetActorTickEnabled(false);

	//possessing new pawn: https://answers.unrealengine.com/questions/109205/c-pawn-possession.html
	AController* controller = this->GetController();
	this->StoredController = controller;
	FVector linearveloctiy = this->GetMesh()->GetPhysicsLinearVelocity();
	FVector angularvelocity = this->GetMesh()->GetPhysicsAngularVelocity();
	this->ResetVelocityLinear = linearveloctiy;
	this->ResetVelocityAngular = angularvelocity;

	UWheeledVehicleMovementComponent4W* Vehicle4W = CastChecked<UWheeledVehicleMovementComponent4W>(GetVehicleMovement()); //TODO do something with this...

	UWheeledVehicleMovementComponent* moveComp = GetVehicleMovement();
	float currRPM = moveComp->GetEngineRotationSpeed();
	// NOTE Modern automobile engines are typically operated around 2,000�3,000 rpm (33�50 Hz) when cruising, with a minimum (idle) speed around 750�900 rpm (12.5�15 Hz), and an upper limit anywhere from 4500 to 10,000 rpm (75�166 Hz) for a road car
	this->ResetRPM = currRPM; 

	// copy primary vehicle to make temp vehicle
	FActorSpawnParameters params = FActorSpawnParameters();
	// make sure copy will be flagged as copy
	this->vehicleType = ECarType::ECT_prediction;
	// don't copy over altered drag
	float curdrag = moveComp->DragCoefficient;
	RevertDragError();
	params.Template = this;
	AVehicleAdv3Pawn *copy = GetWorld()->SpawnActor<AVehicleAdv3Pawn>(this->GetClass(), params);
	//AVehicleAdv3Pawn *copy = GetWorld()->SpawnActor<AVehicleAdv3Pawn>(this->GetClass(), params);
	// reset primary state after copying
	this->vehicleType = ECarType::ECT_actual;
	moveComp->DragCoefficient = curdrag;
	moveComp->SetEngineRotationSpeed(currRPM);
	this->StoredCopy = copy;

	// get all ramps
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACustomRamp::StaticClass(), FoundActors);
	 //turn off collision for ramps (no collide with simulated vehicle)
	for (AActor* ramp : FoundActors)
	{
		//TODO
		copy->MoveIgnoreActorAdd(ramp); // doesn't seem to work
		if (UStaticMeshComponent* mesh = Cast<UStaticMeshComponent>(ramp->GetRootComponent()))
		{
			mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			//if (bLowFrictionOn)
			//{
			//	// no slipping for copy
			//	mesh->SetPhysMaterialOverride(NonSlipperyMaterial);
			//}
		}
	}

	this->GetMesh()->SetAllBodiesSimulatePhysics(false);

	// TODO copy over state to spawned vehicle**
	// get current speed (heading already copied with transform)
	copy->GetMesh()->SetPhysicsLinearVelocity(linearveloctiy);
	copy->GetMesh()->SetPhysicsAngularVelocity(angularvelocity);

	UWheeledVehicleMovementComponent* copyMoveComp = copy->GetVehicleMovement(); // TODO does thsi change the original?
	copyMoveComp->SetTargetGear(moveComp->GetCurrentGear(), true);

	//float debugSetRPM = copyMoveComp->SetEngineRotationSpeedByTire(moveComp->GetTireRotationSpeeds());
	copyMoveComp->SetEngineRotationSpeed(moveComp->GetEngineRotationSpeed());

	// switch controller to temp vehicle <= controller seems like it might auto-transfer... (hard to tell)
	controller->UnPossess(); // TODO doesn't seem to be unposessing when using param.template to clone
	controller->Possess(copy);
}

void AVehicleAdv3Pawn::ResumeExpectedSimulation()
{
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Orange, TEXT("RESUME FROM EXPECTED"));

	// get all ramps
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACustomRamp::StaticClass(), FoundActors);
	// change collision properties of ramps (no collide with simulated vehicle)
	for (AActor* ramp : FoundActors)
	{
		//copy->MoveIgnoreActorAdd(ramp); // doesn't seem to work
		if (UStaticMeshComponent* mesh = Cast<UStaticMeshComponent>(ramp->GetRootComponent()))
		{
			mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
				// apply low physics if error is induced & not copy
				//if (bLowFrictionOn)
				//{
				//	// TODO apply slippery mat to ramp pieces
				//	mesh->SetPhysMaterialOverride(CustomSlipperyMaterial);
				//}
		}
		//ramp->GetRootComponent()->SetSimulatePhysics(false); // doesn't even simulate physics >.<
	}

	// save results for model checking
	UWheeledVehicleMovementComponent* movecomp = this->StoredCopy->GetVehicleMovement();
	USimulationData* results = USimulationData::MAKE(this->StoredCopy->GetTransform(), movecomp->GetCurrentGear(), this->StoredCopy->PathLocations, this->StoredCopy->VelocityAlongPath, this->StoredCopy->RPMAlongPath, this->StoredCopy->LandmarksAlongPath);
	this->expectedFuture = results;
	bModelready = true;

	// clear timer
	bGenExpected = false;
	horizonCountdown = false;
	horizon = HORIZON;

	// resume primary vehicle
	this->SetActorTickEnabled(true);

	// reset player controller back to primary
	AController* controller = this->StoredController;
	// 	Unposses your current pawn(Controller->UnPossess())
	controller->UnPossess();
	// 	Re-posses main pawn
	controller->Possess(this);
	// restart original pawn
	this->GetMesh()->SetAllBodiesSimulatePhysics(true);
	this->GetMesh()->SetPhysicsLinearVelocity(this->ResetVelocityLinear);
	this->GetMesh()->SetAllPhysicsAngularVelocity(ResetVelocityAngular);
	this->GetVehicleMovement()->SetEngineRotationSpeed(this->ResetRPM);
	// destroy temp vehicle
	this->StoredCopy->Destroy(); // TODO look into this more

	// spawn sphere to show predicted final destination
	DrawDebugSphere(
		GetWorld(),
		results->GetTransform().GetLocation(),
		40.f,
		32,
		FColor(255, 0, 0),
		false,
		(HORIZON * 3.f)
	);

	// induce drag error
	/*if (this->GetVehicleMovementComponent()->DragCoefficient < 3000.f)
	{
		InduceDragError();
	}*/

	// change friction
	//InduceFrictionError();

	// reset for error detection
	AtTickLocation = 0;

	//InduceSteeringError();

	bLocationErrorFound = false;
	bRotationErrorFound = false;
}


void AVehicleAdv3Pawn::GenerateDiagnosticRuns(bool bTryThrottle, bool bTrySteer, bool bDriftLeft)
{
	FTransform start = PathLocations[0]; 
	// TODO get other initial stat conditions needed (e.g.rpm, whatever else)
	for (int i = 0; i < NUM_TEST_CARS; i++)
	{
		horizonCountdown = true;
		while (horizonCountdown)
		{
			// TODO spawn car <== TODO get some of the below outside the for-loop since it'll be the same for each car
			this->SetActorTickEnabled(false);

			AController* controller = this->GetController();
			this->StoredController = controller;
			// TODO don't want to give copy these values, want values we used earlier in generate expected
			FVector linearveloctiy = this->GetMesh()->GetPhysicsLinearVelocity();
			FVector angularvelocity = this->GetMesh()->GetPhysicsAngularVelocity();
			this->ResetVelocityLinear = linearveloctiy;
			this->ResetVelocityAngular = angularvelocity;
			float currRPM = GetVehicleMovement()->GetEngineRotationSpeed();
			this->ResetRPM = currRPM;
			FActorSpawnParameters params = FActorSpawnParameters();
			UWheeledVehicleMovementComponent* moveComp = GetVehicleMovement();
			float curdrag = moveComp->DragCoefficient;
			RevertDragError();
			params.Template = this;
			AVehicleAdv3Pawn *copy = GetWorld()->SpawnActor<AVehicleAdv3Pawn>(this->GetClass(), params);
			// reset primary state after copying <== TODO bother with this here or after spawning all copies? 
			moveComp->DragCoefficient = curdrag;
			moveComp->SetEngineRotationSpeed(currRPM);
			this->StoredCopy = copy;

			// TODO adjust throttle and steering
			float throttleAdjust = 0.f;
			float steerAdjust = 0.f;
			// store what change we're trying and in resume, what it's corresponding result is
			currentRun = new TestRunData(steerAdjust, throttleAdjust);
		}
	}
}

void AVehicleAdv3Pawn::ResumeFromDiagnostic()
{
	// TODO
	// reset timer
	horizonCountdown = false;
	horizon = HORIZON;

	// restore steering and throttle to defaults/expected
	throttleInput = DEFAULT_THROTTLE;

	// check results
	// TODO get location of test car
	// TODO get location of actual car
	// TODO compute distance
	/*float finalDistance = TODO;
	if (!closestRun)
	{
		closestRun = TODO;
	}*/
}

TArray<float>* AVehicleAdv3Pawn::RotationErrorInfo(int index)
{
	TArray<float>* results = new TArray<float>;

	TArray<FTransform>* expected = expectedFuture->GetPath();
	FTransform currentTransform = this->GetTransform();
	FQuat currentRotation = currentTransform.GetRotation();
	FTransform expectedTransform = expected->operator[](AtTickLocation);

	float angDist = currentRotation.AngularDistance(expectedTransform.GetRotation());
	results->Add(angDist);

	float currentForwardX = currentRotation.GetForwardVector().X; // X = -0.199136853 ; left X = 0.199502707
	float expectedForwardX = expectedTransform.GetRotation().GetForwardVector().X; // X = -0.000512242317 ; left X = -0.000133275986
	float veering = currentForwardX > expectedForwardX ? veering = LEFT : RIGHT; // TODO make sure this words (just like keep an eye on it)
	results->Add(veering);
	
	float dotprod = FVector::DotProduct(expectedTransform.GetRotation().GetForwardVector(), currentRotation.GetForwardVector());
	results->Add(dotprod);

	return results;
}

TArray<int>* AVehicleAdv3Pawn::CameraErrorInfo(int index)
{
	int missingRight = 0;
	int missingLeft = 0;
	int extraRight = 0;
	int extraLeft = 0;
	TArray<ALandmark*>* landmarks = &LandmarksAlongPath.operator[](index);
	for (int i = 0; i < outputArray.Num(); i++)
	{
		ALandmark* lmhit = Cast<ALandmark>(outputArray[i].GetActor());
		if (lmhit->IsValidLowLevelFast())
		{
			if (expectedFuture->bIsReady)
			{
				// seeing things we shouldn't
				if (!landmarks->Contains(lmhit))
				{
					(lmhit->IsOnLeft()) ? extraLeft++ : extraRight++;
				}
				else
				{
					// remove seen landmark so we can check what's missing at end
					landmarks->Remove(lmhit);
				}
			}
		}
	}
	for (int i = 0; i < landmarks->Num(); i++)
	{
		ALandmark* lm = landmarks->operator[](i);
		(lm->IsOnLeft()) ? missingLeft++ : missingRight++;
	}
	TArray<int>* results = new TArray<int>;
	results->Add(missingRight);
	results->Add(missingLeft);
	results->Add(extraRight);
	results->Add(extraLeft);
	return results;
}

void AVehicleAdv3Pawn::ErrorTriage(int index, bool cameraError, bool headingError, bool rpmError, bool locationError)
{
	/* TODO decide if likely throttle fixable or steering fixable
	 * maybe make a guess if self or environment?
	 * we have (1) landmarks (2) rpm (3) heading (4) rough positioning with "gps" */

	// going wrong way, seeing wrong things, at either wrong speed or spinning to hard
	if (cameraError && headingError && rpmError && !locationError)
	{
		// check how heading differs
		TArray<float>* rotationInfo = RotationErrorInfo(index);
		float angDist = rotationInfo->operator[](0);
		float veering = rotationInfo->operator[](1);
		float dotprod = rotationInfo->operator[](2);

		// check how rpm differs (too fast or slow)
		float rpmExpected = expectedFuture->GetRMPValues()->operator[](AtTickLocation);
		float rpmCurrent = this->GetVehicleMovement()->GetEngineRotationSpeed();
		float rpmDiff = rpmExpected - rpmCurrent;
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Black, FString::Printf(TEXT("Veering %b"), veering));
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Black, FString::Printf(TEXT("Dot Product steering diff %.1f"), dotprod));
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Black, FString::Printf(TEXT("rpm diff %.1f"), rpmDiff));

		// check seen object differences
		TArray<int>* camerainfo = CameraErrorInfo(index);
		// TODO do something with this information about 
		// if missing left and right, totally in wrong place, maybe too fast or too slow
		// if only/mostly missing left and/or additional right --> veering right (or later, not turning)
		// if only/mostly missing right and/or additional left --> verring left (or later, not turning)

	}
	// see wrong things but heading right way and in (very roughly) right place; probably drag or environment (slope or friction) issue, so throttle issue
	else if (cameraError && !headingError && !rpmError && !locationError)
	{
		TArray<int>* camerainfo = CameraErrorInfo(index);
	}

	// likely an error to do only with speed, could be drag, terrain, friction; would be throttle correct
	else if (!cameraError && !headingError && rpmError && !locationError)
	{
		// TODO check how rpm differs (too fast or slow)
		float rpmExpected = expectedFuture->GetRMPValues()->operator[](AtTickLocation);
		float rpmCurrent = this->GetVehicleMovement()->GetEngineRotationSpeed();
		float rpmDiff = rpmExpected - rpmCurrent;
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Black, FString::Printf(TEXT("rpm diff %.1f"), rpmDiff));
	}
	// heading wrong way but seeing right things, likely steering error (or friction issue), def steering correct
	else if (!cameraError && headingError && !rpmError && !locationError)
	{
		// check how heading differs
		TArray<float>* rotationInfo = RotationErrorInfo(index);
		float angDist = rotationInfo->operator[](0);
		float veering = rotationInfo->operator[](1);
		float dotprod = rotationInfo->operator[](2);
	}
}

//void AVehicleAdv3Pawn::IdentifyLocationErrorSource(float distance, int index, FTransform expectedTransform)
//{
//	FTransform currentTransform = this->GetTransform();
//	FVector expectedVelocity = this->expectedFuture->GetVelocities()[index];
//	FVector currentVelocity = this->GetVelocity();
//	float expectedSpeed = expectedVelocity.Size();
//	float currentSpeed = currentVelocity.Size();
//	//GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Black, FString::Printf(TEXT("Speed diff %.1f"), expectedSpeed - currentSpeed)	);
//	float threshold = 20.f;
//	if ((currentSpeed - expectedSpeed) > threshold)
//	{
//		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Black, TEXT("Too Fast"));
//	}
//	else if ((expectedSpeed - currentSpeed) > threshold)
//	{
//		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Black, TEXT("Too Slow"));
//	}
//	else
//	{
//		// TODO probably not a speed/throttle error; likely a steering/direction error
//		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Black, TEXT("Positional, non-speed error"));
//		// TODO maybe call IdentifyRotationErrorScource()?? if not already called
//	}
//
//	// TODO want to determine if this is a throttle issue
//	// first run through some heuristic checks - am i ahead of target or behind? left/right?
//	
//}

void AVehicleAdv3Pawn::IdentifyRotationErrorSource(float angularDistance, int index)
{
	FQuat currentRotation = this->GetTransform().GetRotation();
	TArray<FTransform> path = *this->expectedFuture->GetPath();
	FQuat expectedRotation = path[index].GetRotation();  // out of bounds error on path 
	float threshold = 10.f; // TODO calibrate threshold
	// drifting left
	FVector currentEuler = currentRotation.Euler();
	FVector expectedEuler = expectedRotation.Euler();
	float zDiff = expectedEuler.Z - currentEuler.Z;
	if (zDiff > threshold)
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Black, FString::Printf(TEXT("Z diff (pos) %.1f"), zDiff));
		//GEngine->AddOnScreenDebugMessage(-1, 100.f, FColor::Black, FString::Printf(TEXT("Heading diff %.1f, cur %s, exp %s"), angularDistance, *currentEuler.ToString(), *expectedEuler.ToString() ));
	}
	// drifting right
	else if (zDiff < -(threshold))
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Black, FString::Printf(TEXT("Z diff %.1f"), zDiff));
	}
}

void AVehicleAdv3Pawn::InduceDragError()
{
	UWheeledVehicleMovementComponent* moveComp = GetVehicleMovement();
	float dragCo = moveComp->DragCoefficient;
	//moveComp->InertiaTensorScale; // TODO fuck with this?
	//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Purple, FString::Printf(TEXT("Drag Coefficient: %.1f"), dragCo));
	moveComp->DragCoefficient = 10.0 * dragCo;
	//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Purple, FString::Printf(TEXT("Drag Coefficient New: %.1f"), GetVehicleMovement()->DragCoefficient));
}

void AVehicleAdv3Pawn::RevertDragError()
{
	UWheeledVehicleMovementComponent* moveComp = GetVehicleMovement();
	moveComp->DragCoefficient = 0.3f;
}

void AVehicleAdv3Pawn::InduceSteeringError()
{
	bGenerateDrift = true;
}

void AVehicleAdv3Pawn::StopInduceSteeringError()
{
	bGenerateDrift = false;

}

void AVehicleAdv3Pawn::InduceFrictionError()
{
	GetMesh()->SetPhysMaterialOverride(CustomSlipperyMaterial);
	
	bLowFrictionOn = true;
}

void AVehicleAdv3Pawn::HorizonTimer()
{
	if (horizonCountdown)
	{
		--horizon;
		if (horizon < 1)
		{
			bGenExpected ? ResumeExpectedSimulation() : ResumeFromDiagnostic();
		}
	}
}


/************************************************************************/

void AVehicleAdv3Pawn::UpdateHUDStrings()
{
	float KPH = FMath::Abs(GetVehicleMovement()->GetForwardSpeed()) * 0.036f;
	int32 KPH_int = FMath::FloorToInt(KPH);
	int32 Gear = GetVehicleMovement()->GetCurrentGear();

	// Using FText because this is display text that should be localizable
	SpeedDisplayString = FText::Format(LOCTEXT("SpeedFormat", "{0} km/h"), FText::AsNumber(KPH_int));


	if (bInReverseGear == true)
	{
		GearDisplayString = FText(LOCTEXT("ReverseGear", "R"));
	}
	else
	{
		GearDisplayString = (Gear == 0) ? LOCTEXT("N", "N") : FText::AsNumber(Gear);
	}

}

void AVehicleAdv3Pawn::SetupInCarHUD()
{
	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if ((PlayerController != nullptr) && (InCarSpeed != nullptr) && (InCarGear != nullptr))
	{
		// Setup the text render component strings
		InCarSpeed->SetText(SpeedDisplayString);
		InCarGear->SetText(GearDisplayString);
		
		if (bInReverseGear == false)
		{
			InCarGear->SetTextRenderColor(GearDisplayColor);
		}
		else
		{
			InCarGear->SetTextRenderColor(GearDisplayReverseColor);
		}
	}
}

void AVehicleAdv3Pawn::UpdatePhysicsMaterial()
{
	if (GetActorUpVector().Z < 0)
	{
		if (bIsLowFriction == true)
		{
			GetMesh()->SetPhysMaterialOverride(NonSlipperyMaterial);
			bIsLowFriction = false;
		}
		else
		{
			GetMesh()->SetPhysMaterialOverride(SlipperyMaterial);
			bIsLowFriction = true;
		}
	}
}

#undef LOCTEXT_NAMESPACE
