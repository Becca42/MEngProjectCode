// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WheeledVehicle.h"
#include "SimulationData.h"
#include "Landmark.h"
#include "TestRunData.h"
#include "CopyVehicleData.h"
#include "VehicleAdv3Pawn.generated.h"

/************************************************************************/
/*							 New Code                                   */
#define HORIZON 5
#define NUM_TEST_CARS 4
#define DEFAULT_THROTTLE 0.5F
#define DEFAULT_STEER 0.F

UENUM(Meta = (Bitflags))
enum class ECarType
{
	ECT_prediction,
	ECT_actual,
	ECT_test
};

// TODO update/upgrade this as needed as ErrorTriage evolves
struct SDiagnostics 
{
	bool bTryThrottle;
	bool bTrySteer;
	int nDrift;
	int nSpeedDiff;

	/** reset fields of SDiagnostics object
	* @param SDiagnostics struct to be reset <-- TODO pass by pointer or reference or value? */
	void Reset() 
	{
		bTryThrottle = false;
		bTrySteer = false;
		nDrift = 0;
		nSpeedDiff = 0;
	};
};

struct SCostComponents
{
	FVector location;
	FQuat rotation;
	float rpm;
};

/************************************************************************/

class UPhysicalMaterial;
class UCameraComponent;
class USpringArmComponent;
class UTextRenderComponent;
class UInputComponent;
class UAudioComponent;

UCLASS(config=Game)
class AVehicleAdv3Pawn : public AWheeledVehicle
{
	GENERATED_BODY()

	/** Spring arm that will offset the camera */
	UPROPERTY(Category = Camera, VisibleDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* SpringArm;

	/** Camera component that will be our viewpoint */
	UPROPERTY(Category = Camera, VisibleDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	UCameraComponent* Camera;

	/** SCene component for the In-Car view origin */
	UPROPERTY(Category = Camera, VisibleDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	class USceneComponent* InternalCameraBase;

	/** Camera component for the In-Car view */
	UPROPERTY(Category = Camera, VisibleDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	UCameraComponent* InternalCamera;

	/** Text component for the In-Car speed */
	UPROPERTY(Category = Display, VisibleDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	UTextRenderComponent* InCarSpeed;

	/** Text component for the In-Car gear */
	UPROPERTY(Category = Display, VisibleDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	UTextRenderComponent* InCarGear;

	/** Audio component for the engine sound */
	UPROPERTY(Category = Display, VisibleDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	UAudioComponent* EngineSoundComponent;

	/************************************************************************/
	/*                         New Code                                     */
	UPROPERTY(EditAnywhere)
	AVehicleAdv3Pawn* StoredCopy;
	UPROPERTY(EditAnywhere)
	AController* StoredController;
	FVector ResetVelocityLinear;
	FVector ResetVelocityAngular;
	float ResetRPM;

	/** information about expected path up to some horizon */
	UPROPERTY(EditAnywhere)
	USimulationData* expectedFuture;

	/** Handler for pause timer (horizon) */
	FTimerHandle PauseVehicleTimerHandle;

	/** effectively horizon for simulations triggered by "P" key-press */
	int pauseTimer = 10; // TODO eventually remove and just use 'horizon'

	bool currentlyPaused;

	/** Handler for horizon */
	FTimerHandle HorizonTimerHandle;

	/** effectively horizon for simulations triggered by "P" key-press */
	int horizon = HORIZON;

	bool horizonCountdown;
	
	/** Handler for model/expected trajectory generation timer */
	FTimerHandle GenerateExpectedTimerHandle;

	/** Handler for model/expected trajectory generation timer */
	FTimerHandle CompareLocationTimerHandle;

	/** Tick fire for expected path simulation */
	void HorizonTimer();
	/************************************************************************/

public:
	AVehicleAdv3Pawn();

	/** The current speed as a string eg 10 km/h */
	UPROPERTY(Category = Display, VisibleDefaultsOnly, BlueprintReadOnly)
	FText SpeedDisplayString;

	/** The current gear as a string (R,N, 1,2 etc) */
	UPROPERTY(Category = Display, VisibleDefaultsOnly, BlueprintReadOnly)
	FText GearDisplayString;

	UPROPERTY(Category = Display, VisibleDefaultsOnly, BlueprintReadOnly)
	/** The color of the incar gear text in forward gears */
	FColor	GearDisplayColor;

	/** The color of the incar gear text when in reverse */
	UPROPERTY(Category = Display, VisibleDefaultsOnly, BlueprintReadOnly)
	FColor	GearDisplayReverseColor;

	/** Are we using incar camera */
	UPROPERTY(Category = Camera, VisibleDefaultsOnly, BlueprintReadOnly)
	bool bInCarCameraActive;

	/** Are we in reverse gear */
	UPROPERTY(Category = Camera, VisibleDefaultsOnly, BlueprintReadOnly)
	bool bInReverseGear;

	/** Initial offset of incar camera */
	FVector InternalCameraOrigin;

	// Begin Pawn interface
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;
	// End Pawn interface

	// Begin Actor interface
	virtual void Tick(float Delta) override;
protected:
	virtual void BeginPlay() override;

public:
	// End Actor interface

	/** Handle pressing forwards */
	void MoveForward(float Val);

	/** Setup the strings used on the hud */
	void SetupInCarHUD();

	/** Update the physics material used by the vehicle mesh */
	void UpdatePhysicsMaterial();

	/** Handle pressing right */
	void MoveRight(float Val);
	/** Handle handbrake pressed */
	void OnHandbrakePressed();
	/** Handle handbrake released */
	void OnHandbrakeReleased();
	/** Switch between cameras */
	void OnToggleCamera();
	/** Handle reset VR device */
	void OnResetVR();

	/************************************************************************/
	/*                         New Code                                     */

	UPROPERTY(EditAnywhere, Meta = (Bitmask))
	ECarType vehicleType;

	bool bModelready = false;
	bool bGenExpected = false;
	float throttleInput;
	float steerInput;
	float throttleAdjust;
	float steerAdjust;

	//NTODO: investigate realloc
	/** Store expected variables (generated by simulation) */
	TArray<FTransform> PathLocations;
	TArray<FVector> VelocityAlongPath;
	TArray<float> RPMAlongPath;
	TMap<int32, TArray<ALandmark*>> LandmarksAlongPath;

	/* data for spawning vehicles */
	bool bRunDiagnosticTests;
	
	UPROPERTY(EditAnywhere)
	UCopyVehicleData* dataForSpawn;
	SDiagnostics errorDiagnosticResults;

	/* keep track of error testing results */
	UPROPERTY(EditAnywhere)
	UTestRunData* currentRun;
	float lowestCost;
	UPROPERTY(EditAnywhere)
	UTestRunData* bestRun;
	int runCount;

	const float GPS_ACCURACY = 4.9f; // https://www.gps.gov/systems/gps/performance/accuracy/

	/* Flags for error detection and identification */
	bool bRotationErrorFound = false;
	bool bLocationErrorFound = false;

	/** Array of objects seen by camera sweep for use in error identification */
	TArray<FHitResult > outputArray;

	/** for error triage */
	const int CAMERA = 0;
	const int RPM = 1;
	const int HEADING = 2;
	const int LOCATION = 3;

	const float LEFT = -1.f;
	const float RIGHT = 1.f;

	/** Used to iterate through PathLocation in tick */
	int AtTickLocation = 0;

	/** bool to trigger steering drift in tick() */
	bool bGenerateDrift = false;

	/** flag for applying low friction conditions */
	bool bLowFrictionOn = false;

	//NTODO:spooky ptrs?
	/** checks if seenLandmark is in expectedLandmarks
	@param expectedLandmark array of names of landmarks expected to see in this iteration
	@param seenLandmark name of a landmark seen this iteration
	@returns true if seenLandmark is in expectedLandmarks, false otherwise*/
	bool CheckLandmarkHit(TArray<ALandmark*>* expectedLandmarks, ALandmark* seenLandmark);

	/** determine whether to generate new expected trajectory or do diagnostic testing */
	void RunTestOrExpect();

	/** begin simulation for expected path <-- will be triggered by a timer? */
	void GenerateExpected();

	/** Resumes primary vehicles, saves information about expected path and destroys temp vehicle */
	void ResumeExpectedSimulation();

	/** TODO */
	void GenerateDiagnosticRuns();

	/** TODO */
	void ResumeFromDiagnostic();

	/** get info about severity of rotation error
	 * @param index in simulated data where error found
	 * @return TArray of floats representing (respectively) angular distance (in radians), veering (LEFT/RIGHT), dot product or nullptr if expectedfuture is null */
	TArray<float>* RotationErrorInfo(int index); // <== currently doesn't get called; either delete or call in ErrorTriage

	/**TODO*/
	TArray<int>* CameraErrorInfo(int index);

	/** determines which side of a line (def by two points) another point lies
	  * @param a endpoint of line
	  * @param b endpoint of line
	  * @param m point lying to left, right, (or on) line def by a and b
	  * @return -1 if m is to the left, 0 in on the line, 1 if on the right*/
	int GetSideOfLine(FVector a, FVector b, FVector m);

	/** best guess if car is too fast or slow
	  * TODO use rpm in addition to location?
	  * @return -2 if reversed, -1 if too slow, 0 if correct speed or inconclusive, 1 if too fast */
	int GetFastOrSlow(FVector start, FVector goal, FVector expected, FVector m);

	/** TODO: Called when a variable in the current state does not match what is expected in simulation
	 * @param index - tick where error was found 
	 TODO make sure this is called async?? */
	void ErrorTriage(int index, bool cameraError, bool headingError, bool rpmError, bool locationError);

	/** @return cost of x for target t: l = (t - x)^2 */
	float QuadraticLoss(SCostComponents x, SCostComponents t);

	/** regularization term to preference smallest difference in inputs
	  * @return regularization term for cost calculation  */
	float Regularize(float deltaThrottle, float deltaSteer);
	
	/** Induce error (callback for 'I' keypress) by increasing drag 10x */
	void InduceDragError();

	/** Revert increase in drag (back to original level) */
	void RevertDragError();

	/** Induce steering error by tweaking steering a few degrees */
	void InduceSteeringError();

	/** Revert steering to neutral */
	void StopInduceSteeringError();

	/** TODO: Change coefficient of friction (on ground, not wheel) */
	void InduceFrictionError();

	/************************************************************************/

	static const FName LookUpBinding;
	static const FName LookRightBinding;
	static const FName EngineAudioRPM;

private:
	/** 
	 * Activate In-Car camera. Enable camera and sets visibility of incar hud display
	 *
	 * @param	bState true will enable in car view and set visibility of various
	 */
	void EnableIncarView( const bool bState );

	/** Update the gear and speed strings */
	void UpdateHUDStrings();

	/* Are we on a 'slippery' surface */
	bool bIsLowFriction;
	/** Slippery Material instance */
	UPhysicalMaterial* SlipperyMaterial;
	/** Non Slippery Material instance */
	UPhysicalMaterial* NonSlipperyMaterial;
	/** Custom Slipper Material instance */
	UPhysicalMaterial* CustomSlipperyMaterial;


public:
	/** Returns SpringArm subobject **/
	FORCEINLINE USpringArmComponent* GetSpringArm() const { return SpringArm; }
	/** Returns Camera subobject **/
	FORCEINLINE UCameraComponent* GetCamera() const { return Camera; }
	/** Returns InternalCamera subobject **/
	FORCEINLINE UCameraComponent* GetInternalCamera() const { return InternalCamera; }
	/** Returns InCarSpeed subobject **/
	FORCEINLINE UTextRenderComponent* GetInCarSpeed() const { return InCarSpeed; }
	/** Returns InCarGear subobject **/
	FORCEINLINE UTextRenderComponent* GetInCarGear() const { return InCarGear; }
	/** Returns EngineSoundComponent subobject **/
	FORCEINLINE UAudioComponent* GetEngineSoundComponent() const { return EngineSoundComponent; }
};
